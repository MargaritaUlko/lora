#include "main.h"
#include <stdint.h>
#include <string.h>

/* ===================== НАСТРОЙКИ ===================== */
#define ROLE_TX        0            /* 1 = передатчик, 0 = приёмник */
#define RF_FREQ_HZ     868000000UL
#define TX_POWER_DBM   (-9)         /* минимум SX1262; E22-900M33S ещё усиливает сам */

#define USE_DIO2_RFSW  1
#define USE_TCXO       1
#define TCXO_VOLTAGE   0x02
#define USE_DCDC       1

/* ===================== ПИНЫ ===================== */
#define R_NSS_PORT     GPIOB
#define R_NSS_PIN      GPIO_PIN_12
#define R_RST_PORT     GPIOB
#define R_RST_PIN      GPIO_PIN_8
#define R_BUSY_PORT    GPIOB
#define R_BUSY_PIN     GPIO_PIN_7
#define LED_RED_PORT   GPIOB
#define LED_RED_PIN    GPIO_PIN_10
#define LED_GREEN_PORT GPIOB
#define LED_GREEN_PIN  GPIO_PIN_2

SPI_HandleTypeDef hspi2;

/* ===================== ПЕРЕМЕННЫЕ ДЛЯ ОТЛАДЧИКА ===================== */
/* Общие */
volatile uint8_t  dbg_init_ok = 0;     /* 1 = радио инициализировано */

/* Передатчик */
volatile uint32_t tx_ok = 0;           /* пакетов отправлено успешно */
volatile uint32_t tx_fail = 0;         /* TX не завершился */
volatile uint8_t  tx_seq = 0;          /* номер последнего пакета */
volatile uint8_t  dbg_mode = 0;        /* режим чипа при ошибке TX */
volatile uint16_t dbg_err = 0;         /* код ошибки чипа */

/* Приёмник */
volatile uint32_t rx_ok = 0;           /* верных пакетов "Hello" */
volatile uint32_t rx_crc = 0;          /* пакетов с ошибкой CRC */
volatile uint32_t rx_garbage = 0;      /* пакет принят, но содержимое не то */
volatile uint8_t  rx_seq = 0;          /* номер в последнем верном пакете */
volatile uint32_t rx_lost = 0;         /* сколько пакетов пропущено (по номерам) */
volatile int16_t  rx_rssi = 0;         /* уровень сигнала, дБм */
volatile int8_t   rx_snr = 0;          /* SNR, дБ */
volatile uint8_t  rx_last[32];         /* содержимое последнего пакета */
volatile uint8_t  rx_last_len = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI2_Init(void);

/* ===================== ДИАГНОСТИКА ===================== */
/* Коды зелёного светодиода (n вспышек, потом пауза):
   1 = SPI не отвечает
   2 = TX не завершился (причина не определена)
   3 = не запустился генератор TCXO
   4 = PLL не захватил частоту */
static void green_blink(int n)
{
  for (int i = 0; i < n; i++)
  {
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    HAL_Delay(500);
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_Delay(500);
  }
  HAL_Delay(3000);
}

/* ===================== НИЗКИЙ УРОВЕНЬ SX126x ===================== */
static void wait_busy(void)
{
  uint32_t t0 = HAL_GetTick();
  while (HAL_GPIO_ReadPin(R_BUSY_PORT, R_BUSY_PIN) == GPIO_PIN_SET &&
         HAL_GetTick() - t0 < 100) {}
}

static void spi_xfer(uint8_t *tx, uint8_t *rx, uint16_t n)
{
  wait_busy();
  HAL_GPIO_WritePin(R_NSS_PORT, R_NSS_PIN, GPIO_PIN_RESET);
  HAL_SPI_TransmitReceive(&hspi2, tx, rx, n, 100);
  HAL_GPIO_WritePin(R_NSS_PORT, R_NSS_PIN, GPIO_PIN_SET);
}

static void write_cmd(uint8_t op, const uint8_t *d, uint8_t len)
{
  uint8_t tx[40], rx[40];
  tx[0] = op;
  for (uint8_t i = 0; i < len; i++) tx[1 + i] = d[i];
  spi_xfer(tx, rx, len + 1);
}

static uint16_t get_irq(void)
{
  uint8_t tx[4] = { 0x12, 0, 0, 0 }, rx[4];
  spi_xfer(tx, rx, 4);
  return ((uint16_t)rx[2] << 8) | rx[3];
}

