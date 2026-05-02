/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   app_fsm.c
 * @brief  ESU application state machine.
 *
 * @details
 *  This module owns the ESU system state and orchestrates all other
 *  modules.  It does not contain direct hardware register access.
 *
 *  Relay settle protocol
 *  ─────────────────────
 *  rfGen_configure*() calls relay_apply() internally, which starts a
 *  200 ms non-blocking settle timer.  appFsm_enterState() therefore
 *  does NOT call rfGen_enable*() directly; instead it transitions to
 *  an intermediate "settling" sub-state (AppDefs_EsuState_Settling)
 *  and defers the enable call until relay_isSettled() returns true.
 *  relay_update() is called every main-loop iteration from
 *  app_fsmProcess() so the settle timer advances without blocking.
 *
 *  Polypectomy sub-state cycling (PolypectomyCut ↔ PolypectomyCoag)
 *  is driven by TIM5 ISR via app_fsmPolyTick().  To avoid executing
 *  HAL/RF calls from ISR context, the tick handler only sets volatile
 *  flags.  The actual RF reconfiguration happens in app_fsmProcess()
 *  running from the main loop.
 *
 *  ISR ↔ main flag protocol:
 *    The ISR writes gPolyTransitToCoag / gPolyTransitToCut (volatile
 *    bool).  app_fsmProcess() reads and clears them inside a critical
 *    section (__disable_irq / __enable_irq) to guarantee an atomic
 *    test-and-clear with no lost transitions.
 */

#include "app_fsm.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Private state                                                               */
/* -------------------------------------------------------------------------- */

/*
 * gState is volatile because app_fsmPolyTick() (ISR) reads it.
 * All writes to gState from ISR context must be single-word aligned
 * assignments (atomic on Cortex-M4).
 */
static volatile AppDefs_EsuState_e gState   = AppDefs_EsuState_Idle;
static AppDefs_Channel_e           gChannel = AppDefs_Channel_Mono1;
static AppDefs_EsuSettings_t       gSettings;
static uint8_t                     gErrors  = ESU_ERR_NONE;

// Polypectomy timing (ticks at 100 Hz from TIM5)
static volatile uint16_t gPolyTick      = 0U;
static          uint16_t gPolyCutTicks  = 4U;   // 40 ms
static          uint16_t gPolyCoagTicks = 50U;  // 500 ms

/*
 * ISR → main-loop transition flags.
 * Written in ISR, read and cleared inside a critical section in
 * app_fsmProcess() to guarantee atomic test-and-clear.
 */
static volatile bool gPolyTransitToCoag = false;
static volatile bool gPolyTransitToCut  = false;

/*
 * Pending state — the state to enter once relay_isSettled() is true.
 * AppDefs_EsuState_Count means "nothing pending".
 */
static AppDefs_EsuState_e gPendingState = AppDefs_EsuState_Count;

// Status-push de-duplication
static AppDefs_EsuState_e gLastSentState   = (AppDefs_EsuState_e)0xFFU;
static uint16_t           gLastSentPowerDw = 0xFFFFU;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Disable all RF outputs and zero the DAC.
 * @retval None
 */
static void appFsm_allOff(void) {
    rfGen_disableAll();
    rfGen_audioStop();
    adcMonitor_dacZero();
}

/**
 * @brief  Execute exit actions for the current state, update gState,
 *         then start entry actions for the new state.
 *
 * @details
 *  For states that require relay switching the function calls
 *  rfGen_configure*() (which triggers relay_apply() internally) but
 *  defers rfGen_enable*() until relay_isSettled() is true.  The
 *  gPendingState field records which enable call to make once settled.
 *
 * @param  newState  Target state.
 * @retval None
 */
