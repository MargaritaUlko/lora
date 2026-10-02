#include <string.h>
#include "stm32f0xx_hal.h"
#include "hw_board.h"
#include "sx126x.h"
#include "sx126x-board.h"
#include "timer.h"

#ifndef R_DIO1_PORT
#define R_DIO1_PORT GPIOB
#define R_DIO1_PIN  GPIO_PIN_6
#endif

void spi_xfer(uint8_t *tx, uint8_t *rx, uint16_t n);  /* из hw_board, проверь сигнатуру */
volatile uint32_t dbg_dio1_hits = 0;
volatile uint8_t  dbg_dio1_now = 0;
static uint8_t txb[270], rxb[270];
static DioIrqHandler *irq_cb;
static RadioOperatingModes_t op_mode = MODE_STDBY_RC;

/* ---------- пины ---------- */
void SX126xIoIrqInit(DioIrqHandler dioIrq)
{
  irq_cb = dioIrq;
  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_INPUT;
  g.Pull = GPIO_NOPULL;
  g.Pin = R_DIO1_PIN;  HAL_GPIO_Init(R_DIO1_PORT, &g);
  g.Pin = R_BUSY_PIN;  HAL_GPIO_Init(R_BUSY_PORT, &g);
}

uint32_t SX126xGetDio1PinState(void)
{
  return HAL_GPIO_ReadPin(R_DIO1_PORT, R_DIO1_PIN) == GPIO_PIN_SET;
}

void SX126xReset(void)
{
  HAL_GPIO_WritePin(R_NSS_PORT, R_NSS_PIN, GPIO_PIN_SET);
  HAL_GPIO_WritePin(R_RST_PORT, R_RST_PIN, GPIO_PIN_RESET);
  HAL_Delay(2);
  HAL_GPIO_WritePin(R_RST_PORT, R_RST_PIN, GPIO_PIN_SET);
  HAL_Delay(50);
}

void SX126xWaitOnBusy(void)
{
  uint32_t t0 = HAL_GetTick();
  while (HAL_GPIO_ReadPin(R_BUSY_PORT, R_BUSY_PIN) == GPIO_PIN_SET)
    if (HAL_GetTick() - t0 > 100) break;
}

void SX126xWakeup(void)
{
  uint8_t t[2] = { 0xC0, 0 }, r[2];
  spi_xfer(t, r, 2);
  SX126xWaitOnBusy();
  op_mode = MODE_STDBY_RC;
}

/* ---------- SPI-команды ---------- */
void SX126xWriteCommand(RadioCommands_t cmd, uint8_t *buf, uint16_t size)
{
  SX126xWaitOnBusy();
  txb[0] = (uint8_t)cmd;
  memcpy(&txb[1], buf, size);
  spi_xfer(txb, rxb, size + 1);
}

uint8_t SX126xReadCommand(RadioCommands_t cmd, uint8_t *buf, uint16_t size)
{
  SX126xWaitOnBusy();
  memset(txb, 0, size + 2);
  txb[0] = (uint8_t)cmd;
  spi_xfer(txb, rxb, size + 2);
  memcpy(buf, &rxb[2], size);
  return rxb[1];
}

void SX126xWriteRegisters(uint16_t addr, uint8_t *buf, uint16_t size)
{
  SX126xWaitOnBusy();
  txb[0] = 0x0D; txb[1] = addr >> 8; txb[2] = addr;
  memcpy(&txb[3], buf, size);
  spi_xfer(txb, rxb, size + 3);
}

void SX126xReadRegisters(uint16_t addr, uint8_t *buf, uint16_t size)
{
  SX126xWaitOnBusy();
  memset(txb, 0, size + 4);
  txb[0] = 0x1D; txb[1] = addr >> 8; txb[2] = addr;
  spi_xfer(txb, rxb, size + 4);
  memcpy(buf, &rxb[4], size);
}

