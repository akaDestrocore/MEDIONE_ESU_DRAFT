/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file    app_fsm.h
 * @brief   ESU state machine and RF output control interface
 *
 * @details
 * ...
 */

#ifndef _APP_FSM_H
#define _APP_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include "app_defs.h"
#include "stm32f4xx_hal.h"

/* ----------------------------------------------------------------
 * GPIO helpers
 * ----------------------------------------------------------------*/
/**
 * @brief Output enables
 */
#define ESU_GPIO_CUT_EN_PORT     GPIOE
#define ESU_GPIO_CUT_EN_PIN      GPIO_PIN_2
#define ESU_GPIO_COAG_EN_PORT    GPIOE
#define ESU_GPIO_COAG_EN_PIN     GPIO_PIN_3
#define ESU_GPIO_BIPO_EN_PORT    GPIOE
#define ESU_GPIO_BIPO_EN_PIN     GPIO_PIN_7
#define ESU_GPIO_AUDIO_EN_PORT   GPIOB
#define ESU_GPIO_AUDIO_EN_PIN    GPIO_PIN_1

/**
 * @brief Status LEDs
 */
#define ESU_GPIO_LED_CUT_PORT    GPIOC
#define ESU_GPIO_LED_CUT_PIN     GPIO_PIN_13
#define ESU_GPIO_LED_COAG_PORT   GPIOC
#define ESU_GPIO_LED_COAG_PIN    GPIO_PIN_2
#define ESU_GPIO_LED_REM_PORT    GPIOE
#define ESU_GPIO_LED_REM_PIN     GPIO_PIN_8
#define ESU_GPIO_LED_ERR_PORT    GPIOC
#define ESU_GPIO_LED_ERR_PIN     GPIO_PIN_4

/**
 * @brief Footswitch / handswitch inputs
 */
#define ESU_GPIO_FS_CUT1_PORT    GPIOC
#define ESU_GPIO_FS_CUT1_PIN     GPIO_PIN_1
#define ESU_GPIO_FS_COAG1_PORT   GPIOC
#define ESU_GPIO_FS_COAG1_PIN    GPIO_PIN_8
#define ESU_GPIO_FS_CUT2_PORT    GPIOE
#define ESU_GPIO_FS_CUT2_PIN     GPIO_PIN_9
#define ESU_GPIO_FS_COAG2_PORT   GPIOE
#define ESU_GPIO_FS_COAG2_PIN    GPIO_PIN_10
#define ESU_GPIO_HS_CUT_PORT     GPIOB
#define ESU_GPIO_HS_CUT_PIN      GPIO_PIN_4
#define ESU_GPIO_HS_COAG_PORT    GPIOB
#define ESU_GPIO_HS_COAG_PIN     GPIO_PIN_5
#define ESU_GPIO_REM_OK_PORT     GPIOB
#define ESU_GPIO_REM_OK_PIN      GPIO_PIN_7
#define ESU_GPIO_BIPO_AUTO_PORT  GPIOB
#define ESU_GPIO_BIPO_AUTO_PIN   GPIO_PIN_8

/* ----------------------------------------------------------------
 * ADC channel indices
 * ----------------------------------------------------------------*/
#define ADC_CH_IDX_CURRENT1     0   // PA0 – primary RF current sense
#define ADC_CH_IDX_VOLTAGE1     1   // PA1 – RF output voltage sense
#define ADC_CH_IDX_CURRENT2     2   // PA2 – secondary / neutral current
#define ADC_CH_IDX_REM          3   // PA3 – REM impedance sense
#define ADC_CH_IDX_TEMP         4   // PB0 – heat-sink temperature sense

#define ADC_CHANNELS            5U
#define ADC_VREF_MV             3300U
#define ADC_FULL_SCALE          4095U

/* ----------------------------------------------------------------
 * Power control
 * ----------------------------------------------------------------*/
/**
 * @brief DAC output range: 0-4095 → 0-3.3 V → 0-Vmax at power amp
 */
#define DAC_MAX                 4095U
#define DAC_MIN                 0U

/**
 * @note Gain = NUM/DEN
 */
#define ESU_P_GAIN_NUM          10
#define ESU_P_GAIN_DEN          1    // gain = NUM / DEN

/* ----------------------------------------------------------------
 * Polypectomy timings
 * ----------------------------------------------------------------*/
#define POLY_CUT_TICKS_MIN      3    //  30 ms
#define POLY_CUT_TICKS_MAX      5    //  50 ms
#define POLY_COAG_TICKS_MIN     10   // 100 ms
#define POLY_COAG_TICKS_MAX     150  // 1500 ms
// Level 1 = fastest, level 4 = slowest coag phase
#define POLY_COAG_TICKS(lvl)    (POLY_COAG_TICKS_MIN + ((lvl) - 1U) * \
                                 ((POLY_COAG_TICKS_MAX - POLY_COAG_TICKS_MIN) / 3U))

/* ----------------------------------------------------------------
 * Public interface
 * ----------------------------------------------------------------*/
/**
 * @brief   Initialise the ESU module. Call once after all HAL inits
 * @param   pHadc    Pointer to ADC1 handle
 * @param   pHdac    Pointer to DAC handle
 * @param   pHtim_cut    TIM4  (CUT carrier, 500 kHz)
 * @param   pHtim_coag   TIM13 (COAG carrier, 400 kHz)
 * @param   pHtim_coag2  TIM14 (Contact/Spray carrier, ≥400 kHz)
 * @param   pHtim_audio  TIM9  (buzzer)
 * @param   pHtim_mod    TIM2  (Blend1 envelope, 33 kHz, interrupt)
 * @param   pHtim_poly   TIM5  (polypectomy tick, 100 Hz, interrupt)
 */
void esu_init(ADC_HandleTypeDef   *pHadc,
              DAC_HandleTypeDef   *pHdac,
              TIM_HandleTypeDef   *pHtim_cut,
              TIM_HandleTypeDef   *pHtim_coag,
              TIM_HandleTypeDef   *pHtim_coag2,
              TIM_HandleTypeDef   *pHtim_audio,
              TIM_HandleTypeDef   *pHtim_mod,
              TIM_HandleTypeDef   *pHtim_poly);

/**
 * @brief   Apply a new settings packet received from Nextion
 * @note    Does not activate output — just stores settings
 */
void esu_applySettings(const ESU_Packet_t *pPkt);

/**
 * @brief   Run one iteration of the ESU main loop
 * @details Call from while(1) in main().
 *          Polls footswitches/handswitches and handles state transitions.
 */
void esu_process(void);

/**
 * @brief   TIM5 period-elapsed callback — polypectomy tick (100 Hz).
 * @details Call from HAL_TIM_PeriodElapsedCallback when htim == htim_poly.
 */
void esu_polyTick(void);

/**
 * @brief  TIM2 period-elapsed callback — Blend1 envelope 
 *         Toggles TIM4 CCR to implement burst modulation
 *         Call from HAL_TIM_PeriodElapsedCallback when htim == htim_mod
 */
void esu_blendTick(void);

/**
 * @brief Return current system state
 */
ESU_State_e esu_getState(void);

/**
 * @brief Return current error flags
 */
uint8_t esu_getErrors(void);

/**
 * @brief Return last measured output power in watts (×10 for one decimal)
 */
uint16_t esu_getMeasuredPowerDw(void);

/**
 * @brief Force system into ERROR state (call on hardware fault)
 */
void esu_forceError(uint8_t error_flags);

#ifdef __cplusplus
}
#endif

#endif /* _APP_FSM_H */