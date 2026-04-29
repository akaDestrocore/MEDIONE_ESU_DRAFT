/**
 * @file   pedal.h
 * @brief  Footswitch and handswitch debounce module.
 *
 * @details
 *  All switch inputs are active-LOW with internal pull-up.
 *  Debounce: input must be stable for PEDAL_DEBOUNCE_MS before
 *  it is reported as pressed.
 *
 *  Pin mapping:
 *    PC1  — Footswitch CUT  Mono1
 *    PC8  — Footswitch COAG Mono1
 *    PE9  — Footswitch CUT  Mono2
 *    PE10 — Footswitch COAG Mono2
 *    PB4  — Handswitch CUT  (yellow, Twin Button Handle)
 *    PB5  — Handswitch COAG (blue,   Twin Button Handle)
 *    PB7  — REM OK signal   (active-HIGH)
 *    PB8  — Bipolar auto-start (forceps contact, active-LOW)
 */

#ifndef _PEDAL_H
#define _PEDAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx_hal.h"
#include "app_defs.h"
#include <stdbool.h>

/* Debounce threshold in esu_process() calls.
 * esu_process() runs every ~5 ms → 5 × 5 ms = 25 ms debounce */
#define PEDAL_DEBOUNCE_TICKS  5U

/**
 * @brief  Initialise pedal module (no HAL init needed — GPIOs already
 *         configured in MX_GPIO_Init).  Just resets internal counters.
 */
void pedal_init(void);

/**
 * @brief  Update debounce counters. Call once per esu_process() cycle.
 */
void pedal_update(void);

/**
 * @brief  Return true when the CUT footswitch / handswitch is stably pressed
 *         for the configured channel.
 * @param  ch  Which channel to query (MONO1, MONO2, or BIPOLAR).
 */
bool pedal_cut_pressed(ESU_Channel_e ch);

/**
 * @brief  Return true when the COAG footswitch / handswitch is stably pressed.
 */
bool pedal_coag_pressed(ESU_Channel_e ch);

/**
 * @brief  Return true when bipolar forceps auto-start contact is detected.
 */
bool pedal_bipolar_auto(void);

#ifdef __cplusplus
}
#endif

#endif /* _PEDAL_H */