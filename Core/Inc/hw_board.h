#ifndef HW_BOARD_H
#define HW_BOARD_H

#include "main.h"
#include <stdint.h>

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

extern SPI_HandleTypeDef hspi2;

/* Инициализация железа */
void SystemClock_Config(void);
void MX_GPIO_Init(void);
void MX_SPI2_Init(void);

/* Обмен с радиочипом по SPI (с ожиданием BUSY и управлением NSS) */
void wait_busy(void);
void spi_xfer(uint8_t *tx, uint8_t *rx, uint16_t n);

#endif