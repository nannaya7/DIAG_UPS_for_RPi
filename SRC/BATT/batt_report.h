/**
  ******************************************************************************
  * @file    batt_report.h
  * @brief   Battery monitoring: MAX17048 ALRT handling and the once-a-minute
  *          SOC / cell-voltage report with 0% verification.
  ******************************************************************************
  */
#ifndef __BATT_REPORT_H
#define __BATT_REPORT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/**
  * @brief  Clears and reports a pending ALRT interrupt, if any. Call from the
  *         main loop (and from any other loop that blocks for long).
  */
void BattReport_ServiceAlert(void);

/**
  * @brief  Drives the 0%-SOC re-check sequence. Call every main-loop pass.
  */
void BattReport_Service(void);

/**
  * @brief  Call once per 1Hz wake tick; publishes a report every
  *         BATT_REPORT_PERIOD_TICKS ticks.
  */
void BattReport_Tick1s(void);

/** @brief Resets the report timer and any 0% verification (new ON session). */
void BattReport_Reset(void);

#ifdef __cplusplus
}
#endif

#endif /* __BATT_REPORT_H */
