/**
  ******************************************************************************
  * @file    max17048.h
  * @brief   Minimal register-level driver for the MAX17048 fuel gauge (U8),
  *          reached over I2C1. ALRT (open-drain, active low) is wired to
  *          BATT_ALT (PA15/EXTI15).
  ******************************************************************************
  */
#ifndef __MAX17048_H
#define __MAX17048_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Writes RCOMP (CONFIG[15:8]) to its datasheet default (0x97).
  *         Called once at boot; a starting point for future empirical
  *         tuning against this specific battery pack, not just relying
  *         on whatever RCOMP happened to be left at.
  */
void Max17048_Init(void);

/**
  * @brief  State of charge from the chip's own ModelGauge SOC register
  *         (coulomb-counting based, so it tracks charge/discharge current
  *         correctly -- unlike a plain VCELL voltage mapping, which swings
  *         high while charging or low under heavy load due to IR drop).
  * @retval State of charge in percent (0-100).
  */
uint16_t Max17048_ReadSocPercent(void);

/**
  * @retval Cell voltage in millivolts.
  */
uint16_t Max17048_ReadVoltageMv(void);

/**
  * @brief  Clears the ALRT flag in CONFIG so the ALRT pin de-asserts.
  */
void Max17048_ClearAlert(void);

#ifdef __cplusplus
}
#endif

#endif /* __MAX17048_H */
