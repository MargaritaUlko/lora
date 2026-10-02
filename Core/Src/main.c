#include "main.h"
#include <stdint.h>
#include <string.h>
#include "hw_board.h"
#include "sx1262.h"

/* ===================== НАСТРОЙКИ ===================== */
#define ROLE_TX        1            /* 1 = передатчик, 0 = приёмник */

/* ===================== ПЕРЕМЕННЫЕ ДЛЯ ОТЛАДЧИКА ===================== */
volatile uint8_t  dbg_init_ok = 0;

/* Передатчик */
volatile uint32_t tx_ok = 0;
volatile uint32_t tx_fail = 0;
volatile uint8_t  tx_seq = 0;
volatile uint16_t dbg_err = 0;

/* Приёмник */
volatile uint32_t rx_ok = 0;
volatile uint32_t rx_crc = 0;
volatile uint32_t rx_garbage = 0;
volatile uint8_t  rx_seq = 0;
volatile uint32_t rx_lost = 0;
volatile uint8_t  rx_last[32];
volatile uint8_t  rx_last_len = 0;

/* Коды зелёного светодиода: 1 = SPI не отвечает, 2 = TX не завершился,
   3 = не запустился TCXO, 4 = PLL не захватил частоту */
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

    if (red_off_at   && now >= red_off_at)   { HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);     red_off_at = 0; }
    if (green_off_at && now >= green_off_at) { HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET); green_off_at = 0; }

    /* 3 с тишины - перезапускаем приём */
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
      if (!first) rx_lost += (uint8_t)(buf[5] - rx_seq - 1);
      first = 0;
      rx_seq = buf[5];
      rx_ok++;
      HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);
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