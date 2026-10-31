/**
  ******************************************************************************
  * @file    debug_log.c
  * @brief   printf-style debug output over SEGGER RTT (channel 0), read via
  *          J-Link RTT Viewer / Ozone / Keil's RTT Viewer window over the
  *          existing SWD debug connection. No UART wiring needed.
  ******************************************************************************
  */
#include "debug_log.h"

/* When BN_RTT isn't defined, debug_log.h turns DebugLog_Init/DebugLog_Printf
   into no-op macros -- defining real functions with those same names here
   would collide with them, so skip this whole file's body in that case. */
#ifdef BN_RTT

#include "SEGGER_RTT.h"
#include <stdarg.h>

void DebugLog_Init(void)
{
  SEGGER_RTT_Init();
}

void DebugLog_Printf(const char *fmt, ...)
{
  va_list args;

  va_start(args, fmt);
  SEGGER_RTT_vprintf(0, fmt, &args);
  va_end(args);
}

#endif /* BN_RTT */
