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
 */

#include "app_fsm.h"


/* Private variables ---------------------------------------------------------*/
static AppDefs_EsuState_e gState = AppDefs_EsuState_Idle;
static AppDefs_Channel_e gChannel = AppDefs_Channel_Mono1;
static AppDefs_EsuSettings_t gSettings = {0};
static uint8_t gErrors = ESU_ERR_NONE;

static uint16_t gPolyTick = 0U;
static uint16_t gPolyCutTicks = 4U;    // 40 ms
static uint16_t gPolyCoagTicks = 50U;   // 500 ms

static AppDefs_EsuState_e gLastSentState = (AppDefs_EsuState_e)0xFF;
static uint16_t gLastSentPowerDw = 0xFFFFU;

/* -------------------------------------------------------------------------- */
/* Private helpers                                                            */
/* -------------------------------------------------------------------------- */

/**
  * @brief Disable all active outputs.
  * @param None
  * @retval 0 on success, error code otherwise
  */
static void appFsm_allOff(void)
{
    rf_genDisableAll();
    rf_genAudioStop();
    adcMonitor_dacZero();
}

/**
  * @brief Enter a new application state.
  * @param newState New ESU state.
  * @retval 0 on success, error code otherwise
  */
static void appFsm_enterState(AppDefs_EsuState_e newState)
{
    switch (gState) {
        case AppDefs_EsuState_CutActive:
        case AppDefs_EsuState_PolypectomyCut: {
            rf_genDisableCut();
            rf_genAudioStop();
            adcMonitor_dacZero();
            break;
        }

        case AppDefs_EsuState_CoagActive:
        case AppDefs_EsuState_PolypectomyCoag: {
            rf_genDisableCoag();
            rf_genAudioStop();
            adcMonitor_dacZero();
            break;
        }

        case AppDefs_EsuState_BipolarCut:
        case AppDefs_EsuState_BipolarCoag: {
            rf_genDisableBipolar();
            rf_genAudioStop();
            adcMonitor_dacZero();
            break;
        }

        default:{
            break;
        }
    }

    gState = newState;

    switch (newState) {
        case AppDefs_EsuState_Idle: {
            appFsm_allOff();
            gErrors &= (uint8_t)~(ESU_ERR_OVERCURRENT | ESU_ERR_OVERTEMP);
            uart_protoSendPage("mainPage");
            break;
        }

        case AppDefs_EsuState_CutActive: {
            rf_genConfigureCut((AppDefs_CutMode_e)gSettings.cut_mode);
            rf_genCnableCut();
            rf_genAudioStart(true);
            uart_protoSendPage("activePage");
            break;
        }

        case AppDefs_EsuState_CoagActive: {
            rf_gen_configure_coag((AppDefs_CutMode_e)gSettings.coag_mode);
            rf_gen_enable_coag();
            rf_genAudioStart(false);
            uart_protoSendPage("activePage");
            break;
        }

        case AppDefs_EsuState_BipolarCut: {
            rf_genConfigureBipolarCut((AppDefs_BipolarCutMode_e)gSettings.cut_mode);
            rf_genEnableBipolar();
            rf_genAudioStart(true);
            break;
        }

        case AppDefs_EsuState_BipolarCoag: {
            rf_genConfigureBipolarCoag((AppDefs_BipolarCoagMode_e)gSettings.coag_mode);
            rf_genEnableBipolar();
            rf_genAudioStart(false);
            break;
        }

        case AppDefs_EsuState_PolypectomyCut: {
            gPolyTick = 0U;
            rf_genConfigureCut(AppDefs_CutMode_Polypectomy);
            rf_genCnableCut();
            rf_genAudioStart(true);
            break;
        }

        case AppDefs_EsuState_PolypectomyCoag: {
            break;
        }

        case AppDefs_EsuState_RemAlarm: {
            appFsm_allOff();
            gErrors |= ESU_ERR_REM_ALARM;
            uart_protoSendPage("remAlarmPage");
            break;
        }

        case AppDefs_EsuState_Error: {
            appFsm_allOff();
            uart_protoSendPage("errorPage");
            break;
        }

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
  * @brief  One-time initialisation. Call after all HAL and MX inits.
  * @param  pTimers RF generator timer bundle.
  * @param  pHadc ADC1 handle.
  * @param  pHdac DAC handle.
  * @param  pUartNextion USART3 handle.
  * @retval 0 on success, error code otherwise
  */
void app_fsm_init(const RFGen_Timers_t* pTimers,
                  ADC_HandleTypeDef* pHadc,
                  DAC_HandleTypeDef* pHdac,
                  UART_HandleTypeDef* pUartNextion)
{
    rf_gen_init(pTimers);
    adcMonitor_init(pHadc, pHdac);
    pedal_init();
    uart_proto_init(pUartNextion);

    memset(&gSettings, 0, sizeof(gSettings));
    gSettings.cut_mode = AppDefs_CutMode_Pure;
    gSettings.cut_level = 1U;
    gSettings.cut_powerW = 30U;
    gSettings.coag_mode = AppDefs_CoagMode_Soft;
    gSettings.coag_level = 1U;
    gSettings.coag_powerW = 30U;
    gSettings.poly_level = 1U;

    gChannel = AppDefs_Channel_Mono1;
    gState = AppDefs_EsuState_Idle;
    gErrors = ESU_ERR_NONE;
    gPolyTick = 0U;
    gPolyCutTicks = 4U;
    gPolyCoagTicks = POLY_COAG_TICKS(gSettings.poly_level);
    gLastSentState = (AppDefs_EsuState_e)0xFF;
    gLastSentPowerDw = 0xFFFFU;

    appFsm_allOff();

    uart_protoSendPage("mainPage");
}

/**
  * @brief  Main loop body. Call from while(1) in main().
  * @param  None
  * @retval 0 on success, error code otherwise
  */
void app_fsmProcess(void)
{
    AppDefs_EsuPacket_t pkt;
    bool isBipolar = false;
    Safety_Fault_e faults;
    bool cutPressed = false;
    bool coagPressed = false;

    adcMonitor_scan();
    pedal_update();

    if (uart_proto_get_packet(&pkt)) {
        if (AppDefs_EsuState_Idle == gState) {
            gChannel = (AppDefs_Channel_e)pkt.channel;
            gSettings.cut_mode = pkt.cut_mode;
            gSettings.cut_level = pkt.cut_level;
            gSettings.cut_powerW = pkt.cut_powerW;
            gSettings.coag_mode = pkt.coag_mode;
            gSettings.coag_level = pkt.coag_level;
            gSettings.coag_powerW = pkt.coag_powerW;
            gSettings.poly_level = pkt.poly_level;

            gPolyCutTicks = 4U;
            gPolyCoagTicks = POLY_COAG_TICKS(gSettings.poly_level);
        }
    }

    isBipolar = (AppDefs_Channel_Bipolar == gChannel);
    faults = safety_check(isBipolar);

    if (0U != (faults & SAFETY_FAULT_REM)) {
        if ((AppDefs_EsuState_RemAlarm != gState) && (AppDefs_EsuState_Error != gState)) {
            appFsm_enterState(AppDefs_EsuState_RemAlarm);
        }
        goto pushStatus;
    }

    if ((AppDefs_EsuState_RemAlarm == gState) && (0U == (faults & SAFETY_FAULT_REM))) {
        appFsm_enterState(AppDefs_EsuState_Idle);
    }

    if (0U != (faults & SAFETY_FAULT_OC)) {
        gErrors |= ESU_ERR_OVERCURRENT;
        appFsm_enterState(AppDefs_EsuState_Error);
        goto pushStatus;
    }

    if (0U != (faults & SAFETY_FAULT_OT)) {
        gErrors |= ESU_ERR_OVERTEMP;
        appFsm_enterState(AppDefs_EsuState_Error);
        goto pushStatus;
    }

    if (AppDefs_EsuState_Error == gState) {
        goto pushStatus;
    }

    cutPressed = pedal_cut_pressed(gChannel);
    coagPressed = pedal_coag_pressed(gChannel);

    switch (gState) {
        case AppDefs_EsuState_Idle:
            if (true == isBipolar) {
                if ((true == pedal_bipolar_auto()) &&
                    (AppDefs_BipolarCoagMode_AutoStart == gSettings.coag_mode)) {
                    appFsm_enterState(AppDefs_EsuState_BipolarCoag);
                } else if (true == cutPressed) {
                    appFsm_enterState(AppDefs_EsuState_BipolarCut);
                } else if (true == coagPressed) {
                    appFsm_enterState(AppDefs_EsuState_BipolarCoag);
                } else {
                    // No action required
                }
            }
            else {
                if (true == cutPressed) {
                    if (AppDefs_CutMode_Polypectomy == gSettings.cut_mode) {
                        appFsm_enterState(AppDefs_EsuState_PolypectomyCut);
                    }
                    else {
                        appFsm_enterState(AppDefs_EsuState_CutActive);
                    }
                }
                else if (true == coagPressed) {
                    appFsm_enterState(AppDefs_EsuState_CoagActive);
                }
                else {
                    // No action required
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
            if (AppDefs_BipolarCoagMode_AutoStart == gSettings.coag_mode) {
                if (false == pedal_bipolar_auto()) {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
            }
            else {
                if (false == coagPressed) {
                    appFsm_enterState(AppDefs_EsuState_Idle);
                }
            }
            break;

        case AppDefs_EsuState_PolypectomyCut:
        case AppDefs_EsuState_PolypectomyCoag: {
            uint16_t targetPowerW;

            targetPowerW = (AppDefs_EsuState_PolypectomyCut == gState) ? gSettings.cut_powerW : gSettings.coag_powerW;

            adcMonitor_powerLoop(targetPowerW);

            if (false == cutPressed) {
                appFsm_enterState(AppDefs_EsuState_Idle);
            }
            break;
        }

        default: {
            break;
        }
    }

pushStatus:
    {
        uint16_t powerDw;

        powerDw = adcMonitor_getPowerDw();

        if ((gState != gLastSentState) || (powerDw != gLastSentPowerDw)) {
            uart_proto_push_status(gState, powerDw, gErrors);
            gLastSentState = gState;
            gLastSentPowerDw = powerDw;
        }
    }
}

/**
  * @brief  Polypectomy sub-state tick.
  * @param  None
  * @retval 0 on success, error code otherwise
  */
void app_fsmPolyTick(void)
{
    if ((AppDefs_EsuState_PolypectomyCut != gState) &&
        (AppDefs_EsuState_PolypectomyCoag != gState)) {
        return;
    }

    gPolyTick++;

    if (AppDefs_EsuState_PolypectomyCut == gState) {
        if (gPolyTick >= gPolyCutTicks) {
            gPolyTick = 0U;
            rf_genDisableCut();
            rf_gen_configure_coag(AppDefs_CoagMode_Soft);
            rf_gen_enable_coag();
            rf_genAudioStart(false);
            gState = AppDefs_EsuState_PolypectomyCoag;
        }
    }
    else {
        if (gPolyTick >= gPolyCoagTicks) {
            gPolyTick = 0U;
            rf_genDisableCoag();
            rf_genConfigureCut(AppDefs_CutMode_Polypectomy);
            rf_genCnableCut();
            rf_genAudioStart(true);
            gState = AppDefs_EsuState_PolypectomyCut;
        }
    }
}

/**
  * @brief  Blend envelope tick.
  * @param  None
  * @retval 0 on success, error code otherwise
  */
void app_fsmBlendTick(void)
{
    rf_genBlendTickIsr();
}

/**
  * @brief  USART3 IDLE line callback.
  * @param  None
  * @retval 0 on success, error code otherwise
  */
void app_fsmIdleIsr(void)
{
    uart_protoIdleIsr();
}

/**
  * @brief  Return current ESU state.
  * @param  None
  * @retval 0 on success, error code otherwise
  */
AppDefs_EsuState_e app_fsmGetState(void)
{
    return gState;
}

/**
  * @brief  Return current error bitmask.
  * @param  None
  * @retval 0 on success, error code otherwise
  */
uint8_t app_fsmGetErrors(void)
{
    return gErrors;
}