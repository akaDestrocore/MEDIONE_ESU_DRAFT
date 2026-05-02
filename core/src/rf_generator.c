/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   rf_generator.c
 * @brief  RF carrier generation — timer reconfiguration per mode.
 *
 * @details
 *  Relay/mux routing is now fully delegated to relay.c.  Each
 *  rfGen_configure*() call first invokes relay_apply() (which writes
 *  all relay/mux GPIOs atomically and starts a 200 ms settle timer),
 *  then reconfigures the relevant timer.  The RF amplifier enable
 *  GPIO is driven by rfGen_enable*() — the caller (app_fsm) must
 *  not call those until relay_isSettled() returns true.
 */

#include "rf_generator.h"

/* -------------------------------------------------------------------------- */
/* GPIO helpers — amp / audio enables use BSRR (no RMW)                      */
/* -------------------------------------------------------------------------- */

#define GPIO_SET(pPort, pin)    ((pPort)->BSRR = (uint32_t)(pin))
#define GPIO_CLEAR(pPort, pin)  ((pPort)->BSRR = ((uint32_t)(pin) << 16U))

#define CUT_EN_PORT     GPIOE
#define CUT_EN_PIN      GPIO_PIN_2
#define COAG_EN_PORT    GPIOE
#define COAG_EN_PIN     GPIO_PIN_3
#define BIPO_EN_PORT    GPIOE
#define BIPO_EN_PIN     GPIO_PIN_7
#define AUDIO_EN_PORT   GPIOB
#define AUDIO_EN_PIN    GPIO_PIN_1

#define LED_CUT_PORT    GPIOC
#define LED_CUT_PIN     GPIO_PIN_13
#define LED_COAG_PORT   GPIOC
#define LED_COAG_PIN    GPIO_PIN_2

/* -------------------------------------------------------------------------- */
/* Timer constants                                                             */
/* -------------------------------------------------------------------------- */

#define TIM4_PSC_500KHZ     20U
#define TIM4_ARR_500KHZ     7U
#define TIM4_PSC_400KHZ     20U
#define TIM4_ARR_400KHZ     9U
#define TIM4_PSC_380KHZ     21U
#define TIM4_ARR_380KHZ     9U
#define TIM4_PSC_350KHZ     23U
#define TIM4_ARR_350KHZ     9U

#define TIM13_PSC_400KHZ    20U
#define TIM13_ARR_400KHZ    9U
#define TIM13_CCR_SOFT      4U
#define TIM13_PSC_500KHZ    16U
#define TIM13_ARR_500KHZ    9U
#define TIM13_CCR_HALF      5U

#define TIM14_PSC_727KHZ    11U
#define TIM14_ARR_727KHZ    9U
#define TIM14_CCR_LOW       1U
#define TIM14_PSC_400KHZ    20U
#define TIM14_ARR_400KHZ    9U
#define TIM14_PSC_500KHZ    16U
#define TIM14_ARR_500KHZ    9U

#define TIM9_PSC            209U
#define TIM9_ARR_CUT        499U
#define TIM9_ARR_COAG       799U

#define BLEND1_ON           7U
#define BLEND1_TOTAL        20U
#define BLEND2_ON           5U
#define BLEND2_TOTAL        22U
#define BLEND3_ON           3U
#define BLEND3_TOTAL        20U

#define BIPOF_ON            5U
#define BIPOF_TOTAL         22U

/* -------------------------------------------------------------------------- */
/* Module-private state                                                        */
/* -------------------------------------------------------------------------- */

static RFGen_Timers_t gTimers = {0};
RFGen_BlendState_t gBlendState = {0};
static volatile uint32_t gCutCcrFull = 0U;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Set timer prescaler and auto-reload, force an update event.
 * @param  pHtim  Timer handle.
 * @param  psc    Prescaler value.
 * @param  arr    Auto-reload value.
 * @retval None
 */
