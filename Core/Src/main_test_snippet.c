/*
 * main_test_snippet.c
 *
 * Этот файл НЕ компилируется отдельно -- он показывает, что именно нужно
 * добавить в сгенерированный CubeMX main.c, в соответствующие блоки
 * /* USER CODE BEGIN ... */ / /* USER CODE END ... */.
 *
 * Логика теста:
 *  - Сброс радиочипа SX1262
 *  - Опрос GetStatus несколько раз
 *  - Если чип отвечает корректно -- быстрое мигание LED (проверка ОК)
 *  - Если не отвечает -- медленное мигание LED (проблема со связью/пайкой)
 */

/* USER CODE BEGIN Includes */
#include "radio_pins.h"
#include "sx126x_hal.h"
/* USER CODE END Includes */


/* USER CODE BEGIN 2 */
/* Помещается в main() после MX_SPI2_Init() и других MX_*_Init() */

RadioPins_Init();      /* настройка CS/BUSY/IRQ/NRESET/LED вручную, не через CubeMX */
SX126x_Reset();        /* аппаратный сброс модуля E22-900M33S */

bool radioOk = SX126x_TestCommunication();

if( radioOk )
{
    /* Чип отвечает -- частое мигание (5 раз быстро) как индикатор успеха */
    for( int i = 0; i < 10; i++ )
    {
        HAL_GPIO_TogglePin( LED_G_PORT, LED_G_PIN );
        HAL_Delay( 100 );
    }
}
else
{
    /* Чип не отвечает -- медленное мигание (проверьте пайку/питание/пины) */
    for( int i = 0; i < 6; i++ )
    {
        HAL_GPIO_TogglePin( LED_G_PORT, LED_G_PIN );
        HAL_Delay( 500 );
    }
}
/* USER CODE END 2 */


/* USER CODE BEGIN WHILE */
while( 1 )
{
    /* В основном цикле -- дублируем индикацию статуса связи с чипом,
       чтобы было видно состояние даже после начального теста */
    uint8_t status = SX126x_GetStatus();

    if( status != 0x00 && status != 0xFF )
    {
        HAL_GPIO_WritePin( LED_G_PORT, LED_G_PIN, GPIO_PIN_SET );
    }
    else
    {
        HAL_GPIO_WritePin( LED_G_PORT, LED_G_PIN, GPIO_PIN_RESET );
    }

    HAL_Delay( 500 );
/* USER CODE END WHILE */

/* USER CODE BEGIN 3 */
}
/* USER CODE END 3 */
