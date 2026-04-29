/**
 * @file   adc_monitor.c
 * @brief  ADC1 scan and power/DAC control.
 */

#include "adc_monitor.h"

static ADC_HandleTypeDef *_hadc  = NULL;
static DAC_HandleTypeDef *_hdac  = NULL;

static ADCMon_Data_t  _data      = {0};
static uint32_t       _dac_val   = 0U;
static uint16_t       _power_dw  = 0U;

/* ---------------------------------------------------------------- */

void adc_monitor_init(ADC_HandleTypeDef *hadc, DAC_HandleTypeDef *hdac) {
    _hadc = hadc;
    _hdac = hdac;
    _dac_val  = 0U;
    _power_dw = 0U;

    /* Start continuous scan — results are polled in adc_monitor_scan() */
    HAL_ADC_Start(_hadc);

    /* Initialise DAC channel 1 at zero */
    HAL_DAC_SetValue(_hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U);
    HAL_DAC_Start(_hdac, DAC_CHANNEL_1);
}

void adc_monitor_scan(void) {
    for (uint8_t ch = 0; ch < ADCMON_CH_COUNT; ch++) {
        if (HAL_ADC_PollForConversion(_hadc, 2U) == HAL_OK) {
            _data.raw[ch] = (uint16_t)HAL_ADC_GetValue(_hadc);
        }
    }
}

const ADCMon_Data_t *adc_monitor_get_data(void) {
    return &_data;
}

/**
 * @brief  Estimate RMS power from voltage and current ADC readings.
 *
 *  Hardware-specific note:
 *    The voltage sense pin (PA1) is connected to the output via a
 *    resistive divider; the current sense pin (PA0) is connected via
 *    an op-amp driven by a current shunt.  Both produce a peak-rectified
 *    signal, so the reading is proportional to V_peak and I_peak.
 *
 *    P_rms = (V_peak × I_peak) / 2  [for a pure sine wave]
 *
 *    The constants VOLTAGE_SCALE and CURRENT_SCALE below convert raw
 *    ADC counts to millivolts and milliamps respectively.
 *    *** Calibrate these to your actual hardware! ***
 */
static uint16_t _compute_power_dw(void) {
    /* Scale factors — PLACEHOLDER — tune to actual divider / shunt */
    const uint32_t VOLTAGE_SCALE = 100U;   // 1 ADC count → 100 mV (example)
    const uint32_t CURRENT_SCALE =  20U;   // 1 ADC count →  20 mA (example)

    uint32_t v_mv = ((uint32_t)_data.raw[ADCMON_CH_VOLTAGE]  * VOLTAGE_SCALE);
    uint32_t i_ma = ((uint32_t)_data.raw[ADCMON_CH_CURRENT1] * CURRENT_SCALE);

    /* P_mW = V_mv * I_ma / 2000  (sine rms: P = Vp*Ip/2) */
    uint32_t p_mw = (v_mv * i_ma) / 2000000UL;

    /* Return deci-Watts (W × 10) */
    return (uint16_t)(p_mw / 100U);
}

void adc_monitor_power_loop(uint16_t target_w) {
    _power_dw = _compute_power_dw();

    uint16_t target_dw = target_w * 10U;
    int32_t  error_dw  = (int32_t)target_dw - (int32_t)_power_dw;

    /* Proportional controller */
    int32_t delta = (error_dw * ADCMON_KP_NUM) / ADCMON_KP_DEN;

    int32_t new_val = (int32_t)_dac_val + delta;
    if (new_val < 0)               new_val = 0;
    if (new_val > ADCMON_DAC_MAX)  new_val = (int32_t)ADCMON_DAC_MAX;

    _dac_val = (uint32_t)new_val;
    HAL_DAC_SetValue(_hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, _dac_val);
}

uint16_t adc_monitor_get_power_dw(void) {
    return _power_dw;
}

void adc_monitor_dac_zero(void) {
    _dac_val  = 0U;
    _power_dw = 0U;
    HAL_DAC_SetValue(_hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U);
}

bool adc_monitor_rem_ok(bool bipolar) {
    if (bipolar) return true;   // no REM plate in bipolar mode
    uint16_t rem = _data.raw[ADCMON_CH_REM];
    return (rem >= ADCMON_REM_ADC_MIN && rem <= ADCMON_REM_ADC_MAX);
}

bool adc_monitor_overcurrent(void) {
    return (_data.raw[ADCMON_CH_CURRENT1] > ADCMON_OVERCURRENT_THR ||
            _data.raw[ADCMON_CH_CURRENT2] > ADCMON_OVERCURRENT_THR);
}

bool adc_monitor_overtemp(void) {
    return (_data.raw[ADCMON_CH_TEMP] > ADCMON_OVERTEMP_THR);
}