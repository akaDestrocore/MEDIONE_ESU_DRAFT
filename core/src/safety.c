/**
 * @file   safety.c
 * @brief  Safety interlock — delegates to adc_monitor.
 */

#include "safety.h"

Safety_Fault_e safety_check(bool bipolar) {
    Safety_Fault_e faults = SAFETY_OK;

    if (!adc_monitor_rem_ok(bipolar)) faults |= SAFETY_FAULT_REM;
    if (adc_monitor_overcurrent())    faults |= SAFETY_FAULT_OC;
    if (adc_monitor_overtemp())       faults |= SAFETY_FAULT_OT;

    return faults;
}

bool safety_rem_ok(bool bipolar)    { return adc_monitor_rem_ok(bipolar); }
bool safety_overcurrent(void)       { return adc_monitor_overcurrent(); }
bool safety_overtemp(void)          { return adc_monitor_overtemp(); }