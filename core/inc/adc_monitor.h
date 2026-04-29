/**
 * @file   adc_monitor.h
 * @brief  ADC1 continuous scan and output power calculation.
 *
 * @details
 *  ADC1 channel mapping (scan mode, 5 conversions):
 *    Rank 1 → CH0  PA0  Primary RF current sense    (I1)
 *    Rank 2 → CH1  PA1  RF output voltage sense     (V)
 *    Rank 3 → CH2  PA2  Secondary/neutral current   (I2)
 *    Rank 4 → CH3  PA3  REM impedance divider       (REM)
 *    Rank 5 → CH8  PB0  Heat-sink NTC thermistor    (TEMP)
 *
 *  Power estimation (sinusoidal approximation):
 *    P_rms [W] ≈ (V_peak_V × I_peak_A) / 2
 *
 *  The DAC on PA4 drives the RF amplifier gain.
 *  Closed-loop proportional controller runs in adc_monitor_power_loop().
 */

#ifndef _ADC_MONITOR_H
#define _ADC_MONITOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ADC channel indices inside the scan sequence */
#define ADCMON_CH_CURRENT1   0U   // PA0 — primary current
#define ADCMON_CH_VOLTAGE    1U   // PA1 — output voltage
#define ADCMON_CH_CURRENT2   2U   // PA2 — neutral current
#define ADCMON_CH_REM        3U   // PA3 — REM impedance
#define ADCMON_CH_TEMP       4U   // PB0 — heat-sink NTC
#define ADCMON_CH_COUNT      5U

/* ADC reference and full-scale */
#define ADCMON_VREF_MV      3300U
#define ADCMON_FULLSCALE    4095U

/* Thresholds — tune to actual hardware */
#define ADCMON_REM_ADC_MIN     200U   // below → plate disconnected
#define ADCMON_REM_ADC_MAX    3800U   // above → plate short
#define ADCMON_OVERCURRENT_THR 3900U  // raw 12-bit count
#define ADCMON_OVERTEMP_THR    3500U  // raw 12-bit count (NTC divider)

/* Proportional power-control gain:  delta_dac = error_dW * Kp */
#define ADCMON_KP_NUM   10
#define ADCMON_KP_DEN    1   // Kp = 10/1

/* DAC limits */
#define ADCMON_DAC_MAX  4095U

/**
 * @brief  ADC raw value snapshot (updated by adc_monitor_scan()).
 */
typedef struct {
    uint16_t raw[ADCMON_CH_COUNT];
} ADCMon_Data_t;

/**
 * @brief  Initialise ADC monitor module.
 * @param  hadc  ADC1 handle.
 * @param  hdac  DAC handle.
 */
void adc_monitor_init(ADC_HandleTypeDef *hadc, DAC_HandleTypeDef *hdac);

/**
 * @brief  Read one complete scan cycle from ADC1.
 *         Call every pass through the main loop.
 */
void adc_monitor_scan(void);

/**
 * @brief  Return a snapshot of the latest raw ADC values.
 */
const ADCMon_Data_t *adc_monitor_get_data(void);

/**
 * @brief  Run closed-loop power controller.
 *         Compares measured power to target_w and adjusts DAC.
 * @param  target_w  Desired output power in Watts.
 */
void adc_monitor_power_loop(uint16_t target_w);

/**
 * @brief  Return the last computed output power in deci-Watts (W × 10).
 */
uint16_t adc_monitor_get_power_dw(void);

/**
 * @brief  Force DAC to zero (output off).
 */
void adc_monitor_dac_zero(void);

/**
 * @brief  Return true if REM (return electrode monitoring) is OK.
 *         Always returns true in bipolar mode (no REM needed).
 * @param  bipolar  Pass true when channel is bipolar.
 */
bool adc_monitor_rem_ok(bool bipolar);

/**
 * @brief  Return true if any current sensor exceeds the overcurrent threshold.
 */
bool adc_monitor_overcurrent(void);

/**
 * @brief  Return true if the heat-sink temperature ADC exceeds threshold.
 */
bool adc_monitor_overtemp(void);

#ifdef __cplusplus
}
#endif

#endif /* _ADC_MONITOR_H */