static void appFsm_enterState(AppDefs_EsuState_e newState) {
    // ---- Exit actions ----
    switch (gState) {
        case AppDefs_EsuState_CutActive:
        case AppDefs_EsuState_PolypectomyCut:
            rfGen_disableCut();
            rfGen_audioStop();
            adcMonitor_dacZero();
            // Clear pending poly flags in case pedal was released mid-cycle
            __disable_irq();
            gPolyTransitToCoag = false;
            gPolyTransitToCut  = false;
            __enable_irq();
            break;

        case AppDefs_EsuState_CoagActive:
        case AppDefs_EsuState_PolypectomyCoag:
            rfGen_disableCoag();
            rfGen_audioStop();
            adcMonitor_dacZero();
            __disable_irq();
            gPolyTransitToCoag = false;
            gPolyTransitToCut  = false;
            __enable_irq();
            break;

        case AppDefs_EsuState_BipolarCut:
        case AppDefs_EsuState_BipolarCoag:
            rfGen_disableBipolar();
            rfGen_audioStop();
            adcMonitor_dacZero();
            break;

        default:
            break;
    }

    gState        = newState;
    gPendingState = AppDefs_EsuState_Count; // Clear any stale pending enable

    // ---- Entry actions ----
    switch (newState) {
        case AppDefs_EsuState_Idle:
            appFsm_allOff();
            // Clear recoverable errors on returning to Idle
            gErrors &= (uint8_t)~(ESU_ERR_OVERCURRENT | ESU_ERR_OVERTEMP);
            nextion_sendPage("mainPage");
            break;

        case AppDefs_EsuState_CutActive:
            // Configure timers + relay (settle starts inside rfGen_configure)
            rfGen_configureCut((AppDefs_CutMode_e)gSettings.cut_mode);
            // Defer rfGen_enableCut() until relay settles
            gPendingState = AppDefs_EsuState_CutActive;
            nextion_sendPage("activePage");
            break;

        case AppDefs_EsuState_CoagActive:
            rfGen_configureCoag((AppDefs_CoagMode_e)gSettings.coag_mode);
            gPendingState = AppDefs_EsuState_CoagActive;
            nextion_sendPage("activePage");
            break;

        case AppDefs_EsuState_BipolarCut:
            rfGen_configureBipolarCut((AppDefs_BipolarCutMode_e)gSettings.cut_mode);
            gPendingState = AppDefs_EsuState_BipolarCut;
            break;

        case AppDefs_EsuState_BipolarCoag:
            rfGen_configureBipolarCoag((AppDefs_BipolarCoagMode_e)gSettings.coag_mode);
            gPendingState = AppDefs_EsuState_BipolarCoag;
            break;

        case AppDefs_EsuState_PolypectomyCut:
            __disable_irq();
            gPolyTick          = 0U;
            gPolyTransitToCoag = false;
            gPolyTransitToCut  = false;
            __enable_irq();
            rfGen_configureCut(AppDefs_CutMode_Polypectomy);
            gPendingState = AppDefs_EsuState_PolypectomyCut;
            break;

        // PolypectomyCoag cycling is handled inline via gPolyTransit* flags
        // in app_fsmProcess() — no entry through this path in normal operation.

        case AppDefs_EsuState_RemAlarm:
            appFsm_allOff();
            gErrors |= ESU_ERR_REM_ALARM;
            nextion_sendPage("remAlarmPage");
            break;

        case AppDefs_EsuState_Error:
            appFsm_allOff();
            nextion_sendPage("errorPage");
            break;

        default:
            break;
    }
}

/**
 * @brief  Complete a deferred RF enable once relay settle has elapsed.
 * @retval None
 */
