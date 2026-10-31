/**
  ******************************************************************************
  * @file    at_protocol.h
  * @brief   AT-command line protocol between this MCU and the main board,
  *          carried over the HOST port (see uart_router.c/.h).
  ******************************************************************************
  */
#ifndef __AT_PROTOCOL_H
#define __AT_PROTOCOL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Resets line-assembly state. Call once after UartRouter_Init().
  */
void AtProtocol_Init(void);

/**
  * @brief  Feeds newly-arrived HOST (USART3) RX bytes into the line
  *         assembler. Not called by anything while AT_PROTOCOL_ENABLED is 0.
  * @param  data Pointer to raw RX bytes.
  * @param  len  Number of bytes.
  */
void AtProtocol_OnRxData(const uint8_t *data, uint16_t len);

/**
  * @brief  Fire-and-forget notifications to the main board (no ACK wait --
  *         the main board always replies AT+OKAY, but nothing here blocks on it).
  */
void AtProtocol_SendBtnShortPress(void);
void AtProtocol_SendBtnLongPress(void);
void AtProtocol_SendUsbPlugIn(void);
void AtProtocol_SendUsbPlugOut(void);

/**
  * @brief  Sends battery SOC as AT+BATxx (xx = 2-digit hex, 0-100 clamped).
  * @param  socPercent Battery state of charge, 0-100.
  */
void AtProtocol_SendBattery(uint8_t socPercent);

/**
  * @brief  Sends AT+BATL: fire-and-forget battery-low-SOC alert, pushed
  *         immediately on the MAX17048 ALRT interrupt instead of waiting
  *         for the next once-a-minute AT+BATxx report.
  */
void AtProtocol_SendBatteryLow(void);

/** @brief Acknowledges the Radxa AT+PWR0 shutdown request. */
void AtProtocol_SendPowerOffAck(void);

/** @brief Sends a fire-and-forget AT+PWR0 request for forced button shutdown. */
void AtProtocol_SendPowerOffRequest(void);

/**
  * @brief  Sends battery cell voltage as AT+BVTxx (xx = 2-digit decimal
  *         tenths of a volt, e.g. 42 -> 4.2V, 38 -> 3.8V), so the main
  *         board can judge battery condition from SOC and voltage together.
  * @param  voltageMv Cell voltage in millivolts (rounded to the nearest
  *                   0.1V and clamped to 2 decimal digits, i.e. 9.9V max).
  */
void AtProtocol_SendBatteryVoltage(uint16_t voltageMv);

#ifdef __cplusplus
}
#endif

#endif /* __AT_PROTOCOL_H */
