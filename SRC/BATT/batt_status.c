/**
  ******************************************************************************
  * @file    batt_status.c
  * @brief   Battery low-SOC status holder (see batt_status.h). Every status
  *          change is logged.
  ******************************************************************************
  */
#include "batt_status.h"
#include "debug_log.h"

static BattStatus_t s_status = BATT_STATUS_NORMAL;

const char *BattStatus_Name(BattStatus_t status)
{
  switch (status)
  {
    case BATT_STATUS_LOW: return "LOW";
    case BATT_STATUS_NORMAL:
    default:                return "NORMAL";
  }
}

void BattStatus_Set(BattStatus_t status)
{
  if (status != s_status)
  {
    DebugLog_Printf("batt_status: %s -> %s\r\n",
                     BattStatus_Name(s_status), BattStatus_Name(status));
    s_status = status;
  }
}

BattStatus_t BattStatus_Get(void)
{
  return s_status;
}
