/*
 * sx126x_hal.h
 *
 * Минимальный драйвер для проверки связи с радиочипом SX1262 (модуль E22-900M33S)
 * через SPI2 на STM32F072CBU6. Используется только для теста "чип отвечает / не отвечает",
 * не является полноценным драйвером радио.
 */

#ifndef SX126X_HAL_H
#define SX126X_HAL_H

#include "stm32f0xx_hal.h"
#include <stdint.h>
#include <stdbool.h>

/* Опкоды команд SX126x (см. datasheet SX1262, раздел SPI Interface) */
#define SX126X_CMD_GET_STATUS        0xC0
#define SX126X_CMD_SET_STANDBY       0x80
#define SX126X_CMD_GET_DEVICE_ERRORS 0x17
#define SX126X_CMD_SET_SLEEP         0x84

/* Инициализация GPIO-пинов радио (CS, BUSY, IRQ, NRESET, LED) */
void RadioPins_Init( void );

/* Аппаратный сброс радиочипа */
void SX126x_Reset( void );

/* Ожидание готовности чипа (BUSY == LOW), с таймаутом */
bool SX126x_WaitOnBusy( uint32_t timeout_ms );

/* Отправка команды с опциональными параметрами, чтение ответа */
void SX126x_WriteCommand( uint8_t opcode, uint8_t *params, uint16_t paramsLen );
void SX126x_ReadCommand( uint8_t opcode, uint8_t *buffer, uint16_t bufferLen );

/* Прочитать регистр статуса чипа (1 байт) */
uint8_t SX126x_GetStatus( void );

/* Простой тест: проверяет, отвечает ли чип корректным статусом */
bool SX126x_TestCommunication( void );

#endif /* SX126X_HAL_H */
