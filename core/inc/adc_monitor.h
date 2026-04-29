/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
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
 *  Closed-loop proportional controller runs in adcMonitor_powerLoop().
 */

#ifndef ADC_MONITOR_H_
#define ADC_MONITOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

/* ADC channel indices inside the scan sequence */
#define ADC_MONITOR_CH__CURRENT1        0U   /* PA0 — primary current  */
#define ADC_MONITOR_CH__VOLTAGE         1U   /* PA1 — output voltage   */
#define ADC_MONITOR_CH__CURRENT2        2U   /* PA2 — neutral current  */
#define ADC_MONITOR_CH__REM             3U   /* PA3 — REM impedance    */
#define ADC_MONITOR_CH__TEMP            4U   /* PB0 — heat-sink NTC    */
#define ADC_MONITOR_CH__COUNT           5U

/* ADC reference and full-scale */
#define ADC_MONITOR_VREF_MV          3300U
#define ADC_MONITOR_FULLSCALE        4095U

/* Thresholds — tune to actual hardware */
#define ADC_MONITOR_REM_ADC_MIN       200U   /* below → plate disconnected */
#define ADC_MONITOR_REM_ADC_MAX      3800U   /* above → plate short        */
#define ADC_MONITOR_OVERCURRENT_THR  3900U   /* raw 12-bit count           */
#define ADC_MONITOR_OVERTEMP_THR     3500U   /* raw 12-bit count (NTC)     */

/* Proportional power-control gain: delta_dac = error_dW × Kp */
#define ADC_MONITOR_KP_NUM             10
#define ADC_MONITOR_KP_DEN              1    /* Kp = 10 / 1 */

/* DAC limits */
#define ADC_MONITOR_DAC_MAX          4095U

/**
 * @brief  ADC raw value snapshot.
 */
typedef struct
{
    uint16_t raw[ADC_MONITOR_CH__COUNT];
} AdcMonitor_Data_t;

/**
 * @brief  Initialise ADC monitor module.
 * @param  pHadc  ADC1 handle.
 * @param  pHdac  DAC handle.
 */
void adcMonitor_init(ADC_HandleTypeDef* pHadc, DAC_HandleTypeDef* pHdac);

/**
 * @brief  Read one complete scan cycle from ADC1.
 */
void adcMonitor_scan(void);

/**
 * @brief  Return a snapshot of the latest raw ADC values.
 * @retval Pointer to the latest ADC data snapshot.
 */
const AdcMonitor_Data_t* adcMonitor_getData(void);

/**
 * @brief  Run closed-loop power controller.
 * @param  targetW  Desired output power in watts.
 */
void adcMonitor_powerLoop(uint16_t targetW);

/**
 * @brief  Return the last computed output power in deci-watts.
 * @retval Output power in deci-watts.
 */
uint16_t adcMonitor_getPowerDw(void);

/**
 * @brief  Force DAC to zero.
 */
void adcMonitor_dacZero(void);

/**
 * @brief  Return true if REM is OK.
 * @param  isBipolar  Pass true when channel is bipolar.
 * @retval true if REM is acceptable, false otherwise.
 */
bool adcMonitor_isRemOk(bool isBipolar);

/**
 * @brief  Return true if any current sensor exceeds the threshold.
 * @retval true if overcurrent is detected, false otherwise.
 */
bool adcMonitor_isOvercurrent(void);

/**
 * @brief  Return true if the heat-sink temperature exceeds the threshold.
 * @retval true if overtemperature is detected, false otherwise.
 */
bool adcMonitor_isOvertemp(void);

#ifdef __cplusplus
}
#endif

#endif /* ADC_MONITOR_H_ */