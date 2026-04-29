/**
 * @file   safety.h
 * @brief  Safety interlock — REM, overcurrent, overtemperature.
 *
 * @details
 *  All checks run against the latest ADC scan from adc_monitor.
 *  On any fault the caller (app_fsm) must:
 *    1. Immediately call rf_gen_disable_all().
 *    2. Call adc_monitor_dac_zero().
 *    3. Transition to ESU_STATE_ERROR or ESU_STATE_REM_ALARM.
 */

#ifndef _SAFETY_H
#define _SAFETY_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "adc_monitor.h"

/** @brief  Fault bitmask returned by safety_check(). */
typedef enum {
    SAFETY_OK          = 0x00,
    SAFETY_FAULT_REM   = 0x01,
    SAFETY_FAULT_OC    = 0x02,   // overcurrent
    SAFETY_FAULT_OT    = 0x04,   // overtemperature
} Safety_Fault_e;

/**
 * @brief  Run all safety checks in one call.
 * @param  bipolar  Set true when operating in bipolar mode (skips REM check).
 * @return Bitmask of active faults (0 = all OK).
 */
Safety_Fault_e safety_check(bool bipolar);

/**
 * @brief  Return true if the REM electrode contact is acceptable.
 */
bool safety_rem_ok(bool bipolar);

/**
 * @brief  Return true if an overcurrent condition is detected.
 */
bool safety_overcurrent(void);

/**
 * @brief  Return true if a thermal overload is detected.
 */
bool safety_overtemp(void);

#ifdef __cplusplus
}
#endif

#endif /* _SAFETY_H */