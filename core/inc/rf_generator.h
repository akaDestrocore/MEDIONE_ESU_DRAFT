/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file   rf_generator.h
 * @brief  RF carrier generation — timer reconfiguration per mode.
 *
 * @details
 *  Timer allocation (system clock 168 MHz, APB1/2 at 42 MHz → timer at 84 MHz):
 *
 *  TIM4  CH1 → PD12 : CUT carrier
 *      Pure Cut    : 500 kHz  (PSC=20, ARR=7)
 *      Blend1      : 400 kHz  (PSC=20, ARR=9)
 *      Blend2/3    : 380 kHz  (PSC=21, ARR=9)
 *      Polypectomy : 350 kHz  (PSC=23, ARR=9)
 *
 *  TIM13 CH1 → PA6  : COAG Soft / Bipolar carriers
 *      Soft Coag  : 400 kHz, duty 39 %  (PSC=20, ARR=9, CCR=4)
 *      Bip.Std/AS : 500 kHz, duty 50 %  (PSC=16, ARR=9, CCR=5)
 *
 *  TIM14 CH1 → PA7  : COAG Contact / Spray / Argon / Bip.Forced
 *      Contact     : 727 kHz, duty ~10 % (PSC=11, ARR=9, CCR=1)
 *      Spray/Argon : 400 kHz, duty ~10 % (PSC=20, ARR=9, CCR=1)
 *      Bip.Forced  : 500 kHz, duty ~10 % (PSC=16, ARR=9, CCR=1)
 *
 *   TIM9  CH2 → PE6    : Audio buzzer
 *   TIM2  CH1 → PA5    : Blend envelope modulator (33 kHz / 27 kHz)
 *   TIM3  CH1 → PC6    : Polypectomy slow on/off pulse (~1.5 Hz)
 *   TIM5  (no output)  : Polypectomy cycle tick 100 Hz
 */

#ifndef _RF_GENERATOR_H
#define _RF_GENERATOR_H

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include "stm32f4xx_hal.h"
#include "app_defs.h"

/* ----------------------------------------------------------------
 * Timer handle bundle — populated once in rf_gen_init()
 * ----------------------------------------------------------------*/
typedef struct {
    TIM_HandleTypeDef *htim_cut;        // TIM4
    TIM_HandleTypeDef *htim_coag;       // TIM13
    TIM_HandleTypeDef *htim_coag2;      // TIM14
    TIM_HandleTypeDef *htim_audio;      // TIM9
    TIM_HandleTypeDef *htim_blend;      // TIM2  (33/27 kHz modulator ISR)
    TIM_HandleTypeDef *htim_poly_slow;  // TIM3 (~1.5 Hz on/off)
    TIM_HandleTypeDef *htim_poly_tick;  // TIM5 (100 Hz tick ISR)
} RFGen_Timers_t;

/* ----------------------------------------------------------------
 * Blend software modulation state (shared with ISR via rf_generator.c)
 * ----------------------------------------------------------------*/
typedef struct {
    volatile uint8_t on;        // ON counts per period
    volatile uint8_t total;     // total counts per period
    volatile uint8_t counter;   // running counter (0..total-1)
    volatile bool    active;    // false → modulator ISR is a no-op
} RFGen_BlendState_t;

extern RFGen_BlendState_t g_blend;

/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/

/**
 * @brief  Initialise RF generator module
 * @param  timers  Pointer to a fully populated RFGen_Timers_t struct
 */
void rf_gen_init(const RFGen_Timers_t *timers);

/**
 * @brief   Configure all timers for the requested CUT mode
 *          Does NOT enable the amp — call rf_gen_enable() separately
 */
void rf_gen_configure_cut(CutMode_e mode);

/**
 * @brief   Configure all timers for the requested COAG mode
 */
void rf_gen_configure_coag(CoagMode_e mode);

/**
 * @brief   Configure timers for a Bipolar CUT sub-mode
 */
void rf_gen_configure_bipolar_cut(BipolarCutMode_e mode);

/**
 * @brief   Configure timers for a Bipolar COAG sub-mode
 */
void rf_gen_configure_bipolar_coag(BipolarCoagMode_e mode);

/**
 * @brief   Enable the RF output amplifier for the CUT path
 *         LED is set here too
 */
void rf_gen_enable_cut(void);

/**
 * @brief   Disable the RF output amplifier for the CUT path
 */
void rf_gen_disable_cut(void);

/**
 * @brief   Enable the RF output amplifier for the COAG path
 */
void rf_gen_enable_coag(void);

/**
 * @brief   Disable the RF output amplifier for the COAG path
 */
void rf_gen_disable_coag(void);

/**
 * @brief   Enable the RF output amplifier for the Bipolar path
 */
void rf_gen_enable_bipolar(void);

/**
 * @brief   Disable the RF output amplifier for the Bipolar path
 */
void rf_gen_disable_bipolar(void);

/**
 * @brief   Disable all RF paths and reset blend modulator
 */
void rf_gen_disable_all(void);

/**
 * @brief   Start the audio buzzer
 * @param   is_cut  true → CUT tone (~800 Hz), false → COAG tone (~500 Hz)
 */
void rf_gen_audio_start(bool is_cut);

/**
 * @brief  Stop the audio buzzer
 */
void rf_gen_audio_stop(void);

/**
 * @brief  Called from TIM2/TIM3 period-elapsed ISR
 *         Implements the blend envelope modulation by toggling TIM4 CCR
 */
void rf_gen_blend_tick_isr(void);

#ifdef __cplusplus
}
#endif

#endif /* _RF_GENERATOR_H */