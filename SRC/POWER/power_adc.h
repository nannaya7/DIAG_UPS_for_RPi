/**
  ******************************************************************************
  * @file    power_adc.h
  * @brief   DCDC output voltage sensing over ADC1. Battery voltage/SOC is
  *          read from the MAX17048 fuel gauge (max17048.h) instead -- the
  *          BATT_ADC divider is a populated backup path, not read here.
  ******************************************************************************
  */
#ifndef __POWER_ADC_H
#define __POWER_ADC_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Runs the ADC1 self-calibration. Call once after MX_ADC1_Init().
  */
void PowerAdc_Init(void);

/**
  * @brief  Reads the (always-on) DCDC output voltage divider.
  * @retval DCDC output voltage in millivolts.
  */
uint16_t PowerAdc_ReadDcdcMv(void);

#ifdef __cplusplus
}
#endif

#endif /* __POWER_ADC_H */
