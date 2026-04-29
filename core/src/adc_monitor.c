/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   adc_monitor.c
 * @brief  ADC1 scan and closed-loop power/DAC control.
 */

#include "adc_monitor.h"

static ADC_HandleTypeDef* gAdc = NULL;
static DAC_HandleTypeDef* gDac = NULL;

static AdcMonitor_Data_t gAdcData  = {0};
static uint32_t          gDacValue = 0U;
static uint16_t          gPowerDw  = 0U;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Estimate output power in deci-watts from ADC readings.
 * @details
 *  Sinusoidal approximation: P_rms ≈ (V_peak × I_peak) / 2
 *
 *  Scale factors are placeholder values — calibrate to actual hardware.
 *  To prevent uint32_t overflow the intermediate V×I product is computed
 *  as uint64_t before dividing back to a 32-bit result.
 *
 *  Worst-case intermediate: 4095 counts × 100 mV/cnt = 409,500 mV
 *                           4095 counts ×  20 mA/cnt =  81,900 mA
 *                           409,500 × 81,900 = ~33.5 × 10⁹  (overflows uint32!)
 *  With uint64: max = ~1.84 × 10¹⁹ — no overflow.
 * @retval Power estimate in deci-watts.
 */
static uint16_t adcMonitor_computePowerDw(void) {
    const uint32_t VOLTAGE_SCALE_MV = 100U; // 1 ADC count → 100 mV  (calibrate)
    const uint32_t CURRENT_SCALE_MA =  20U; // 1 ADC count →  20 mA  (calibrate)

    uint32_t voltageMv = (uint32_t)gAdcData.raw[ADC_MONITOR_CH__VOLTAGE]  * VOLTAGE_SCALE_MV;
    uint32_t currentMa = (uint32_t)gAdcData.raw[ADC_MONITOR_CH__CURRENT1] * CURRENT_SCALE_MA;

    // Use uint64_t to prevent overflow before dividing
    uint64_t powerMw64 = ((uint64_t)voltageMv * (uint64_t)currentMa) / 2000000ULL;

    // Convert mW → dW; cap at UINT16_MAX to keep the return type safe
    uint64_t powerDw64 = powerMw64 / 100ULL;
    if (powerDw64 > (uint64_t)UINT16_MAX) {
        powerDw64 = (uint64_t)UINT16_MAX;
    }

    return (uint16_t)powerDw64;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Initialise ADC monitor module.
 * @param  pHadc  ADC1 handle.
 * @param  pHdac  DAC handle.
 * @retval None
 */
void adcMonitor_init(ADC_HandleTypeDef *pHadc, DAC_HandleTypeDef *pHdac) {
    gAdc      = pHadc;
    gDac      = pHdac;
    gDacValue = 0U;
    gPowerDw  = 0U;

    if ((NULL != gAdc) && (NULL != gDac)) {
        // Start continuous scan — results are polled in adcMonitor_scan()
        (void)HAL_ADC_Start(gAdc);

        // Initialise DAC channel 1 at zero
        (void)HAL_DAC_SetValue(gDac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U);
        (void)HAL_DAC_Start(gDac, DAC_CHANNEL_1);
    }
}

/**
 * @brief  Read one complete scan cycle from ADC1.
 * @retval None
 */
void adcMonitor_scan(void) {
    uint8_t channelIndex;

    if (NULL == gAdc) {
        return;
    }

    for (channelIndex = 0U; channelIndex < ADC_MONITOR_CH__COUNT; channelIndex++) {
        if (HAL_OK == HAL_ADC_PollForConversion(gAdc, 2U)) {
            gAdcData.raw[channelIndex] = (uint16_t)HAL_ADC_GetValue(gAdc);
        }
    }
}

/**
 * @brief  Return a snapshot of the latest raw ADC values.
 * @param  None
 * @retval Pointer to the latest AdcMonitor_Data_t snapshot.
 */
const AdcMonitor_Data_t *adcMonitor_getData(void) {
    return &gAdcData;
}

/**
 * @brief  Run closed-loop proportional power controller.
 * @param  targetW  Desired output power in watts.
 * @retval None
 */
void adcMonitor_powerLoop(uint16_t targetW) {
    int32_t errorDw;
    int32_t delta;
    int32_t newValue;
    uint16_t targetDw;

    gPowerDw = adcMonitor_computePowerDw();

    targetDw = (uint16_t)(targetW * 10U);
    errorDw  = (int32_t)targetDw - (int32_t)gPowerDw;

    // Proportional controller
    delta    = (errorDw * ADC_MONITOR_KP_NUM) / ADC_MONITOR_KP_DEN;
    newValue = (int32_t)gDacValue + delta;

    if (newValue < 0) {
        newValue = 0;
    } else if (newValue > (int32_t)ADC_MONITOR_DAC_MAX) {
        newValue = (int32_t)ADC_MONITOR_DAC_MAX;
    } else {
        // Within range — no action required
    }

    gDacValue = (uint32_t)newValue;

    if (NULL != gDac) {
        (void)HAL_DAC_SetValue(gDac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, gDacValue);
    }
}

/**
 * @brief  Return the last computed output power in deci-watts.
 * @param  None
 * @retval Output power in deci-watts.
 */
uint16_t adcMonitor_getPowerDw(void) {
    return gPowerDw;
}

/**
 * @brief  Force DAC to zero and reset the power reading.
 * @param  None
 * @retval None
 */
void adcMonitor_dacZero(void) {
    gDacValue = 0U;
    gPowerDw  = 0U;

    if (NULL != gDac) {
        (void)HAL_DAC_SetValue(gDac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U);
    }
}

/**
 * @brief  Return true if the REM electrode impedance is acceptable.
 * @param  isBipolar  Pass true when channel is bipolar (REM not required).
 * @retval true if REM is OK, false if an alarm condition exists.
 */
bool adcMonitor_isRemOk(bool isBipolar) {
    bool     isRemOk  = true;
    uint16_t remValue;

    if (false == isBipolar) {
        remValue = gAdcData.raw[ADC_MONITOR_CH__REM];
        isRemOk  = ((remValue >= ADC_MONITOR_REM_ADC_MIN) &&
                    (remValue <= ADC_MONITOR_REM_ADC_MAX));
    }

    return isRemOk;
}

/**
 * @brief  Return true if any current sensor exceeds the overcurrent threshold.
 * @param  None
 * @retval true if overcurrent is detected, false otherwise.
 */
bool adcMonitor_isOvercurrent(void) {
    return ((gAdcData.raw[ADC_MONITOR_CH__CURRENT1] > ADC_MONITOR_OVERCURRENT_THR) ||
            (gAdcData.raw[ADC_MONITOR_CH__CURRENT2] > ADC_MONITOR_OVERCURRENT_THR));
}

/**
 * @brief  Return true if the heat-sink NTC reading exceeds the over-temperature threshold.
 * @param  None
 * @retval true if overtemperature is detected, false otherwise.
 */
bool adcMonitor_isOvertemp(void) {
    return (gAdcData.raw[ADC_MONITOR_CH__TEMP] > ADC_MONITOR_OVERTEMP_THR);
}