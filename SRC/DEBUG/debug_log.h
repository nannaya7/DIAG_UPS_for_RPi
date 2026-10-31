/**
  ******************************************************************************
  * @file    debug_log.h
  * @brief   printf-style debug output over SEGGER RTT (no UART wiring
  *          needed; view with J-Link RTT Viewer / Ozone / Keil RTT Viewer).
  ******************************************************************************
  */
#ifndef __DEBUG_LOG_H
#define __DEBUG_LOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

/* All debug messages compile away completely when BN_RTT isn't defined
   (see the Keil project's C/C++ Preprocessor Symbols) -- every
   DebugLog_Printf() call site everywhere in the codebase is gated by
   this one place, nothing else needs to change. */
#ifdef BN_RTT

void DebugLog_Init(void);
void DebugLog_Printf(const char *fmt, ...);

#else

#define DebugLog_Init()       ((void)0)
#define DebugLog_Printf(...)  ((void)0)

#endif /* BN_RTT */

#ifdef __cplusplus
}
#endif

#endif /* __DEBUG_LOG_H */
