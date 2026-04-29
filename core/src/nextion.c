/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file    nextion.c
 * @brief   Nextion display USART3 DMA + IDLE driver.
 *
 * @details
 *  DMA1 Stream1 Channel4 runs in circular mode; the buffer is
 *  never restarted. On each IDLE interrupt the driver computes
 *  how many new bytes arrived since the last interrupt by tracking
 *  a ring-buffer head (gLastPos). This correctly handles wrap-around
 *  and back-to-back frames.
 *
 *  Reception:    USART3_RX PB11, DMA1 Stream1 Ch4, circular.
 *  Transmission: USART3_TX PB10, blocking HAL_UART_Transmit.
 *
 *  Every Nextion command must be terminated with 0xFF 0xFF 0xFF.
 */

#include "nextion.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Module state                                                                */
/* -------------------------------------------------------------------------- */

static UART_HandleTypeDef* gUart = NULL;

// DMA circular receive buffer
static uint8_t gDmaBuf[NEXTION_RX_BUF_SIZE];

// Copy of the last valid packet
static uint8_t           gPktBuf[NEXTION_RX_BUF_SIZE];
static volatile uint16_t gPktLen   = 0U;
static volatile bool     gPktReady = false;

/*
 * Ring-buffer head: the position in gDmaBuf where the LAST processed
 * byte ended.  Updated only inside the IDLE ISR (no race).
 * Range: [0 .. NEXTION_RX_BUF_SIZE - 1].
 */
static volatile uint16_t gLastPos = 0U;

static const uint8_t gTerm[3U] = {0xFFU, 0xFFU, 0xFFU};

/* -------------------------------------------------------------------------- */
/* Private helpers                                                             */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Transmit raw bytes over USART3.
 * @param  pBuf  Data buffer.
 * @param  len   Number of bytes.
 * @retval None
 */
static void nextion_txRaw(const uint8_t *pBuf, uint16_t len) {
    if ((NULL != gUart) && (NULL != pBuf) && (0U != len)) {
        (void)HAL_UART_Transmit(gUart, (uint8_t *)pBuf, len, NEXTION_TX_TIMEOUT_MS);
    }
}

/**
 * @brief  Append Nextion 0xFF 0xFF 0xFF terminator and transmit.
 * @param  pBuf    Command buffer.
 * @param  len     Current command length (bytes without terminator).
 * @param  maxLen  Buffer capacity.
 * @retval None
 */
static void nextion_appendTerminator(char *pBuf, uint16_t len, uint16_t maxLen) {
    if ((NULL != pBuf) && ((uint16_t)(len + 3U) <= maxLen)) {
        pBuf[len]      = (char)gTerm[0U];
        pBuf[len + 1U] = (char)gTerm[1U];
        pBuf[len + 2U] = (char)gTerm[2U];
        nextion_txRaw((const uint8_t *)pBuf, (uint16_t)(len + 3U));
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                  */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Initialise Nextion driver.
 * @param  pUart  USART3 handle (must already be configured by CubeMX).
 * @retval None
 */
void nextion_init(UART_HandleTypeDef *pUart) {
    gUart     = pUart;
    gPktReady = false;
    gPktLen   = 0U;
    gLastPos  = 0U;

    if (NULL != gUart) {
        __HAL_UART_ENABLE_IT(gUart, UART_IT_IDLE);
        (void)HAL_UART_Receive_DMA(gUart, gDmaBuf, NEXTION_RX_BUF_SIZE);
    }
}

/**
 * @brief  USART3 IDLE-line ISR callback.
 * @details
 *  Computes the number of bytes received since the last call using a
 *  ring-buffer head pointer, so back-to-back frames are handled correctly
 *  without restarting the DMA.
 * @param  None
 * @retval None
 */
void nextion_cbIdleIsr(void) {
    uint16_t remaining;
    uint16_t currentPos;
    uint16_t received;
    uint16_t index;
    uint8_t  xorValue;

    if (NULL == gUart) {
        return;
    }

    remaining  = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    currentPos = (uint16_t)(NEXTION_RX_BUF_SIZE - remaining);

    // Compute delta, handling circular wrap-around
    if (currentPos >= gLastPos) {
        received = currentPos - gLastPos;
    } else {
        received = (uint16_t)(NEXTION_RX_BUF_SIZE - gLastPos) + currentPos;
    }

    if (0U == received) {
        return;
    }

    // Linear copy from gDmaBuf into gPktBuf, respecting wrap
    for (index = 0U; index < received; index++) {
        uint16_t srcIdx = (uint16_t)((gLastPos + index) % NEXTION_RX_BUF_SIZE);
        gPktBuf[index]  = gDmaBuf[srcIdx];
    }
    gPktLen  = received;
    gLastPos = currentPos;

    // Validate length and XOR checksum
    if ((received == (uint16_t)NEXTION_PKT_SIZE) &&
        (NEXTION_PKT_HEADER == gPktBuf[0U])) {
        xorValue = 0U;
        for (index = 0U; index < (uint16_t)(NEXTION_PKT_SIZE - 1U); index++) {
            xorValue ^= gPktBuf[index];
        }

        if (xorValue == gPktBuf[(uint16_t)(NEXTION_PKT_SIZE - 1U)]) {
            gPktReady = true;
        }
    }
}

/**
 * @brief  Check whether a valid settings packet has arrived.
 * @param  pPkt  Receives the decoded settings packet.
 * @retval true if a new packet is ready, false otherwise.
 */
bool nextion_getPacket(AppDefs_EsuPacket_t *pPkt) {
    if ((NULL == pPkt) || (false == gPktReady)) {
        return false;
    }

    (void)memcpy(pPkt, gPktBuf, sizeof(AppDefs_EsuPacket_t));
    gPktReady = false;

    return true;
}

/**
 * @brief  Send a Nextion numeric component update.
 * @param  pComponent  Nextion component name (e.g. "state").
 * @param  value       Value to assign to .val.
 * @retval None
 */
void nextion_sendVal(const char *pComponent, int32_t value) {
    char    buffer[48];
    int32_t len;

    if ((NULL == gUart) || (NULL == pComponent)) {
        return;
    }

    len = snprintf(buffer, sizeof(buffer), "%s.val=%ld", pComponent, (long)value);
    if (0 < len) {
        nextion_appendTerminator(buffer, (uint16_t)len, (uint16_t)sizeof(buffer));
    }
}

/**
 * @brief  Send a Nextion page-change command.
 * @param  pPageName  Target page name.
 * @retval None
 */
void nextion_sendPage(const char *pPageName) {
    char    buffer[32];
    int32_t len;

    if ((NULL == gUart) || (NULL == pPageName)) {
        return;
    }

    len = snprintf(buffer, sizeof(buffer), "page %s", pPageName);
    if (0 < len) {
        nextion_appendTerminator(buffer, (uint16_t)len, (uint16_t)sizeof(buffer));
    }
}

/**
 * @brief  Push complete ESU status to the display.
 * @param  state    Current ESU state.
 * @param  powerDw  Measured output power in deci-watts.
 * @param  errors   Error bitmask (ESU_ERR_*).
 * @retval None
 */
void nextion_pushStatus(AppDefs_EsuState_e state, uint16_t powerDw, uint8_t errors) {
    nextion_sendVal("state", (int32_t)state);
    nextion_sendVal("pwr",   (int32_t)powerDw);
    nextion_sendVal("err",   (int32_t)errors);
}