/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   pedal.h
 * @brief  Footswitch and handswitch debounce module.
 *
 * @details
 *  All switch inputs are active-LOW with internal pull-up.
 *  Debounce: input must be stable for PEDAL_DEBOUNCE_TICKS before
 *  it is reported as pressed.
 *
 *  Pin mapping:
 *    PC1  — Footswitch CUT  Mono1
 *    PC8  — Footswitch COAG Mono1
 *    PE9  — Footswitch CUT  Mono2
 *    PE10 — Footswitch COAG Mono2
 *    PB4  — Handswitch CUT   (yellow, Twin Button Handle)
 *    PB5  — Handswitch COAG  (blue,   Twin Button Handle)
 *    PB7  — REM OK signal    (active-HIGH)
 *    PB8  — Bipolar auto-start (forceps contact, active-LOW)
 */

#ifndef PEDAL_H_
#define PEDAL_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>
#include "app_defs.h"
#include "stm32f4xx_hal.h"

#define PEDAL_DEBOUNCE_TICKS    5U

/**
 * @brief  Initialise pedal module.
 * @param  None
 * @retval None
 */
void pedal_init(void);

/**
 * @brief  Update debounce counters — call once per main-loop iteration.
 * @param  None
 * @retval None
 */
void pedal_update(void);

/**
 * @brief  Return true when the CUT input is stably pressed.
 * @param  channel  Channel to query.
 * @retval true if CUT is pressed, false otherwise.
 */
bool pedal_isCutPressed(AppDefs_Channel_e channel);

/**
 * @brief  Return true when the COAG input is stably pressed.
 * @param  channel  Channel to query.
 * @retval true if COAG is pressed, false otherwise.
 */
bool pedal_isCoagPressed(AppDefs_Channel_e channel);

/**
 * @brief  Return true when bipolar forceps auto-start contact is detected.
 * @param  None
 * @retval true if bipolar auto-start is active, false otherwise.
 */
bool pedal_isBipolarAuto(void);

#ifdef __cplusplus
}
#endif

#endif /* PEDAL_H_ */