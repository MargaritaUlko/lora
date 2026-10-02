#include "hw_board.h"

SPI_HandleTypeDef hspi2;

/* ===================== ТАКТИРОВАНИЕ ===================== */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef osc = {0};
  RCC_ClkInitTypeDef clk = {0};

  osc.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  osc.HSIState = RCC_HSI_ON;
  osc.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  osc.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&osc) != HAL_OK) Error_Handler();

  clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
  clk.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
  clk.APB1CLKDivider = RCC_HCLK_DIV1;
  if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_0) != HAL_OK) Error_Handler();
}

/* ===================== SPI2 ===================== */
void MX_SPI2_Init(void)
{
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_8;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 7;
  hspi2.Init.CRCLength = SPI_CRC_LENGTH_DATASIZE;
  hspi2.Init.NSSPMode = SPI_NSS_PULSE_DISABLE;
  if (HAL_SPI_Init(&hspi2) != HAL_OK) Error_Handler();
}

/* ===================== GPIO ===================== */
void MX_GPIO_Init(void)
{
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  GPIO_InitTypeDef g = {0};
  g.Speed = GPIO_SPEED_FREQ_HIGH;
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Pin = R_NSS_PIN;     HAL_GPIO_Init(R_NSS_PORT, &g);
  g.Pin = R_RST_PIN;     HAL_GPIO_Init(R_RST_PORT, &g);
  g.Pin = LED_RED_PIN;   HAL_GPIO_Init(LED_RED_PORT, &g);
  g.Pin = LED_GREEN_PIN; HAL_GPIO_Init(LED_GREEN_PORT, &g);
  g.Pin = R_BUSY_PIN;    HAL_GPIO_Init(R_BUSY_PORT, &g);
  g.Mode = GPIO_MODE_INPUT;


  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
}

/* ===================== НИЗКИЙ УРОВЕНЬ SPI ===================== */
void wait_busy(void)
{
  uint32_t t0 = HAL_GetTick();
  while (HAL_GPIO_ReadPin(R_BUSY_PORT, R_BUSY_PIN) == GPIO_PIN_SET &&
         HAL_GetTick() - t0 < 100) {}
}

void spi_xfer(uint8_t *tx, uint8_t *rx, uint16_t n)
{
  wait_busy();
  HAL_GPIO_WritePin(R_NSS_PORT, R_NSS_PIN, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(&hspi2, tx, rx, n, 100);
  HAL_GPIO_WritePin(R_NSS_PORT, R_NSS_PIN, GPIO_PIN_SET);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}