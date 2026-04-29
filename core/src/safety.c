/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   safety.c
 * @brief  Safety interlock — delegates to adc_monitor.
 */

#include "safety.h"

/**
 * @brief  Run all safety checks in one call.
 * @param  isBipolar Set true when operating in bipolar mode.
 * @retval 0 on success, error code otherwise
 */
Safety_Fault_e safetyCheck(bool isBipolar)
{
    Safety_Fault_e faults = Safety_Fault_e_OK;

    if (false == adcMonitor_isRemOk(isBipolar)) {
        faults = (Safety_Fault_e)(faults | Safety_Fault_e_REM);
    }

    if (true == adcMonitor_isOvercurrent()) {
        faults = (Safety_Fault_e)(faults | Safety_Fault_e_OC);
    }

    if (true == adcMonitor_isOvertemp()) {
        faults = (Safety_Fault_e)(faults | Safety_Fault_e_OT);
    }

    return faults;
}

/**
 * @brief  Return true if the REM electrode contact is acceptable.
 * @param  isBipolar Set true when operating in bipolar mode.
 * @retval true if REM is OK, false otherwise.
 */
bool safetyIsRemOk(bool isBipolar)
{
    return adcMonitor_isRemOk(isBipolar);
}

/**
 * @brief  Return true if an overcurrent condition is detected.
 * @param  None
 * @retval true if overcurrent is detected, false otherwise.
 */
bool safetyIsOvercurrent(void)
{
    return adcMonitor_isOvercurrent();
}

/**
 * @brief  Return true if a thermal overload is detected.
 * @param  None
 * @retval true if overtemperature is detected, false otherwise.
 */
bool safetyIsOvertemp(void)
{
    return adcMonitor_isOvertemp();
}