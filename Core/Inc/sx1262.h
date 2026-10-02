#ifndef SX1262_H
#define SX1262_H

#include <stdint.h>

/* Отладочные переменные, которые заполняет радиослой */
extern volatile uint8_t  dbg_mode;     /* режим чипа при ошибке TX */
extern volatile int16_t  rx_rssi;      /* уровень сигнала, дБм */
extern volatile int8_t   rx_snr;       /* SNR, дБ */

int      radio_init(void);                                /* 0 = ок, -1 = SPI не отвечает */
int      radio_send(const uint8_t *data, uint8_t len);    /* 0 = отправлено, -1 = ошибка */
void     radio_start_rx(void);
int      radio_poll_rx(uint8_t *out, uint8_t max);        /* длина, 0 = нет пакета, -1 = ошибка CRC */

uint16_t get_device_errors(void);
void     clear_device_errors(void);

#endif