#include <string.h>
#include "radio.h"
#include "sx126x.h"
#include "hw_board.h"
#include "sx1262.h"

void board_poll(void);

#define RF_FREQ_HZ   868000000UL
#define TX_POWER_DBM (-9)
#define PAYLOAD_MAX  32

volatile uint8_t dbg_mode = 0;
volatile int16_t rx_rssi;
volatile int8_t  rx_snr;

static RadioEvents_t events;
static uint8_t rx_buf[PAYLOAD_MAX];
static volatile uint8_t tx_done, rx_len, rx_crc_err;

static void on_tx_done(void)    { tx_done = 1; Radio.Rx(0); }
static void on_tx_timeout(void) { tx_done = 2; }
static void on_rx_done(uint8_t *p, uint16_t size, int16_t rssi, int8_t snr)
{
  if (size > PAYLOAD_MAX) size = PAYLOAD_MAX;
  memcpy(rx_buf, p, size);
  rx_len = size; rx_rssi = rssi; rx_snr = snr;
}
static void on_rx_error(void)   { rx_crc_err = 1; }
static void on_rx_timeout(void) { }

int radio_init(void)
{
  events.TxDone    = on_tx_done;
  events.TxTimeout = on_tx_timeout;
  events.RxDone    = on_rx_done;
  events.RxError   = on_rx_error;
  events.RxTimeout = on_rx_timeout;

  Radio.Init(&events);

  /* проверка связи по SPI, как в старом коде */
  if (Radio.Read(0x0740) != 0x14 || Radio.Read(0x0741) != 0x24) return -1;

  Radio.SetPublicNetwork(false);
  Radio.SetChannel(RF_FREQ_HZ);
  Radio.SetTxConfig(MODEM_LORA, TX_POWER_DBM, 0, 0, 7, 1, 8, false, true, 0, 0, false, 3000);
  Radio.SetRxConfig(MODEM_LORA, 0, 7, 1, 0, 8, 0, false, 0, true, 0, 0, false, true);
  Radio.Rx(0);
  return 0;
}

void radio_start_rx(void) { Radio.Rx(0); }

int radio_send(const uint8_t *data, uint8_t len)
{
  tx_done = 0;
  Radio.Send((uint8_t *)data, len);
  uint32_t t0 = HAL_GetTick();
  while (!tx_done && HAL_GetTick() - t0 < 3000)
  {
    board_poll();
    Radio.IrqProcess();
  }
  return (tx_done == 1) ? 0 : -1;
}

int radio_poll_rx(uint8_t *out, uint8_t max)
{
  board_poll();
  Radio.IrqProcess();
  if (rx_crc_err) { rx_crc_err = 0; return -1; }
  if (!rx_len) return 0;
  uint8_t n = rx_len > max ? max : rx_len;
  memcpy(out, rx_buf, n);
  rx_len = 0;
  return n;
}

uint16_t get_device_errors(void)   { return SX126xGetDeviceErrors().Value; }
void     clear_device_errors(void) { SX126xClearDeviceErrors(); }