/*
 * sx126x_hal.c
 *
 * Использует SPI2 (hspi2), сгенерированный CubeMX (файл spi.h/spi.c).
 * SPI2 должен быть настроен как Full-Duplex Master, NSS = Software (Disable).
 */

#include "sx126x_hal.h"
#include "radio_pins.h"
#include "main.h"

/*
 * В этом проекте CubeMX сгенерировал SPI-инициализацию прямо внутри main.c
 * (нет отдельных файлов spi.c/spi.h), поэтому переменная hspi2 объявлена
 * там как глобальная. Подключаемся к ней через extern.
 */
extern SPI_HandleTypeDef hspi2;

static void CS_Low( void )
{
    HAL_GPIO_WritePin( RADIO_CS_PORT, RADIO_CS_PIN, GPIO_PIN_RESET );
}

static void CS_High( void )
{
    HAL_GPIO_WritePin( RADIO_CS_PORT, RADIO_CS_PIN, GPIO_PIN_SET );
}

void RadioPins_Init( void )
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* CS -- выход, изначально неактивен (High) */
    HAL_GPIO_WritePin( RADIO_CS_PORT, RADIO_CS_PIN, GPIO_PIN_SET );
    gpio.Pin   = RADIO_CS_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init( RADIO_CS_PORT, &gpio );

    /* NRESET -- выход, изначально неактивен (High = чип не в сбросе) */
    HAL_GPIO_WritePin( RADIO_NRST_PORT, RADIO_NRST_PIN, GPIO_PIN_SET );
    gpio.Pin   = RADIO_NRST_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init( RADIO_NRST_PORT, &gpio );

    /* BUSY -- вход */
    gpio.Pin  = RADIO_BUSY_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init( RADIO_BUSY_PORT, &gpio );

    /* IRQ (DIO1) -- вход, без прерывания в этом тесте (просто poll) */
    gpio.Pin  = RADIO_IRQ_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init( RADIO_IRQ_PORT, &gpio );

    /* LED индикатор -- выход */
    HAL_GPIO_WritePin( LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET );
    gpio.Pin   = LED_G_PIN;
    gpio.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio.Pull  = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init( LED_G_PORT, &gpio );
}

void SX126x_Reset( void )
{
    HAL_GPIO_WritePin( RADIO_NRST_PORT, RADIO_NRST_PIN, GPIO_PIN_RESET );
    HAL_Delay( 5 );
    HAL_GPIO_WritePin( RADIO_NRST_PORT, RADIO_NRST_PIN, GPIO_PIN_SET );
    HAL_Delay( 10 );
    SX126x_WaitOnBusy( 100 );
}

bool SX126x_WaitOnBusy( uint32_t timeout_ms )
{
    uint32_t start = HAL_GetTick();
    while( HAL_GPIO_ReadPin( RADIO_BUSY_PORT, RADIO_BUSY_PIN ) == GPIO_PIN_SET )
    {
        if( ( HAL_GetTick() - start ) > timeout_ms )
        {
            return false; /* тайм-аут -- чип не отвечает */
        }
    }
    return true;
}

void SX126x_WriteCommand( uint8_t opcode, uint8_t *params, uint16_t paramsLen )
{
    SX126x_WaitOnBusy( 1000 );

    CS_Low();
    HAL_SPI_Transmit( &hspi2, &opcode, 1, 1000 );
    if( paramsLen > 0 && params != NULL )
    {
        HAL_SPI_Transmit( &hspi2, params, paramsLen, 1000 );
    }
    CS_High();

    if( opcode != SX126X_CMD_SET_SLEEP )
    {
        SX126x_WaitOnBusy( 1000 );
    }
}

void SX126x_ReadCommand( uint8_t opcode, uint8_t *buffer, uint16_t bufferLen )
{
    uint8_t nop = 0x00;

    SX126x_WaitOnBusy( 1000 );

    CS_Low();
    HAL_SPI_Transmit( &hspi2, &opcode, 1, 1000 );
    HAL_SPI_Transmit( &hspi2, &nop, 1, 1000 ); /* статусный байт, отбрасываем */
    for( uint16_t i = 0; i < bufferLen; i++ )
    {
        HAL_SPI_TransmitReceive( &hspi2, &nop, &buffer[i], 1, 1000 );
    }
    CS_High();

    SX126x_WaitOnBusy( 1000 );
}

uint8_t SX126x_GetStatus( void )
{
    uint8_t opcode = SX126X_CMD_GET_STATUS;
    uint8_t status = 0x00;
    uint8_t nop = 0x00;

    SX126x_WaitOnBusy( 1000 );

    CS_Low();
    HAL_SPI_Transmit( &hspi2, &opcode, 1, 1000 );
    HAL_SPI_TransmitReceive( &hspi2, &nop, &status, 1, 1000 );
    CS_High();

    return status;
}

bool SX126x_TestCommunication( void )
{
    uint8_t status;

    /* 0x00 или 0xFF на линии MISO почти всегда значит "нет ответа / нет чипа" */
    status = SX126x_GetStatus();

    if( status == 0x00 || status == 0xFF )
    {
        return false;
    }

    /*
     * Биты [6:4] статусного байта -- код режима чипа:
     * 2 = STBY_RC, 3 = STBY_XOSC, 4 = FS, 5 = RX, 6 = TX
     * После сброса ожидаем STBY_RC (2).
     */
    uint8_t chipMode = ( status >> 4 ) & 0x07;

    return ( chipMode != 0 );
}
