/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   rf_generator.h
 * @brief  RF carrier generation — timer reconfiguration per mode.
 *
 * @details
 *  Timer allocation (system clock 168 MHz, APB1/2 at 42 MHz -> timer at 84 MHz):
 *
 *  TIM4  CH1 -> PD12 : CUT carrier
 *      Pure Cut    : 500 kHz  (PSC=20, ARR=7)
 *      Blend1      : 400 kHz  (PSC=20, ARR=9)
 *      Blend2/3    : 380 kHz  (PSC=21, ARR=9)
 *      Polypectomy : 350 kHz  (PSC=23, ARR=9)
 *
 *  TIM13 CH1 -> PA6  : COAG Soft / Bipolar carriers
 *      Soft Coag   : 400 kHz, duty 39 %  (PSC=20, ARR=9, CCR=4)
 *      Bip.Std/AS  : 500 kHz, duty 50 %  (PSC=16, ARR=9, CCR=5)
 *
 *  TIM14 CH1 -> PA7  : COAG Contact / Spray / Argon / Bip.Forced
 *      Contact     : 727 kHz, duty ~10 % (PSC=11, ARR=9, CCR=1)
 *      Spray/Argon : 400 kHz, duty ~10 % (PSC=20, ARR=9, CCR=1)
 *      Bip.Forced  : 500 kHz, duty ~10 % (PSC=16, ARR=9, CCR=1)
 *
 *  TIM9  CH2 -> PE6  : Audio buzzer
 *  TIM2  CH1 -> PA5  : Blend envelope modulator (33 kHz / 27 kHz)
 *  TIM3  CH1 -> PC6  : Polypectomy slow on/off pulse (~1.5 Hz)
 *  TIM5  (no output) : Polypectomy cycle tick 100 Hz
 */

#ifndef RF_GENERATOR_H_
#define RF_GENERATOR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "app_defs.h"
#include "stm32f4xx_hal.h"

/**
 * @brief Timer handle bundle used by the RF generator module.
 */
typedef struct
{
    TIM_HandleTypeDef* pHtimCut;
    TIM_HandleTypeDef* pHtimCoag;
    TIM_HandleTypeDef* pHtimCoag2;
    TIM_HandleTypeDef* pHtimAudio;
    TIM_HandleTypeDef* pHtimBlend;
    TIM_HandleTypeDef* pHtimPolySlow;
    TIM_HandleTypeDef* pHtimPolyTick;
} RFGen_Timers_t;

/**
 * @brief Blend modulation state shared with the ISR.
 */
typedef struct
{
    volatile uint8_t onCount;
    volatile uint8_t totalCount;
    volatile uint8_t counter;
    volatile bool isActive;
} RFGen_BlendState_t;

extern RFGen_BlendState_t gBlendState;

/**
 * @brief Initialise RF generator module.
 * @param pTimers Pointer to a fully populated RFGen_Timers_t structure.
 * @retval 0 on success, error code otherwise
 */
void rfGen_init(const RFGen_Timers_t* pTimers);

/**
 * @brief Configure all timers for the requested CUT mode.
 * @param mode CUT mode selection.
 * @retval 0 on success, error code otherwise
 */
void rfGen_configureCut(AppDefs_CutMode_e mode);

/**
 * @brief Configure all timers for the requested COAG mode.
 * @param mode COAG mode selection.
 * @retval 0 on success, error code otherwise
 */
void rfGen_configureCoag(AppDefs_CoagMode_e mode);

/**
 * @brief Configure timers for a Bipolar CUT sub-mode.
 * @param mode Bipolar CUT mode selection.
 * @retval 0 on success, error code otherwise
 */
void rfGen_configureBipolarCut(AppDefs_BipolarCutMode_e mode);

/**
 * @brief Configure timers for a Bipolar COAG sub-mode.
 * @param mode Bipolar COAG mode selection.
 * @retval 0 on success, error code otherwise
 */
void rfGen_configureBipolarCoag(AppDefs_BipolarCoagMode_e mode);

/**
 * @brief Enable the RF output amplifier for the CUT path.
 * @retval 0 on success, error code otherwise
 */
void rfGen_enableCut(void);

/**
 * @brief Disable the RF output amplifier for the CUT path.
 * @retval 0 on success, error code otherwise
 */
void rfGen_disableCut(void);

/**
 * @brief Enable the RF output amplifier for the COAG path.
 * @retval 0 on success, error code otherwise
 */
void rfGen_enableCoag(void);

/**
 * @brief Disable the RF output amplifier for the COAG path.
 * @retval 0 on success, error code otherwise
 */
void rfGen_disableCoag(void);

/**
 * @brief Enable the RF output amplifier for the Bipolar path.
 * @retval 0 on success, error code otherwise
 */
void rfGen_enableBipolar(void);

/**
 * @brief Disable the RF output amplifier for the Bipolar path.
 * @retval 0 on success, error code otherwise
 */
void rfGen_disableBipolar(void);

/**
 * @brief Disable all RF paths and reset the blend modulator.
 * @retval 0 on success, error code otherwise
 */
void rfGen_disableAll(void);

/**
 * @brief Start the audio buzzer.
 * @param isCut true for CUT tone, false for COAG tone.
 * @retval 0 on success, error code otherwise
 */
void rfGen_audioStart(bool isCut);

/**
 * @brief Stop the audio buzzer.
 * @retval 0 on success, error code otherwise
 */
void rfGen_audioStop(void);

/**
 * @brief Blend envelope ISR callback.
 * @retval 0 on success, error code otherwise
 */
void rfGen_blendTickIsr(void);

#ifdef __cplusplus
}
#endif

#endif /* RF_GENERATOR_H_ */