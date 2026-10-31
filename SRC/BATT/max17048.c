/**
  ******************************************************************************
  * @file    max17048.c
  * @brief   Minimal register-level driver for the MAX17048 fuel gauge.
  ******************************************************************************
  */
#include "max17048.h"

extern I2C_HandleTypeDef hi2c1;

#define MAX17048_I2C_ADDR   (0x36U << 1)
#define MAX17048_REG_VCELL  0x02U
#define MAX17048_REG_SOC    0x04U
#define MAX17048_REG_CONFIG 0x0CU
#define MAX17048_CONFIG_ALRT_BIT  0x0020U /* bit5 of the low byte */
#define MAX17048_RCOMP_DEFAULT    0x97U   /* datasheet default; starting point for future tuning */
#define I2C_TIMEOUT_MS      10U

static uint16_t Max17048_ReadReg16(uint8_t reg)
{
  uint8_t buf[2] = {0};

  if (HAL_I2C_Mem_Read(&hi2c1, MAX17048_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                        buf, 2U, I2C_TIMEOUT_MS) != HAL_OK)
  {
    return 0U;
  }

  return (uint16_t)((buf[0] << 8) | buf[1]);
}

static void Max17048_WriteReg16(uint8_t reg, uint16_t value)
{
  uint8_t buf[2];

  buf[0] = (uint8_t)(value >> 8);
  buf[1] = (uint8_t)(value & 0xFFU);

  HAL_I2C_Mem_Write(&hi2c1, MAX17048_I2C_ADDR, reg, I2C_MEMADD_SIZE_8BIT,
                     buf, 2U, I2C_TIMEOUT_MS);
}

void Max17048_Init(void)
{
  uint16_t config = Max17048_ReadReg16(MAX17048_REG_CONFIG);

  config = (uint16_t)((config & 0x00FFU) | ((uint16_t)MAX17048_RCOMP_DEFAULT << 8));
  Max17048_WriteReg16(MAX17048_REG_CONFIG, config);
}

uint16_t Max17048_ReadVoltageMv(void)
{
  uint16_t raw = Max17048_ReadReg16(MAX17048_REG_VCELL);

  /* 78.125uV/bit = 125/1600 mV/bit */
  return (uint16_t)(((uint32_t)raw * 125U) / 1600U);
}

uint16_t Max17048_ReadSocPercent(void)
{
  uint16_t raw = Max17048_ReadReg16(MAX17048_REG_SOC);
  uint16_t pct = (uint16_t)(raw >> 8); /* high byte = 1%/bit */

  /* ModelGauge can report slightly over 100% near full charge; clamp for display. */
  return (pct > 100U) ? 100U : pct;
}

void Max17048_ClearAlert(void)
{
  uint16_t config = Max17048_ReadReg16(MAX17048_REG_CONFIG);

  config &= (uint16_t)~MAX17048_CONFIG_ALRT_BIT;
  Max17048_WriteReg16(MAX17048_REG_CONFIG, config);
}
