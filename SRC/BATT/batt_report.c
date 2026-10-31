/**
  ******************************************************************************
  * @file    batt_report.c
  * @brief   Battery monitoring (see batt_report.h).
  *
  * BATT_STATUS (batt_status.c) is decided purely from the SOC percentage on
  * each report (<= BATT_STATUS_LOW_PCT -> LOW) -- deliberately NOT tied to
  * the MAX17048 ALRT interrupt, since that pin can spuriously fire
  * independent of SOC.
  *
  * A MAX17048 I2C failure and a real 0% SOC both read as zero, so a zero
  * reading is re-checked SOC_ZERO_RETRY_COUNT more times at
  * SOC_ZERO_RETRY_INTERVAL_MS before it is published. A nonzero retry is
  * published at once.
  ******************************************************************************
  */
#include "batt_report.h"
#include "max17048.h"
#include "batt_status.h"
#include "at_protocol.h"
#include "debug_log.h"

#define BATT_REPORT_PERIOD_TICKS   60U    /* once per minute (1Hz ticks) */
#define BATT_STATUS_LOW_PCT        15U    /* SOC at or below this -> BATT_STATUS_LOW */
#define SOC_ZERO_RETRY_INTERVAL_MS 10000U
#define SOC_ZERO_RETRY_COUNT       3U

static volatile uint8_t s_alertPending = 0U;  /* EXTI ISR -> main loop */
static uint32_t s_tickCount            = 0U;
static uint8_t  s_zeroVerifyActive     = 0U;
static uint8_t  s_zeroRetryCount       = 0U;
static uint32_t s_zeroLastRetryTick    = 0U;

static void BattReport_Publish(uint8_t socPct)
{
  AtProtocol_SendBattery(socPct);
  AtProtocol_SendBatteryVoltage(Max17048_ReadVoltageMv());
  BattStatus_Set((socPct <= BATT_STATUS_LOW_PCT) ? BATT_STATUS_LOW : BATT_STATUS_NORMAL);
}

static void BattReport_Start(void)
{
  uint8_t socPct = (uint8_t)Max17048_ReadSocPercent();

  if (socPct != 0U)
  {
    BattReport_Publish(socPct);
    return;
  }

  s_zeroVerifyActive  = 1U;
  s_zeroRetryCount    = 0U;
  s_zeroLastRetryTick = HAL_GetTick();
  DebugLog_Printf("battery: SOC changed/read as 0%% -> verify 3 times\r\n");
}

void BattReport_ServiceAlert(void)
{
  if (s_alertPending != 0U)
  {
    s_alertPending = 0U;
    Max17048_ClearAlert();
    DebugLog_Printf("battery: low-SOC alert\r\n");
    AtProtocol_SendBatteryLow();
  }
}

void BattReport_Service(void)
{
  uint8_t socPct;

  if ((s_zeroVerifyActive == 0U) ||
      ((HAL_GetTick() - s_zeroLastRetryTick) < SOC_ZERO_RETRY_INTERVAL_MS))
  {
    return;
  }

  s_zeroLastRetryTick = HAL_GetTick();
  s_zeroRetryCount++;
  socPct = (uint8_t)Max17048_ReadSocPercent();

  DebugLog_Printf("battery: SOC 0%% verify %u/%u -> %u%%\r\n",
                   (unsigned int)s_zeroRetryCount,
                   (unsigned int)SOC_ZERO_RETRY_COUNT,
                   (unsigned int)socPct);

  if (socPct != 0U)
  {
    s_zeroVerifyActive = 0U;
    BattReport_Publish(socPct);
  }
  else if (s_zeroRetryCount >= SOC_ZERO_RETRY_COUNT)
  {
    s_zeroVerifyActive = 0U;
    BattReport_Publish(0U);
  }
}

void BattReport_Tick1s(void)
{
  s_tickCount++;
  if (s_tickCount >= BATT_REPORT_PERIOD_TICKS)
  {
    s_tickCount = 0U;
    if (s_zeroVerifyActive == 0U)
    {
      BattReport_Start();
    }
  }
}

void BattReport_Reset(void)
{
  s_tickCount        = 0U;
  s_zeroVerifyActive = 0U;
  s_zeroRetryCount   = 0U;
}

/* BTN_PWR_IN needs no action here: EXTI0 wakes SLEEP mode by itself, and
   PowerControl_Update() reads the pin level after every wake. */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  if (GPIO_Pin == BATT_ALT_Pin)
  {
    s_alertPending = 1U;
  }
}