void SX126xWriteRegister(uint16_t addr, uint8_t v) { SX126xWriteRegisters(addr, &v, 1); }
uint8_t SX126xReadRegister(uint16_t addr) { uint8_t v; SX126xReadRegisters(addr, &v, 1); return v; }

void SX126xWriteBuffer(uint8_t offset, uint8_t *buf, uint8_t size)
{
  SX126xWaitOnBusy();
  txb[0] = 0x0E; txb[1] = offset;
  memcpy(&txb[2], buf, size);
  spi_xfer(txb, rxb, size + 2);
}

void SX126xReadBuffer(uint8_t offset, uint8_t *buf, uint8_t size)
{
  SX126xWaitOnBusy();
  memset(txb, 0, size + 3);
  txb[0] = 0x1E; txb[1] = offset;
  spi_xfer(txb, rxb, size + 3);
  memcpy(buf, &rxb[3], size);
}

/* ---------- режимы, питание, антенна ---------- */
void SX126xSetOperatingMode(RadioOperatingModes_t m) { op_mode = m; }
RadioOperatingModes_t SX126xGetOperatingMode(void)   { return op_mode; }

void SX126xSetRfTxPower(int8_t power) { SX126xSetTxParams(power, RADIO_RAMP_40_US); }
uint8_t SX126xGetDeviceId(void)       { return SX1262; }
uint32_t SX126xGetBoardTcxoWakeupTime(void) { return 5; }   /* мс, TCXO есть */

void SX126xIoTcxoInit(void)
{
  SX126xSetDio3AsTcxoCtrl(TCXO_CTRL_1_8V, 320);   /* 320 * 15.625 мкс = 5 мс, как 0x000140 в старом коде */
  CalibrationParams_t c;
  c.Value = 0x7F;
  SX126xCalibrate(c);
}
void SX126xIoRfSwitchInit(void) { SX126xSetDio2AsRfSwitchCtrl(true); }
void SX126xAntSwOn(void)        { }
void SX126xAntSwOff(void)       { }

/* ---------- критическая секция ---------- */
void BoardCriticalSectionBegin(uint32_t *mask) { *mask = __get_PRIMASK(); __disable_irq(); }
void BoardCriticalSectionEnd(uint32_t *mask)   { __set_PRIMASK(*mask); }

/* ---------- задержки и таймеры (на HAL_GetTick) ---------- */
void DelayMs(uint32_t ms) { HAL_Delay(ms); }

#define MAX_TIMERS 4
static TimerEvent_t *timers[MAX_TIMERS];

void TimerInit(TimerEvent_t *obj, void (*cb)(void *))
{
  obj->Callback = cb; obj->IsStarted = false; obj->ReloadValue = 0;
  for (int i = 0; i < MAX_TIMERS; i++)
    if (timers[i] == obj || timers[i] == NULL) { timers[i] = obj; break; }
}
void TimerSetValue(TimerEvent_t *obj, uint32_t v) { obj->ReloadValue = v; }
void TimerStart(TimerEvent_t *obj) { obj->Timestamp = HAL_GetTick(); obj->IsStarted = true; }
void TimerStop(TimerEvent_t *obj)  { obj->IsStarted = false; }
TimerTime_t TimerGetCurrentTime(void) { return HAL_GetTick(); }
TimerTime_t TimerGetElapsedTime(TimerTime_t past) { return HAL_GetTick() - past; }

/* ---------- вызывать в главном цикле перед Radio.IrqProcess() ---------- */
void board_poll(void)
{
  for (int i = 0; i < MAX_TIMERS; i++) {
    TimerEvent_t *t = timers[i];
    if (t && t->IsStarted && HAL_GetTick() - t->Timestamp >= t->ReloadValue) {
      t->IsStarted = false;
      if (t->Callback) t->Callback(t->Context);
    }
  }
  if (irq_cb && SX126xGetDio1PinState()) irq_cb(NULL);
}