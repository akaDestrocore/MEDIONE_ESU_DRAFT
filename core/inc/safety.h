/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   safety.h
 * @brief  Safety interlock — REM, overcurrent, overtemperature.
 *
 * @details
 *  All checks run against the latest ADC scan from adcMonitor.
 *  On any fault the caller (appFsm) must:
 *    1. Immediately call rfGen_disableAll().
 *    2. Call adcMonitor_dacZero().
 *    3. Transition to ESU_STATE_ERROR or ESU_STATE_REM_ALARM.
 */

#ifndef SAFETY_H_
#define SAFETY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

#include "adc_monitor.h"

/**
 * @brief Fault bitmask returned by safetyCheck().
 */
typedef enum
{
    Safety_Fault_e_OK = 0x00U,
    Safety_Fault_e_REM = 0x01U,
    Safety_Fault_e_OC = 0x02U,
    Safety_Fault_e_OT = 0x04U
} Safety_Fault_e;

/**
 * @brief  Run all safety checks in one call.
 * @param  isBipolar Set true when operating in bipolar mode.
 * @retval 0 on success, error code otherwise
 */
Safety_Fault_e safetyCheck(bool isBipolar);

/**
 * @brief  Return true if the REM electrode contact is acceptable.
 * @param  isBipolar Set true when operating in bipolar mode.
 * @retval true if REM is OK, false otherwise.
 */
bool safetyIsRemOk(bool isBipolar);

/**
 * @brief  Return true if an overcurrent condition is detected.
 * @param  None
 * @retval true if overcurrent is detected, false otherwise.
 */
bool safetyIsOvercurrent(void);

/**
 * @brief  Return true if a thermal overload is detected.
 * @param  None
 * @retval true if overtemperature is detected, false otherwise.
 */
bool safetyIsOvertemp(void);

#ifdef __cplusplus
}
#endif

#endif /* SAFETY_H_ */