static void appFsm_applyPendingEnable(void) {
    if (AppDefs_EsuState_Count == gPendingState) {
        return; // Nothing pending
    }

    if (false == relay_isSettled()) {
        return; // Still settling
    }

    switch (gPendingState) {
        case AppDefs_EsuState_CutActive:
        case AppDefs_EsuState_PolypectomyCut:
            rfGen_enableCut();
            rfGen_audioStart(true);
            break;

        case AppDefs_EsuState_CoagActive:
        case AppDefs_EsuState_PolypectomyCoag:
            rfGen_enableCoag();
            rfGen_audioStart(false);
            break;

        case AppDefs_EsuState_BipolarCut:
            rfGen_enableBipolar();
            rfGen_audioStart(true);
            break;

        case AppDefs_EsuState_BipolarCoag:
            rfGen_enableBipolar();
            rfGen_audioStart(false);
            break;

        default:
            break;
    }

    gPendingState = AppDefs_EsuState_Count;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief  One-time initialisation.  Call after all HAL and MX inits.
 * @param  pTimers       RF generator timer bundle.
 * @param  pHadc         ADC1 handle.
 * @param  pHdac         DAC handle.
 * @param  pUartNextion  USART3 handle (Nextion display, 9600 baud).
 * @retval None
 */
void app_fsm_init(const RFGen_Timers_t *pTimers,
                  ADC_HandleTypeDef    *pHadc,
                  DAC_HandleTypeDef    *pHdac,
                  UART_HandleTypeDef   *pUartNextion) {
    rfGen_init(pTimers);
    adcMonitor_init(pHadc, pHdac);
    pedal_init();
    nextion_init(pUartNextion);

    (void)memset(&gSettings, 0, sizeof(gSettings));
    gSettings.cut_mode    = (uint8_t)AppDefs_CutMode_Pure;
    gSettings.cut_level   = 1U;
    gSettings.cut_powerW  = 30U;
    gSettings.coag_mode   = (uint8_t)AppDefs_CoagMode_Soft;
    gSettings.coag_level  = 1U;
    gSettings.coag_powerW = 30U;
    gSettings.poly_level  = 1U;

    gChannel           = AppDefs_Channel_Mono1;
    gState             = AppDefs_EsuState_Idle;
    gErrors            = ESU_ERR_NONE;
    gPolyTick          = 0U;
    gPolyCutTicks      = 4U;
    gPolyCoagTicks     = POLY_COAG_TICKS(gSettings.poly_level);
    gPolyTransitToCoag = false;
    gPolyTransitToCut  = false;
    gPendingState      = AppDefs_EsuState_Count;
    gLastSentState     = (AppDefs_EsuState_e)0xFFU;
    gLastSentPowerDw   = 0xFFFFU;

    appFsm_allOff();
    nextion_sendPage("mainPage");
}

/**
 * @brief  Main loop body.  Call from while(1) in main().
 * @retval None
 */
void app_fsmProcess(void) {
    AppDefs_EsuPacket_t pkt;
    bool                isBipolar;
    Safety_Fault_e      faults;
    bool                cutPressed;
    bool                coagPressed;
    bool                skipMainFsm = false;
    uint16_t            powerDw;

    // Local copies of ISR flags, read atomically
    bool transitToCoag;
    bool transitToCut;

    // Advance relay settle timer
    relay_update();

    adcMonitor_scan();
    pedal_update();

    // Complete any deferred RF enable (post-relay-settle)
    appFsm_applyPendingEnable();

    /* ------------------------------------------------------------------
     * Atomically read and clear polypectomy transition flags.
     * RF reconfiguration is done here (main context) — HAL calls must
     * not happen in ISR context.
     * ------------------------------------------------------------------ */
    __disable_irq();
    transitToCoag      = (bool)gPolyTransitToCoag;
    gPolyTransitToCoag = false;
    transitToCut       = (bool)gPolyTransitToCut;
    gPolyTransitToCut  = false;
    __enable_irq();

    if ((true == transitToCoag) && (AppDefs_EsuState_PolypectomyCut == gState)) {
        rfGen_disableCut();
        rfGen_configureCoag(AppDefs_CoagMode_Soft);
        // Deferred enable — wait for relay settle
        gPendingState = AppDefs_EsuState_PolypectomyCoag;
        gState        = AppDefs_EsuState_PolypectomyCoag;
    }

    if ((true == transitToCut) && (AppDefs_EsuState_PolypectomyCoag == gState)) {
        rfGen_disableCoag();
        rfGen_configureCut(AppDefs_CutMode_Polypectomy);
        gPendingState = AppDefs_EsuState_PolypectomyCut;
        gState        = AppDefs_EsuState_PolypectomyCut;
    }

    /* ------------------------------------------------------------------
     * Receive and apply settings packet from Nextion.
     * Settings are only accepted in Idle to prevent mid-activation
     * configuration changes.
     * ------------------------------------------------------------------ */
    if (true == nextion_getPacket(&pkt)) {
        if (AppDefs_EsuState_Idle == gState) {
            gChannel              = (AppDefs_Channel_e)pkt.channel;
            gSettings.cut_mode    = pkt.cut_mode;
            gSettings.cut_level   = pkt.cut_level;
            gSettings.cut_powerW  = pkt.cut_powerW;
            gSettings.coag_mode   = pkt.coag_mode;
            gSettings.coag_level  = pkt.coag_level;
            gSettings.coag_powerW = pkt.coag_powerW;
            gSettings.poly_level  = pkt.poly_level;
            gPolyCutTicks         = 4U;
            gPolyCoagTicks        = POLY_COAG_TICKS(gSettings.poly_level);
        }
    }

    isBipolar = (AppDefs_Channel_Bipolar == gChannel);
    faults    = safetyCheck(isBipolar);

    /* ------------------------------------------------------------------
     * Safety interlocks — evaluated in priority order.
     * ------------------------------------------------------------------ */

    // Priority 1: REM alarm
    if (0U != ((uint8_t)faults & (uint8_t)Safety_Fault_e_REM)) {
        if ((AppDefs_EsuState_RemAlarm != gState) &&
            (AppDefs_EsuState_Error    != gState)) {
            appFsm_enterState(AppDefs_EsuState_RemAlarm);
        }
        skipMainFsm = true;
    }

    // Clear REM alarm once fault resolves
    if ((false == skipMainFsm) &&
        (AppDefs_EsuState_RemAlarm == gState) &&
        (0U == ((uint8_t)faults & (uint8_t)Safety_Fault_e_REM))) {
        appFsm_enterState(AppDefs_EsuState_Idle);
    }

    // Priority 2: Overcurrent
    if ((false == skipMainFsm) &&
        (0U != ((uint8_t)faults & (uint8_t)Safety_Fault_e_OC))) {
        gErrors |= ESU_ERR_OVERCURRENT;
        appFsm_enterState(AppDefs_EsuState_Error);
        skipMainFsm = true;
    }

    // Priority 3: Overtemperature
    if ((false == skipMainFsm) &&
        (0U != ((uint8_t)faults & (uint8_t)Safety_Fault_e_OT))) {
        gErrors |= ESU_ERR_OVERTEMP;
        appFsm_enterState(AppDefs_EsuState_Error);
        skipMainFsm = true;
    }

    // Error state latches until external reset
    if ((false == skipMainFsm) && (AppDefs_EsuState_Error == gState)) {
        skipMainFsm = true;
    }

    /* ------------------------------------------------------------------
     * Main application state machine
     * ------------------------------------------------------------------ */
    if (false == skipMainFsm) {
        cutPressed  = pedal_isCutPressed(gChannel);
        coagPressed = pedal_isCoagPressed(gChannel);

        switch (gState) {
            case AppDefs_EsuState_Idle:
                if (true == isBipolar) {
                    if ((true == pedal_isBipolarAuto()) &&
                        ((uint8_t)AppDefs_BipolarCoagMode_AutoStart == gSettings.coag_mode)) {
                        appFsm_enterState(AppDefs_EsuState_BipolarCoag);
                    } else if (true == cutPressed) {
                        appFsm_enterState(AppDefs_EsuState_BipolarCut);
                    } else if (true == coagPressed) {
                        appFsm_enterState(AppDefs_EsuState_BipolarCoag);
                    } else {
                        // No activation
                    }
                } else {
                    if (true == cutPressed) {
                        if ((uint8_t)AppDefs_CutMode_Polypectomy == gSettings.cut_mode) {
                            appFsm_enterState(AppDefs_EsuState_PolypectomyCut);
                        } else {
                            appFsm_enterState(AppDefs_EsuState_CutActive);
                        }
                    } else if (true == coagPressed) {
                        appFsm_enterState(AppDefs_EsuState_CoagActive);
                    } else {
                        // No activation
                    }
                }
                break;

            case AppDefs_EsuState_CutActive:
                adcMonitor_powerLoop(gSettings.cut_powerW);
                if (false == cutPressed) {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            case AppDefs_EsuState_CoagActive:
                adcMonitor_powerLoop(gSettings.coag_powerW);
                if (false == coagPressed) {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            case AppDefs_EsuState_BipolarCut:
                adcMonitor_powerLoop(gSettings.cut_powerW);
                if (false == cutPressed) {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            case AppDefs_EsuState_BipolarCoag:
                adcMonitor_powerLoop(gSettings.coag_powerW);
                if ((uint8_t)AppDefs_BipolarCoagMode_AutoStart == gSettings.coag_mode) {
                    if (false == pedal_isBipolarAuto()) {
                        appFsm_enterState(AppDefs_EsuState_Idle);
                    }
                } else {
                    if (false == coagPressed) {
                        appFsm_enterState(AppDefs_EsuState_Idle);
                    }
                }
                break;

            case AppDefs_EsuState_PolypectomyCut:
                adcMonitor_powerLoop(gSettings.cut_powerW);
                if (false == cutPressed) {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            case AppDefs_EsuState_PolypectomyCoag:
                adcMonitor_powerLoop(gSettings.coag_powerW);
                if (false == cutPressed) { // Pedal released during coag burst
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            // RemAlarm and Error handled above via skipMainFsm
            default:
                break;
        }
    }

    /* ------------------------------------------------------------------
     * Push status to Nextion only on change (reduce UART traffic)
     * ------------------------------------------------------------------ */
    powerDw = adcMonitor_getPowerDw();
    if ((gState != gLastSentState) || (powerDw != gLastSentPowerDw)) {
        nextion_pushStatus(gState, powerDw, gErrors);
        gLastSentState   = gState;
        gLastSentPowerDw = powerDw;
    }
}

/**
 * @brief  Polypectomy sub-state tick — call from TIM5 ISR at 100 Hz.
 * @retval None
 */
void app_fsmPolyTick(void) {
    AppDefs_EsuState_e state = gState; // Single atomic read

    if ((AppDefs_EsuState_PolypectomyCut  != state) &&
        (AppDefs_EsuState_PolypectomyCoag != state)) {
        return;
    }

    gPolyTick++;

    if (AppDefs_EsuState_PolypectomyCut == state) {
        if (gPolyTick >= gPolyCutTicks) {
            gPolyTick          = 0U;
            gPolyTransitToCoag = true;
        }
    } else {
        if (gPolyTick >= gPolyCoagTicks) {
            gPolyTick         = 0U;
            gPolyTransitToCut = true;
        }
    }
}

/**
 * @brief  Blend envelope tick — call from TIM2 ISR.
 * @retval None
 */
void app_fsmBlendTick(void) {
    rfGen_blendTickIsr();
}

/**
 * @brief  IDLE line callback — call from USART3_IRQHandler.
 * @retval None
 */
void app_fsmIdleIsr(void) {
    nextion_cbIdleIsr();
}

/**
 * @brief  Return current ESU state.
 * @retval Current AppDefs_EsuState_e value.
 */
AppDefs_EsuState_e app_fsmGetState(void) {
    return gState;
}

/**
 * @brief  Return current error bitmask.
 * @retval ESU_ERR_* bitmask.
 */
uint8_t app_fsmGetErrors(void) {
    return gErrors;
}