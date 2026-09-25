/*
 * radio_pins.h
 *
 * Распиновка модуля LoRa E22-900M33S (SX1262) на плате STM32F072CBU6.
 * SPI2 используется для SCK/MISO/MOSI (настраивается через CubeMX),
 * остальные линии управляются программно через HAL GPIO.
 */

#ifndef RADIO_PINS_H
#define RADIO_PINS_H

#include "stm32f0xx_hal.h"

/* SPI2: SCK = PB13, MISO = PB14, MOSI = PB15 -- настроены в CubeMX (MX_SPI2_Init) */

/* Chip Select (программный, НЕ аппаратный NSS) */
#define RADIO_CS_PORT           GPIOB
#define RADIO_CS_PIN            GPIO_PIN_12

/* BUSY -- вход, высокий уровень значит "радиочип занят" */
#define RADIO_BUSY_PORT         GPIOB
#define RADIO_BUSY_PIN          GPIO_PIN_7

/* IRQ (DIO1) -- вход, прерывание по фронту при TxDone/RxDone и т.д. */
#define RADIO_IRQ_PORT          GPIOB
#define RADIO_IRQ_PIN           GPIO_PIN_6

/* NRESET -- выход, активный низкий уровень */
#define RADIO_NRST_PORT         GPIOB
#define RADIO_NRST_PIN          GPIO_PIN_8

/* Индикаторный светодиод */
#define LED_G_PORT              GPIOB
#define LED_G_PIN               GPIO_PIN_2

#endif /* RADIO_PINS_H */
