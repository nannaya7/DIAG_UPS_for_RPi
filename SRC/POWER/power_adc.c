/**
  ******************************************************************************
  * @file    power_adc.c
  * @brief   DCDC output voltage sensing over ADC1.
  *
  * DCDC_ADC (PA5/ADC1_IN5): divider R19/R20 = 100k/100k (factor 2), always on.
  *
  * BATT_ADC (PA4/ADC1_IN4, gated by BATT_ADC_CTRL/PA7 through Q5/Q4) is
  * populated on the board as a backup battery-voltage path, but is no
  * longer read here -- battery voltage/SOC now comes from the MAX17048
  * fuel gauge (max17048.c) over I2C instead.
  ******************************************************************************
  */
#include "power_adc.h"

extern ADC_HandleTypeDef hadc1;

#define ADC_VREF_MV          3300U
#define ADC_FULL_SCALE       4095U
#define DCDC_DIVIDER_FACTOR  2U
#define ADC_TIMEOUT_MS       10U

void PowerAdc_Init(void)
{
  HAL_ADCEx_Calibration_Start(&hadc1);
}

/* Single-shot, software-triggered read of DCDC_ADC (ADC1_IN5): reconfigures
   the channel, starts, polls to completion, then stops. Sampling is only
   needed once per 1Hz tick (see Dcdc_Update()/PowerControl_LogStatus()),
   so the per-call config/start/stop overhead is irrelevant next to how
   rarely this runs. Hardcoded to this one channel -- BATT_ADC (the only
   other channel this ADC ever had) was physically removed from the board,
   so a channel parameter here would just be unused generality. */
static uint32_t PowerAdc_ReadDcdcRaw(void)
{
  ADC_ChannelConfTypeDef sConfig = {0};
  uint32_t raw = 0U;

  sConfig.Channel      = ADC_CHANNEL_5;
  sConfig.Rank         = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
  HAL_ADC_ConfigChannel(&hadc1, &sConfig);

  HAL_ADC_Start(&hadc1);
  if (HAL_ADC_PollForConversion(&hadc1, ADC_TIMEOUT_MS) == HAL_OK)
  {
    raw = HAL_ADC_GetValue(&hadc1);
  }
  HAL_ADC_Stop(&hadc1);

  return raw;
}

uint16_t PowerAdc_ReadDcdcMv(void)
{
  uint32_t raw = PowerAdc_ReadDcdcRaw();

  return (uint16_t)((raw * ADC_VREF_MV * DCDC_DIVIDER_FACTOR) / ADC_FULL_SCALE);
}