static void rfGen_setTimerFrequency(TIM_HandleTypeDef *pHtim,
                                    uint32_t           psc,
                                    uint32_t           arr) {
    if (NULL != pHtim) {
        pHtim->Instance->CR1 &= (uint32_t)~TIM_CR1_CEN;
        pHtim->Instance->PSC  = psc;
        pHtim->Instance->ARR  = arr;
        pHtim->Instance->EGR  = TIM_EGR_UG;
        pHtim->Instance->SR  &= (uint32_t)~TIM_SR_UIF;
        pHtim->Instance->CR1 |= TIM_CR1_CEN;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Initialise RF generator module.
 * @param  pTimers  Pointer to a fully populated RFGen_Timers_t structure.
 * @retval None
 */
void rfGen_init(const RFGen_Timers_t *pTimers) {
    if (NULL != pTimers) {
        gTimers = *pTimers;
    }

    (void)memset(&gBlendState, 0, sizeof(gBlendState));
    gBlendState.isActive = false;
    gCutCcrFull          = 0U;

    relay_allOff();

    if (NULL != gTimers.pHtimCut) {
        (void)HAL_TIM_PWM_Start(gTimers.pHtimCut, TIM_CHANNEL_1);
        gTimers.pHtimCut->Instance->CCR1 = 0U;
    }

    if (NULL != gTimers.pHtimCoag) {
        (void)HAL_TIM_PWM_Start(gTimers.pHtimCoag, TIM_CHANNEL_1);
        gTimers.pHtimCoag->Instance->CCR1 = 0U;
    }

    if (NULL != gTimers.pHtimCoag2) {
        (void)HAL_TIM_PWM_Start(gTimers.pHtimCoag2, TIM_CHANNEL_1);
        gTimers.pHtimCoag2->Instance->CCR1 = 0U;
    }

    if (NULL != gTimers.pHtimBlend) {
        (void)HAL_TIM_Base_Start_IT(gTimers.pHtimBlend);
    }

    if (NULL != gTimers.pHtimPolyTick) {
        (void)HAL_TIM_Base_Start_IT(gTimers.pHtimPolyTick);
    }

    if (NULL != gTimers.pHtimPolySlow) {
        (void)HAL_TIM_PWM_Start(gTimers.pHtimPolySlow, TIM_CHANNEL_1);
        gTimers.pHtimPolySlow->Instance->CCR1 = 0U;
    }
}

/**
 * @brief  Configure timers and relay routing for the requested CUT mode.
 * @param  mode  CUT mode selection.
 * @retval None
 */
void rfGen_configureCut(AppDefs_CutMode_e mode) {
    gBlendState.isActive = false;
    gBlendState.counter  = 0U;

    // Apply relay routing first; caller must await relay_isSettled()
    relay_apply(RELAY_CFG_CUT_MONO);

    switch (mode) {
        case AppDefs_CutMode_Pure: {
            rfGen_setTimerFrequency(gTimers.pHtimCut, TIM4_PSC_500KHZ, TIM4_ARR_500KHZ);
            gCutCcrFull = (TIM4_ARR_500KHZ + 1U) / 2U;
            if (NULL != gTimers.pHtimCut) {
                gTimers.pHtimCut->Instance->CCR1 = gCutCcrFull;
            }
            break;
        }

        case AppDefs_CutMode_Blend1: {
            rfGen_setTimerFrequency(gTimers.pHtimCut, TIM4_PSC_400KHZ, TIM4_ARR_400KHZ);
            gCutCcrFull              = (TIM4_ARR_400KHZ + 1U) / 2U;
            if (NULL != gTimers.pHtimCut) {
                gTimers.pHtimCut->Instance->CCR1 = gCutCcrFull;
            }
            gBlendState.onCount    = BLEND1_ON;
            gBlendState.totalCount = BLEND1_TOTAL;
            gBlendState.counter    = 0U;
            gBlendState.isActive   = true;
            break;
        }

        case AppDefs_CutMode_Blend2: {
            rfGen_setTimerFrequency(gTimers.pHtimCut, TIM4_PSC_380KHZ, TIM4_ARR_380KHZ);
            gCutCcrFull              = (TIM4_ARR_380KHZ + 1U) / 2U;
            if (NULL != gTimers.pHtimCut) {
                gTimers.pHtimCut->Instance->CCR1 = gCutCcrFull;
            }
            gBlendState.onCount    = BLEND2_ON;
            gBlendState.totalCount = BLEND2_TOTAL;
            gBlendState.counter    = 0U;
            gBlendState.isActive   = true;
            break;
        }

        case AppDefs_CutMode_Blend3: {
            rfGen_setTimerFrequency(gTimers.pHtimCut, TIM4_PSC_380KHZ, TIM4_ARR_380KHZ);
            gCutCcrFull              = (TIM4_ARR_380KHZ + 1U) / 2U;
            if (NULL != gTimers.pHtimCut) {
                gTimers.pHtimCut->Instance->CCR1 = gCutCcrFull;
            }
            gBlendState.onCount    = BLEND3_ON;
            gBlendState.totalCount = BLEND3_TOTAL;
            gBlendState.counter    = 0U;
            gBlendState.isActive   = true;
            break;
        }

        case AppDefs_CutMode_Polypectomy: {
            rfGen_setTimerFrequency(gTimers.pHtimCut, TIM4_PSC_350KHZ, TIM4_ARR_350KHZ);
            gCutCcrFull = (TIM4_ARR_350KHZ + 1U) / 2U;
            if (NULL != gTimers.pHtimCut) {
                gTimers.pHtimCut->Instance->CCR1 = gCutCcrFull;
            }
            if (NULL != gTimers.pHtimPolySlow) {
                gTimers.pHtimPolySlow->Instance->CCR1 =
                    (gTimers.pHtimPolySlow->Instance->ARR + 1U) / 2U;
            }
            break;
        }

        default: {
            break;
        }
    }
}

/**
 * @brief  Configure timers and relay routing for the requested COAG mode.
 * @param  mode  COAG mode selection.
 * @retval None
 */
void rfGen_configureCoag(AppDefs_CoagMode_e mode) {
    gBlendState.isActive = false;
    gBlendState.counter  = 0U;

    // Select relay routing before touching timers
    switch (mode) {
        case AppDefs_CoagMode_Spray:  relay_apply(RELAY_CFG_COAG_SPRAY);   break;
        case AppDefs_CoagMode_Argon:  relay_apply(RELAY_CFG_COAG_ARGON);   break;
        case AppDefs_CoagMode_Contact: relay_apply(RELAY_CFG_COAG_CONTACT); break;
        case AppDefs_CoagMode_Soft:   // fall-through
        default:                      relay_apply(RELAY_CFG_COAG_SOFT);    break;
    }

    switch (mode) {
        case AppDefs_CoagMode_Soft: {
            rfGen_setTimerFrequency(gTimers.pHtimCoag, TIM13_PSC_400KHZ, TIM13_ARR_400KHZ);
            if (NULL != gTimers.pHtimCoag) {
                gTimers.pHtimCoag->Instance->CCR1 = TIM13_CCR_SOFT;
            }
            if (NULL != gTimers.pHtimCoag2) {
                gTimers.pHtimCoag2->Instance->CCR1 = 0U;
            }
            break;
        }

        case AppDefs_CoagMode_Contact: {
            rfGen_setTimerFrequency(gTimers.pHtimCoag2, TIM14_PSC_727KHZ, TIM14_ARR_727KHZ);
            if (NULL != gTimers.pHtimCoag2) {
                gTimers.pHtimCoag2->Instance->CCR1 = TIM14_CCR_LOW;
            }
            if (NULL != gTimers.pHtimCoag) {
                gTimers.pHtimCoag->Instance->CCR1 = 0U;
            }
            break;
        }

        case AppDefs_CoagMode_Spray:
        case AppDefs_CoagMode_Argon: {
            rfGen_setTimerFrequency(gTimers.pHtimCoag2, TIM14_PSC_400KHZ, TIM14_ARR_400KHZ);
            if (NULL != gTimers.pHtimCoag2) {
                gTimers.pHtimCoag2->Instance->CCR1 = TIM14_CCR_LOW;
            }
            if (NULL != gTimers.pHtimCoag) {
                gTimers.pHtimCoag->Instance->CCR1 = 0U;
            }
            break;
        }

        default: {
            break;
        }
    }
}

/**
 * @brief  Configure timers and relay routing for a Bipolar CUT sub-mode.
 * @param  mode  Bipolar CUT mode selection.
 * @retval None
 */
void rfGen_configureBipolarCut(AppDefs_BipolarCutMode_e mode) {
    gBlendState.isActive = false;
    gBlendState.counter  = 0U;

    relay_apply(RELAY_CFG_BIPOLAR_CUT);

    rfGen_setTimerFrequency(gTimers.pHtimCoag, TIM13_PSC_500KHZ, TIM13_ARR_500KHZ);

    if (AppDefs_BipolarCutMode_Standard == mode) {
        if (NULL != gTimers.pHtimCoag) {
            gTimers.pHtimCoag->Instance->CCR1 = TIM13_CCR_HALF;
        }
    } else {
        if (NULL != gTimers.pHtimCoag) {
            gTimers.pHtimCoag->Instance->CCR1 = TIM13_CCR_HALF;
        }
        gCutCcrFull              = TIM13_CCR_HALF;
        gBlendState.onCount    = BLEND1_ON;
        gBlendState.totalCount = BLEND1_TOTAL;
        gBlendState.counter    = 0U;
        gBlendState.isActive   = true;
    }
}

/**
 * @brief  Configure timers and relay routing for a Bipolar COAG sub-mode.
 * @param  mode  Bipolar COAG mode selection.
 * @retval None
 */
void rfGen_configureBipolarCoag(AppDefs_BipolarCoagMode_e mode) {
    gBlendState.isActive = false;
    gBlendState.counter  = 0U;

    relay_apply(RELAY_CFG_BIPOLAR_COAG);

    rfGen_setTimerFrequency(gTimers.pHtimCoag, TIM13_PSC_500KHZ, TIM13_ARR_500KHZ);

    if (AppDefs_BipolarCoagMode_Forced == mode) {
        if (NULL != gTimers.pHtimCoag) {
            gTimers.pHtimCoag->Instance->CCR1 = TIM13_CCR_HALF;
        }
        gCutCcrFull              = TIM13_CCR_HALF;
        gBlendState.onCount    = BIPOF_ON;
        gBlendState.totalCount = BIPOF_TOTAL;
        gBlendState.counter    = 0U;
        gBlendState.isActive   = true;
    } else {
        if (NULL != gTimers.pHtimCoag) {
            gTimers.pHtimCoag->Instance->CCR1 = TIM13_CCR_HALF;
        }
    }
}

/**
 * @brief  Enable the RF output amplifier for the CUT path.
 * @retval None
 */
void rfGen_enableCut(void) {
    GPIO_SET(CUT_EN_PORT, CUT_EN_PIN);
    GPIO_SET(LED_CUT_PORT, LED_CUT_PIN);
}

/**
 * @brief  Disable the RF output amplifier for the CUT path.
 * @retval None
 */
void rfGen_disableCut(void) {
    gBlendState.isActive = false;

    if (NULL != gTimers.pHtimCut) {
        gTimers.pHtimCut->Instance->CCR1 = 0U;
    }
    if (NULL != gTimers.pHtimPolySlow) {
        gTimers.pHtimPolySlow->Instance->CCR1 = 0U;
    }

    GPIO_CLEAR(CUT_EN_PORT, CUT_EN_PIN);
    GPIO_CLEAR(LED_CUT_PORT, LED_CUT_PIN);
}

/**
 * @brief  Enable the RF output amplifier for the COAG path.
 * @retval None
 */
void rfGen_enableCoag(void) {
    GPIO_SET(COAG_EN_PORT, COAG_EN_PIN);
    GPIO_SET(LED_COAG_PORT, LED_COAG_PIN);
}

/**
 * @brief  Disable the RF output amplifier for the COAG path.
 * @retval None
 */
void rfGen_disableCoag(void) {
    gBlendState.isActive = false;

    if (NULL != gTimers.pHtimCoag) {
        gTimers.pHtimCoag->Instance->CCR1 = 0U;
    }
    if (NULL != gTimers.pHtimCoag2) {
        gTimers.pHtimCoag2->Instance->CCR1 = 0U;
    }

    GPIO_CLEAR(COAG_EN_PORT, COAG_EN_PIN);
    GPIO_CLEAR(LED_COAG_PORT, LED_COAG_PIN);
}

/**
 * @brief  Enable the RF output amplifier for the Bipolar path.
 * @retval None
 */
void rfGen_enableBipolar(void) {
    GPIO_SET(BIPO_EN_PORT, BIPO_EN_PIN);
}

/**
 * @brief  Disable the RF output amplifier for the Bipolar path.
 * @retval None
 */
void rfGen_disableBipolar(void) {
    gBlendState.isActive = false;

    if (NULL != gTimers.pHtimCoag) {
        gTimers.pHtimCoag->Instance->CCR1 = 0U;
    }

    GPIO_CLEAR(BIPO_EN_PORT, BIPO_EN_PIN);
}

/**
 * @brief  Disable all RF paths, reset blend modulator, and safe all relays.
 * @retval None
 */
void rfGen_disableAll(void) {
    gBlendState.isActive = false;
    gBlendState.counter  = 0U;

    GPIO_CLEAR(CUT_EN_PORT,   CUT_EN_PIN);
    GPIO_CLEAR(COAG_EN_PORT,  COAG_EN_PIN);
    GPIO_CLEAR(BIPO_EN_PORT,  BIPO_EN_PIN);
    GPIO_CLEAR(LED_CUT_PORT,  LED_CUT_PIN);
    GPIO_CLEAR(LED_COAG_PORT, LED_COAG_PIN);
    GPIO_CLEAR(AUDIO_EN_PORT, AUDIO_EN_PIN);

    if (NULL != gTimers.pHtimCut) {
        gTimers.pHtimCut->Instance->CCR1 = 0U;
    }
    if (NULL != gTimers.pHtimCoag) {
        gTimers.pHtimCoag->Instance->CCR1 = 0U;
    }
    if (NULL != gTimers.pHtimCoag2) {
        gTimers.pHtimCoag2->Instance->CCR1 = 0U;
    }
    if (NULL != gTimers.pHtimPolySlow) {
        gTimers.pHtimPolySlow->Instance->CCR1 = 0U;
    }
    if (NULL != gTimers.pHtimAudio) {
        (void)HAL_TIM_PWM_Stop(gTimers.pHtimAudio, TIM_CHANNEL_2);
    }

    relay_allOff();
}

/**
 * @brief  Start the audio buzzer.
 * @param  isCut  true for CUT tone, false for COAG tone.
 * @retval None
 */
void rfGen_audioStart(bool isCut) {
    uint32_t arr;
    uint32_t ccr;

    if (NULL == gTimers.pHtimAudio) {
        return;
    }

    arr = (true == isCut) ? TIM9_ARR_CUT : TIM9_ARR_COAG;
    ccr = (arr + 1U) / 2U;

    __HAL_TIM_SET_AUTORELOAD(gTimers.pHtimAudio, arr);
    __HAL_TIM_SET_COMPARE(gTimers.pHtimAudio, TIM_CHANNEL_2, ccr);

    GPIO_SET(AUDIO_EN_PORT, AUDIO_EN_PIN);
    (void)HAL_TIM_PWM_Start(gTimers.pHtimAudio, TIM_CHANNEL_2);
}

/**
 * @brief  Stop the audio buzzer.
 * @retval None
 */
void rfGen_audioStop(void) {
    if (NULL != gTimers.pHtimAudio) {
        (void)HAL_TIM_PWM_Stop(gTimers.pHtimAudio, TIM_CHANNEL_2);
    }
    GPIO_CLEAR(AUDIO_EN_PORT, AUDIO_EN_PIN);
}

/**
 * @brief  Blend envelope ISR callback — call from TIM2 ISR.
 * @retval None
 */
void rfGen_blendTickIsr(void) {
    if (false == gBlendState.isActive) {
        return;
    }
    if (NULL == gTimers.pHtimCut) {
        return;
    }

    gBlendState.counter++;
    if (gBlendState.counter >= gBlendState.totalCount) {
        gBlendState.counter = 0U;
    }

    if (gBlendState.counter < gBlendState.onCount) {
        gTimers.pHtimCut->Instance->CCR1 = gCutCcrFull;
    } else {
        gTimers.pHtimCut->Instance->CCR1 = 0U;
    }
}