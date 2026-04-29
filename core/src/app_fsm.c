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
 *  modules. It does not contain direct hardware register access.
 *
 *  Polypectomy sub-state cycling (PolypectomyCut ↔ PolypectomyCoag)
 *  is driven by TIM5 ISR via app_fsmPolyTick(). To avoid executing
 *  HAL/RF calls from ISR context, the tick handler only sets volatile
 *  flags. The actual RF reconfiguration happens in app_fsmProcess()
 *  running from the main loop.
 */

#include "app_fsm.h"
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Private state                                                              */
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

/* Polypectomy timing (ticks at 100 Hz from TIM5) */
static volatile uint16_t gPolyTick      = 0U;
static          uint16_t gPolyCutTicks  = 4U;   /* 40 ms  */
static          uint16_t gPolyCoagTicks = 50U;  /* 500 ms */

/*
 * ISR → main-loop transition flags.
 * Written in ISR, read and cleared in app_fsmProcess().
 */
static volatile bool gPolyTransitToCoag = false;
static volatile bool gPolyTransitToCut  = false;

/* Status-push de-duplication */
static AppDefs_EsuState_e gLastSentState   = (AppDefs_EsuState_e)0xFFU;
static uint16_t           gLastSentPowerDw = 0xFFFFU;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief Disable all RF outputs and zero the DAC.
 */
static void appFsm_allOff(void)
{
    rfGen_disableAll();
    rfGen_audioStop();
    adcMonitor_dacZero();
}

/**
 * @brief Execute exit actions for the current state, update gState, then
 *        execute entry actions for the new state.
 *
 * @note  Must only be called from main-loop context (not from ISR).
 *        Polypectomy sub-state cycling bypasses this function and manages
 *        RF directly in app_fsmProcess() to avoid ISR-context HAL calls.
 *
 * @param newState  Target state.
 */
