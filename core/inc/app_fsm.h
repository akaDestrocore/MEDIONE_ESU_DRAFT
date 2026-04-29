/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * 
 * @file   app_fsm.h
 * @brief  ESU application state machine — public interface.
 */

#ifndef _APP_FSM_H
#define _APP_FSM_H

#ifdef __cplusplus
extern "C" {
#endif

#include "app_defs.h"
#include "rf_generator.h"
#include "stm32f4xx_hal.h"

/* Polypectomy coag tick count from level 1-4 */
#define POLY_COAG_TICKS_MIN  10U    // 100 ms
#define POLY_COAG_TICKS_MAX 150U    // 1500 ms
#define POLY_COAG_TICKS(lvl) \
    (POLY_COAG_TICKS_MIN + \
     ((uint16_t)((lvl) - 1U) * \
      ((POLY_COAG_TICKS_MAX - POLY_COAG_TICKS_MIN) / 3U)))

/**
 * @brief  One-time initialisation.  Call after all HAL and MX inits.
 * @param  timers         RF generator timer bundle.
 * @param  hadc           ADC1 handle.
 * @param  hdac           DAC handle.
 * @param  huart_nextion  USART3 handle (Nextion display, 9600 baud).
 */
void app_fsm_init(const RFGen_Timers_t *timers,
                  ADC_HandleTypeDef    *hadc,
                  DAC_HandleTypeDef    *hdac,
                  UART_HandleTypeDef   *huart_nextion);

/**
 * @brief  Main loop body.  Call from while(1) in main().
 */
void app_fsm_process(void);

/**
 * @brief  Polypectomy sub-state tick.
 *         Call from HAL_TIM_PeriodElapsedCallback when htim == &htim5.
 */
void app_fsm_poly_tick(void);

/**
 * @brief  Blend envelope tick.
 *         Call from HAL_TIM_PeriodElapsedCallback when htim == &htim2.
 */
void app_fsm_blend_tick(void);

/**
 * @brief  USART3 IDLE line callback.
 *         Call from USART3_IRQHandler after clearing the IDLE flag.
 */
void app_fsm_idle_isr(void);

/** @brief  Return current ESU state. */
ESU_State_e app_fsm_get_state(void);

/** @brief  Return current error bitmask. */
uint8_t app_fsm_get_errors(void);

#ifdef __cplusplus
}
#endif

#endif /* _APP_FSM_H */