static void clear_irq(void)
{
  uint8_t d[2] = { 0x03, 0xFF };
  write_cmd(0x02, d, 2);
}

static uint16_t get_device_errors(void)
{
  uint8_t tx[4] = { 0x17, 0, 0, 0 }, rx[4];
  spi_xfer(tx, rx, 4);
  return ((uint16_t)rx[2] << 8) | rx[3];
}

static void clear_device_errors(void)
{
  uint8_t d[2] = { 0x00, 0x00 };
  write_cmd(0x07, d, 2);
}

static void set_packet_params(uint8_t payload_len)
{
  uint8_t d[6] = { 0x00, 0x08, 0x00, payload_len, 0x01, 0x00 };
  write_cmd(0x8C, d, 6);
}

/* ===================== НАСТРОЙКА РАДИО ===================== */
static int radio_init(void)
{
  uint8_t d[8];

  HAL_GPIO_WritePin(R_NSS_PORT, R_NSS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(R_RST_PORT, R_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(R_RST_PORT, R_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(50);

  uint8_t tx[6] = { 0x1D, 0x07, 0x40, 0, 0, 0 }, rx[6] = { 0 };
  spi_xfer(tx, rx, 6);
  if (rx[4] != 0x14 || rx[5] != 0x24) return -1;

  d[0] = 0x00; write_cmd(0x80, d, 1);

#if USE_TCXO
  d[0] = TCXO_VOLTAGE; d[1] = 0x00; d[2] = 0x01; d[3] = 0x40;
  write_cmd(0x97, d, 4);
  HAL_Delay(10);
  clear_device_errors();
  d[0] = 0x7F; write_cmd(0x89, d, 1);
  HAL_Delay(10);
#endif

#if USE_DCDC
  d[0] = 0x01; write_cmd(0x96, d, 1);
#endif

  d[0] = 0xD7; d[1] = 0xDB;
  write_cmd(0x98, d, 2);

#if USE_DIO2_RFSW
  d[0] = 0x01; write_cmd(0x9D, d, 1);
#endif

  d[0] = 0x01; write_cmd(0x8A, d, 1);

  uint32_t f = (uint32_t)(((uint64_t)RF_FREQ_HZ << 25) / 32000000ULL);
  d[0] = f >> 24; d[1] = f >> 16; d[2] = f >> 8; d[3] = f;
  write_cmd(0x86, d, 4);

  d[0] = 0x04; d[1] = 0x07; d[2] = 0x00; d[3] = 0x01;
  write_cmd(0x95, d, 4);
  d[0] = (uint8_t)(int8_t)TX_POWER_DBM; d[1] = 0x04;
  write_cmd(0x8E, d, 2);

  d[0] = 0x00; d[1] = 0x00;
  write_cmd(0x8F, d, 2);

  d[0] = 0x07; d[1] = 0x04; d[2] = 0x01; d[3] = 0x00;
  write_cmd(0x8B, d, 4);

  set_packet_params(32);

  d[0] = 0x02; d[1] = 0x63;
  d[2] = 0; d[3] = 0; d[4] = 0; d[5] = 0; d[6] = 0; d[7] = 0;
  write_cmd(0x08, d, 8);
  clear_irq();
  return 0;
}

/* ===================== ОТПРАВКА / ПРИЁМ ===================== */
static int radio_send(const uint8_t *data, uint8_t len)
{
  uint8_t buf[40], rx[40];
  set_packet_params(len);

  buf[0] = 0x0E; buf[1] = 0x00;
  for (uint8_t i = 0; i < len; i++) buf[2 + i] = data[i];
  spi_xfer(buf, rx, len + 2);

  clear_irq();
  uint8_t t[3] = { 0, 0, 0 };
  write_cmd(0x83, t, 3);

  uint32_t t0 = HAL_GetTick();
  while (HAL_GetTick() - t0 < 3000)
  {
    if (get_irq() & 0x0001) { clear_irq(); return 0; }
  }

  uint8_t st[2] = { 0xC0, 0 }, sr[2];
  spi_xfer(st, sr, 2);
  dbg_mode = (sr[1] >> 4) & 0x07;

  uint8_t s = 0x00;
  write_cmd(0x80, &s, 1);
  return -1;
}

static void radio_start_rx(void)
{
  clear_irq();
  uint8_t t[3] = { 0xFF, 0xFF, 0xFF };
  write_cmd(0x82, t, 3);
}

/* возвращает длину, 0 если пакета нет, -1 если ошибка CRC */
static int radio_poll_rx(uint8_t *out, uint8_t max)
{
  uint16_t irq = get_irq();
  if (!(irq & 0x0002)) return 0;

  int res;
  if (irq & 0x0040) res = -1;
  else
  {
    /* уровень сигнала: GetPacketStatus */
    uint8_t ps[4] = { 0x14, 0, 0, 0 }, pr[4];
    spi_xfer(ps, pr, 4);
    rx_rssi = -(int16_t)(pr[2] / 2);
    rx_snr = ((int8_t)pr[3]) / 4;

    uint8_t tx[4] = { 0x13, 0, 0, 0 }, rx[4];
    spi_xfer(tx, rx, 4);
    uint8_t len = rx[2], start = rx[3];
    if (len > max) len = max;

    uint8_t btx[40] = { 0x1E, start, 0 }, brx[40];
    spi_xfer(btx, brx, len + 3);
    for (uint8_t i = 0; i < len; i++) out[i] = brx[3 + i];
    res = len;
  }
  radio_start_rx();
  return res;
}

/* ===================== MAIN ===================== */
int main(void)
{
  HAL_Init();
  SystemClock_Config();
  MX_GPIO_Init();
  MX_SPI2_Init();

  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
  HAL_Delay(500);
  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

  if (radio_init() != 0)
  {
    dbg_init_ok = 0;
    while (1) green_blink(1);
  }
  dbg_init_ok = 1;

#if ROLE_TX
  while (1)
  {
    uint8_t msg[6] = { 'H', 'e', 'l', 'l', 'o', tx_seq };
    tx_seq++;
    if (radio_send(msg, sizeof(msg)) == 0)
    {
      tx_ok++;
      HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
      HAL_Delay(1000);
    }
    else
    {
      uint16_t e = get_device_errors();
      clear_device_errors();
      tx_fail++;
      dbg_err = e;
      if (e & 0x0020)      green_blink(3);
      else if (e & 0x0040) green_blink(4);
      else
      {
        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
        HAL_Delay(2000);
        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
        HAL_Delay(2000);

        for (int i = 0; i < dbg_mode; i++)
        {
          HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
          HAL_Delay(500);
          HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
          HAL_Delay(500);
        }
        HAL_Delay(2500);
        green_blink(2);
      }
    }
  }
#else
  uint8_t buf[32];
  uint8_t first = 1;
  uint32_t red_off_at = 0, green_off_at = 0, last_rx = HAL_GetTick();
  radio_start_rx();
  while (1)
  {
    uint32_t now = HAL_GetTick();

    /* гасим светодиоды через 100 мс после вспышки */
    if (red_off_at   && now >= red_off_at)   { HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);       red_off_at = 0; }
    if (green_off_at && now >= green_off_at) { HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);   green_off_at = 0; }

    /* если 3 с тишины - перезапускаем приём */
    if (now - last_rx > 3000)
    {
      clear_device_errors();
      radio_start_rx();
      last_rx = now;
    }

    int n = radio_poll_rx(buf, sizeof(buf));
    if (n == 0) continue;
    last_rx = HAL_GetTick();

    if (n < 0)
    {
      rx_crc++;
      HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
      green_off_at = last_rx + 100;
      continue;
    }

    rx_last_len = (uint8_t)n;
    for (int i = 0; i < n; i++) rx_last[i] = buf[i];

    if (n == 6 && memcmp(buf, "Hello", 5) == 0)
    {
      if (!first) rx_lost += (uint8_t)(buf[5] - rx_seq - 1);  /* пропуски по номеру */
      first = 0;
      rx_seq = buf[5];
      rx_ok++;
      HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);   /* красный: короткая вспышка на верный пакет */
      red_off_at = last_rx + 100;
    }
    else
    {
      rx_garbage++;
      HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
      green_off_at = last_rx + 100;
    }
  }
#endif
}

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

static void MX_SPI2_Init(void)
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

static void MX_GPIO_Init(void)
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

  g.Mode = GPIO_MODE_INPUT;
  g.Pin = R_BUSY_PIN;    HAL_GPIO_Init(R_BUSY_PORT, &g);

  HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
}

void Error_Handler(void)
{
  __disable_irq();
  while (1) {}
}