static void appFsm_enterState(AppDefs_EsuState_e newState)
{
    /* ---- Exit actions ---- */
    switch (gState)
    {
        case AppDefs_EsuState_CutActive:
        case AppDefs_EsuState_PolypectomyCut:
            rfGen_disableCut();
            rfGen_audioStop();
            adcMonitor_dacZero();
            /* Clear pending poly flags in case pedal was released mid-cycle */
            gPolyTransitToCoag = false;
            gPolyTransitToCut  = false;
            break;

        case AppDefs_EsuState_CoagActive:
        case AppDefs_EsuState_PolypectomyCoag:
            rfGen_disableCoag();
            rfGen_audioStop();
            adcMonitor_dacZero();
            gPolyTransitToCoag = false;
            gPolyTransitToCut  = false;
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

    gState = newState;

    /* ---- Entry actions ---- */
    switch (newState)
    {
        case AppDefs_EsuState_Idle:
            appFsm_allOff();
            /* Clear recoverable errors on returning to Idle */
            gErrors &= (uint8_t)~(ESU_ERR_OVERCURRENT | ESU_ERR_OVERTEMP);
            uartProto_sendPage("mainPage");
            break;

        case AppDefs_EsuState_CutActive:
            rfGen_configureCut((AppDefs_CutMode_e)gSettings.cut_mode);
            rfGen_enableCut();
            rfGen_audioStart(true);
            uartProto_sendPage("activePage");
            break;

        case AppDefs_EsuState_CoagActive:
            rfGen_configureCoag((AppDefs_CoagMode_e)gSettings.coag_mode);
            rfGen_enableCoag();
            rfGen_audioStart(false);
            uartProto_sendPage("activePage");
            break;

        case AppDefs_EsuState_BipolarCut:
            rfGen_configureBipolarCut((AppDefs_BipolarCutMode_e)gSettings.cut_mode);
            rfGen_enableBipolar();
            rfGen_audioStart(true);
            break;

        case AppDefs_EsuState_BipolarCoag:
            rfGen_configureBipolarCoag((AppDefs_BipolarCoagMode_e)gSettings.coag_mode);
            rfGen_enableBipolar();
            rfGen_audioStart(false);
            break;

        case AppDefs_EsuState_PolypectomyCut:
            /* Initial entry from Idle — start CUT phase */
            gPolyTick         = 0U;
            gPolyTransitToCoag = false;
            gPolyTransitToCut  = false;
            rfGen_configureCut(AppDefs_CutMode_Polypectomy);
            rfGen_enableCut();
            rfGen_audioStart(true);
            break;

        /*
         * AppDefs_EsuState_PolypectomyCoag:
         *   Sub-state cycling is handled inline in app_fsmProcess()
         *   via gPolyTransit* flags. This case is never reached through
         *   appFsm_enterState() in normal operation.
         */

        case AppDefs_EsuState_RemAlarm:
            appFsm_allOff();
            gErrors |= ESU_ERR_REM_ALARM;
            uartProto_sendPage("remAlarmPage");
            break;

        case AppDefs_EsuState_Error:
            appFsm_allOff();
            uartProto_sendPage("errorPage");
            break;

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void app_fsm_init(const RFGen_Timers_t   *pTimers,
                  ADC_HandleTypeDef      *pHadc,
                  DAC_HandleTypeDef      *pHdac,
                  UART_HandleTypeDef     *pUartNextion)
{
    rfGen_init(pTimers);
    adcMonitor_init(pHadc, pHdac);
    pedal_init();
    uartProto_init(pUartNextion);

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
    gLastSentState     = (AppDefs_EsuState_e)0xFFU;
    gLastSentPowerDw   = 0xFFFFU;

    appFsm_allOff();
    uartProto_sendPage("mainPage");
}


void app_fsmProcess(void)
{
    AppDefs_EsuPacket_t pkt;
    bool                isBipolar;
    Safety_Fault_e      faults;
    bool                cutPressed;
    bool                coagPressed;
    bool                skipMainFsm = false;
    uint16_t            powerDw;

    adcMonitor_scan();
    pedal_update();

    /* ------------------------------------------------------------------
     * Service ISR-triggered polypectomy sub-state transitions.
     * RF reconfiguration is performed here (main context) rather than
     * in the ISR, keeping HAL calls out of interrupt context.
     * ------------------------------------------------------------------ */
    if (gPolyTransitToCoag)
    {
        gPolyTransitToCoag = false;
        if (AppDefs_EsuState_PolypectomyCut == gState)
        {
            rfGen_disableCut();
            rfGen_configureCoag(AppDefs_CoagMode_Soft);
            rfGen_enableCoag();
            rfGen_audioStart(false);
            gState = AppDefs_EsuState_PolypectomyCoag;
        }
    }

    if (gPolyTransitToCut)
    {
        gPolyTransitToCut = false;
        if (AppDefs_EsuState_PolypectomyCoag == gState)
        {
            rfGen_disableCoag();
            rfGen_configureCut(AppDefs_CutMode_Polypectomy);
            rfGen_enableCut();
            rfGen_audioStart(true);
            gState = AppDefs_EsuState_PolypectomyCut;
        }
    }

    /* ------------------------------------------------------------------
     * Receive and apply settings packet from Nextion.
     * Settings are only accepted in Idle to prevent mid-activation
     * configuration changes.
     * ------------------------------------------------------------------ */
    if (uartProto_getPacket(&pkt))
    {
        if (AppDefs_EsuState_Idle == gState)
        {
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
     * Each sets skipMainFsm to prevent the main FSM from running
     * when a fault is active or the state has already been changed.
     * ------------------------------------------------------------------ */

    /* Priority 1: REM alarm */
    if (0U != ((uint8_t)faults & (uint8_t)Safety_Fault_e_REM))
    {
        if ((AppDefs_EsuState_RemAlarm != gState) &&
            (AppDefs_EsuState_Error    != gState))
        {
            appFsm_enterState(AppDefs_EsuState_RemAlarm);
        }
        skipMainFsm = true;
    }

    /* Clear REM alarm once fault resolves */
    if (!skipMainFsm &&
        (AppDefs_EsuState_RemAlarm == gState) &&
        (0U == ((uint8_t)faults & (uint8_t)Safety_Fault_e_REM)))
    {
        appFsm_enterState(AppDefs_EsuState_Idle);
    }

    /* Priority 2: Overcurrent */
    if (!skipMainFsm &&
        (0U != ((uint8_t)faults & (uint8_t)Safety_Fault_e_OC)))
    {
        gErrors |= ESU_ERR_OVERCURRENT;
        appFsm_enterState(AppDefs_EsuState_Error);
        skipMainFsm = true;
    }

    /* Priority 3: Overtemperature */
    if (!skipMainFsm &&
        (0U != ((uint8_t)faults & (uint8_t)Safety_Fault_e_OT)))
    {
        gErrors |= ESU_ERR_OVERTEMP;
        appFsm_enterState(AppDefs_EsuState_Error);
        skipMainFsm = true;
    }

    /* Error state latches until external reset — skip main FSM */
    if (!skipMainFsm && (AppDefs_EsuState_Error == gState))
    {
        skipMainFsm = true;
    }

    /* ------------------------------------------------------------------
     * Main application state machine
     * ------------------------------------------------------------------ */
    if (!skipMainFsm)
    {
        cutPressed  = pedal_isCutPressed(gChannel);
        coagPressed = pedal_isCoagPressed(gChannel);

        switch (gState)
        {
            /* ---- Idle ---- */
            case AppDefs_EsuState_Idle:
                if (isBipolar)
                {
                    if (pedal_isBipolarAuto() &&
                        ((uint8_t)AppDefs_BipolarCoagMode_AutoStart == gSettings.coag_mode))
                    {
                        appFsm_enterState(AppDefs_EsuState_BipolarCoag);
                    }
                    else if (cutPressed)
                    {
                        appFsm_enterState(AppDefs_EsuState_BipolarCut);
                    }
                    else if (coagPressed)
                    {
                        appFsm_enterState(AppDefs_EsuState_BipolarCoag);
                    }
                    else
                    {
                        /* No activation — remain Idle */
                    }
                }
                else
                {
                    if (cutPressed)
                    {
                        if ((uint8_t)AppDefs_CutMode_Polypectomy == gSettings.cut_mode)
                        {
                            appFsm_enterState(AppDefs_EsuState_PolypectomyCut);
                        }
                        else
                        {
                            appFsm_enterState(AppDefs_EsuState_CutActive);
                        }
                    }
                    else if (coagPressed)
                    {
                        appFsm_enterState(AppDefs_EsuState_CoagActive);
                    }
                    else
                    {
                        /* No activation — remain Idle */
                    }
                }
                break;

            /* ---- Monopolar CUT active ---- */
            case AppDefs_EsuState_CutActive:
                adcMonitor_powerLoop(gSettings.cut_powerW);
                if (!cutPressed)
                {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            /* ---- Monopolar COAG active ---- */
            case AppDefs_EsuState_CoagActive:
                adcMonitor_powerLoop(gSettings.coag_powerW);
                if (!coagPressed)
                {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            /* ---- Bipolar CUT ---- */
            case AppDefs_EsuState_BipolarCut:
                adcMonitor_powerLoop(gSettings.cut_powerW);
                if (!cutPressed)
                {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            /* ---- Bipolar COAG ---- */
            case AppDefs_EsuState_BipolarCoag:
                adcMonitor_powerLoop(gSettings.coag_powerW);
                if ((uint8_t)AppDefs_BipolarCoagMode_AutoStart == gSettings.coag_mode)
                {
                    if (!pedal_isBipolarAuto())
                    {
                        appFsm_enterState(AppDefs_EsuState_Idle);
                    }
                }
                else
                {
                    if (!coagPressed)
                    {
                        appFsm_enterState(AppDefs_EsuState_Idle);
                    }
                }
                break;

            /* ---- Polypectomy CUT phase ---- */
            case AppDefs_EsuState_PolypectomyCut:
                adcMonitor_powerLoop(gSettings.cut_powerW);
                if (!cutPressed)
                {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            /* ---- Polypectomy COAG phase ---- */
            case AppDefs_EsuState_PolypectomyCoag:
                adcMonitor_powerLoop(gSettings.coag_powerW);
                if (!cutPressed)   /* pedal released during coag burst */
                {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
                break;

            /* RemAlarm and Error handled above via skipMainFsm */
            default:
                break;
        }
    }

    /* ------------------------------------------------------------------
     * Push status to Nextion only on change (reduce UART traffic)
     * ------------------------------------------------------------------ */
    powerDw = adcMonitor_getPowerDw();
    if ((gState != gLastSentState) || (powerDw != gLastSentPowerDw))
    {
        uartProto_pushStatus(gState, powerDw, gErrors);
        gLastSentState   = gState;
        gLastSentPowerDw = powerDw;
    }
}


void app_fsmPolyTick(void)
{
    /*
     * Called from TIM5 ISR at 100 Hz.
     * Only set flags — no HAL or RF calls here.
     * app_fsmProcess() services these flags from main context.
     */
    AppDefs_EsuState_e state = gState; /* single atomic read */

    if ((AppDefs_EsuState_PolypectomyCut  != state) &&
        (AppDefs_EsuState_PolypectomyCoag != state))
    {
        return;
    }

    gPolyTick++;

    if (AppDefs_EsuState_PolypectomyCut == state)
    {
        if (gPolyTick >= gPolyCutTicks)
        {
            gPolyTick          = 0U;
            gPolyTransitToCoag = true;
        }
    }
    else
    {
        if (gPolyTick >= gPolyCoagTicks)
        {
            gPolyTick         = 0U;
            gPolyTransitToCut = true;
        }
    }
}


void app_fsmBlendTick(void)
{
    /* Called from TIM2 ISR — rfGen_blendTickIsr() is ISR-safe (only CCR writes) */
    rfGen_blendTickIsr();
}


void app_fsmIdleIsr(void)
{
    /* Called from USART3 IRQ when IDLE line is detected */
    uartProto_cbIdleIsr();
}


AppDefs_EsuState_e app_fsmGetState(void)
{
    return gState;
}


uint8_t app_fsmGetErrors(void)
{
    return gErrors;
}