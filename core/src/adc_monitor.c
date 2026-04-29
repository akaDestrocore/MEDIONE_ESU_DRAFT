/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
 * @file   adc_monitor.c
 * @brief  ADC1 scan and power/DAC control.
 */

#include "adc_monitor.h"

static ADC_HandleTypeDef* gAdc = NULL;
static DAC_HandleTypeDef* gDac = NULL;

static AdcMonitor_Data_t gAdcData = {0};
static uint32_t gDacValue = 0U;
static uint16_t gPowerDw = 0U;

/**
 * @brief  Estimate output power in deci-watts from ADC readings.
 * @retval Power estimate in deci-watts.
 */
static uint16_t adcMonitor_computePowerDw(void) {
    // Scale factors — placeholder, tune to actual hardware
    const uint32_t voltageScaleMv = 100U;   // 1 ADC count -> 100 mV
    const uint32_t currentScaleMa = 20U;     // 1 ADC count -> 20 mA

    uint32_t voltageMv = ((uint32_t)gAdcData.raw[ADC_MONITOR_CH__VOLTAGE] * voltageScaleMv);
    uint32_t currentMa = ((uint32_t)gAdcData.raw[ADC_MONITOR_CH__CURRENT1] * currentScaleMa);

    /* P_mW = V_mV * I_mA / 2000 for the sine approximation */
    uint32_t powerMw = (voltageMv * currentMa) / 2000000UL;

    return (uint16_t)(powerMw / 100U);
}

/**
 * @brief  Initialise ADC monitor module.
 * @param  pHadc  ADC1 handle.
 * @param  pHdac  DAC handle.
 * @retval 0 on success, error code otherwise
 */
void adcMonitor_init(ADC_HandleTypeDef* pHadc, DAC_HandleTypeDef* pHdac) {
    gAdc = pHadc;
    gDac = pHdac;
    gDacValue = 0U;
    gPowerDw = 0U;

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
 * @retval 0 on success, error code otherwise
 */
void adcMonitor_scan(void) {
    uint8_t channelIndex = 0U;

    if (NULL != gAdc) {
        for (channelIndex = 0U; channelIndex < ADC_MONITOR_CH__COUNT; channelIndex++) {
            if (HAL_ADC_PollForConversion(gAdc, 2U) == HAL_OK) {
                gAdcData.raw[channelIndex] = (uint16_t)HAL_ADC_GetValue(gAdc);
            }
        }
    }
}

/**
 * @brief  Return a snapshot of the latest raw ADC values.
 * @retval Pointer to the latest ADC data snapshot.
 */
const AdcMonitor_Data_t* adcMonitor_getData(void) {
    return &gAdcData;
}

/**
 * @brief  Run closed-loop power controller.
 * @param  targetW  Desired output power in watts.
 * @retval 0 on success, error code otherwise
 */
void adcMonitor_powerLoop(uint16_t targetW) {
    int32_t errorDw = 0;
    int32_t delta = 0;
    int32_t newValue = 0;
    uint16_t targetDw = 0U;

    gPowerDw = adcMonitor_computePowerDw();

    targetDw = (uint16_t)(targetW * 10U);
    errorDw = (int32_t)targetDw - (int32_t)gPowerDw;

    /* Proportional controller */
    delta = (errorDw * ADC_MONITOR_KP_NUM) / ADC_MONITOR_KP_DEN;

    newValue = (int32_t)gDacValue + delta;
    if (newValue < 0) {
        newValue = 0;
    }
    else if (newValue > (int32_t)ADC_MONITOR_DAC_MAX) {
        newValue = (int32_t)ADC_MONITOR_DAC_MAX;
    }
    else {
        // No action required
    }

    gDacValue = (uint32_t)newValue;

    if (NULL != gDac) {
        (void)HAL_DAC_SetValue(gDac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, gDacValue);
    }
}

/**
 * @brief  Return the last computed output power in deci-watts.
 * @retval Output power in deci-watts.
 */
uint16_t adcMonitor_getPowerDw(void) {
    return gPowerDw;
}

/**
 * @brief  Force DAC to zero.
 * @retval 0 on success, error code otherwise
 */
void adcMonitor_dacZero(void) {
    gDacValue = 0U;
    gPowerDw = 0U;

    if (NULL != gDac) {
        (void)HAL_DAC_SetValue(gDac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, 0U);
    }
}

/**
 * @brief  Return true if REM is OK.
 * @param  isBipolar  Pass true when channel is bipolar.
 * @retval true if REM is acceptable, false otherwise.
 */
bool adcMonitor_isRemOk(bool isBipolar) {
    bool isRemOk = true;
    uint16_t remValue = 0U;

    if (false == isBipolar) {
        remValue = gAdcData.raw[ADC_MONITOR_CH__REM];
        isRemOk = ((remValue >= ADC_MONITOR_REM_ADC_MIN) && (remValue <= ADC_MONITOR_REM_ADC_MAX));
    }

    return isRemOk;
}

/**
 * @brief  Return true if any current sensor exceeds the threshold.
 * @retval true if overcurrent is detected, false otherwise.
 */
bool adcMonitor_isOvercurrent(void) {
    return ((gAdcData.raw[ADC_MONITOR_CH__CURRENT1] > ADC_MONITOR_OVERCURRENT_THR) ||
            (gAdcData.raw[ADC_MONITOR_CH__CURRENT2] > ADC_MONITOR_OVERCURRENT_THR));
}

/**
 * @brief  Return true if the heat-sink temperature exceeds the threshold.
 * @retval true if overtemperature is detected, false otherwise.
 */
bool adcMonitor_isOvertemp(void) {
    return (gAdcData.raw[ADC_MONITOR_CH__TEMP] > ADC_MONITOR_OVERTEMP_THR);
}