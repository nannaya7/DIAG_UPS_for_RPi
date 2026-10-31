/**
  ******************************************************************************
  * @file    batt_status.h
  * @brief   Battery low-SOC status holder (NORMAL/LOW), latched from the
  *          MAX17048 ALRT interrupt. See batt_status.c for how it clears.
  ******************************************************************************
  */
#ifndef __BATT_STATUS_H
#define __BATT_STATUS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

typedef enum
{
  BATT_STATUS_NORMAL = 0,
  BATT_STATUS_LOW
} BattStatus_t;

/**
  * @brief  Sets the current battery status. Logs the transition if the
  *         status actually changes.
  */
void BattStatus_Set(BattStatus_t status);

/**
  * @retval Current battery status. Defaults to BATT_STATUS_NORMAL at boot.
  */
BattStatus_t BattStatus_Get(void);

/**
  * @retval Human-readable name for a BattStatus_t.
  */
const char *BattStatus_Name(BattStatus_t status);

#ifdef __cplusplus
}
#endif

#endif /* __BATT_STATUS_H */
