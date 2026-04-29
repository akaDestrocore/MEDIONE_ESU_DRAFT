/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file    nextion.c
 * @brief   Nextion display USART3 DMA + IDLE driver.
 */

#include "nextion.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Module state                                                               */
/* -------------------------------------------------------------------------- */

static UART_HandleTypeDef* gUart = NULL;
static uint8_t gDmaBuf[NEXTION_RX_BUF_SIZE];
static uint8_t gPktBuf[NEXTION_RX_BUF_SIZE];
static volatile uint16_t gPktLen = 0U;
static volatile bool gPktReady = false;

static const uint8_t gTerm[3U] = {0xFFU, 0xFFU, 0xFFU};

/* -------------------------------------------------------------------------- */
/* Private helpers                                                            */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Transmit raw bytes over USART3.
 * @param  pBuf Data buffer.
 * @param  len Number of bytes.
 * @retval None
 */
static void nextion_txRaw(const uint8_t* pBuf, uint16_t len)
{
    if ((NULL != gUart) && (NULL != pBuf) && (0U != len)) {
        (void)HAL_UART_Transmit(gUart, (uint8_t*)pBuf, len, NEXTION_TX_TIMEOUT_MS);
    }
}

/**
 * @brief  Append Nextion terminator and transmit the buffer.
 * @param  pBuf Command buffer.
 * @param  len Current command length.
 * @param  maxLen Buffer capacity.
 * @retval None
 */
static void nextion_appendTerminator(char* pBuf, uint16_t len, uint16_t maxLen)
{
    if ((NULL != pBuf) && ((uint16_t)(len + 3U) <= maxLen)) {
        pBuf[len] = (char)gTerm[0U];
        pBuf[len + 1U] = (char)gTerm[1U];
        pBuf[len + 2U] = (char)gTerm[2U];
        nextion_txRaw((const uint8_t*)pBuf, (uint16_t)(len + 3U));
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
 * @brief  Initialise Nextion driver.
 * @param  pUart USART3 handle.
 * @retval None
 */
void nextion_init(UART_HandleTypeDef* pUart)
{
    gUart = pUart;
    gPktReady = false;
    gPktLen = 0U;

    if (NULL != gUart) {
        __HAL_UART_ENABLE_IT(gUart, UART_IT_IDLE);
        (void)HAL_UART_Receive_DMA(gUart, gDmaBuf, NEXTION_RX_BUF_SIZE);
    }
}

/**
 * @brief  USART3 IDLE-line callback.
 * @param  None
 * @retval None
 */
void nextion_cbIdleIsr(void)
{
    uint16_t remaining = 0U;
    uint16_t received = 0U;
    uint16_t index = 0U;
    uint8_t xorValue = 0U;

    if (NULL == gUart) {
        return;
    }

    remaining = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    received = (uint16_t)(NEXTION_RX_BUF_SIZE - remaining);

    if ((0U == received) || (received > NEXTION_RX_BUF_SIZE)) {
        return;
    }

    (void)memcpy(gPktBuf, gDmaBuf, received);
    gPktLen = received;

    if ((received == (uint16_t)NEXTION_PKT_SIZE) &&
        (gPktBuf[0U] == NEXTION_PKT_HEADER)) {
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
 * @param  pPkt Receives the decoded settings packet.
 * @retval true if a new packet is ready, false otherwise.
 */
bool nextion_getPacket(AppDefs_EsuPacket_t* pPkt)
{
    if ((NULL == pPkt) || (false == gPktReady)) {
        return false;
    }

    (void)memcpy(pPkt, gPktBuf, sizeof(AppDefs_EsuPacket_t));
    gPktReady = false;

    return true;
}

/**
 * @brief  Send a Nextion component value update.
 * @param  pComponent Nextion component name.
 * @param  value Value to send.
 * @retval None
 */
void nextion_sendVal(const char* pComponent, int32_t value)
{
    char buffer[48];
    int32_t len = 0;

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
 * @param  pPageName Page name.
 * @retval None
 */
void nextion_sendPage(const char* pPageName)
{
    char buffer[32];
    int32_t len = 0;

    if ((NULL == gUart) || (NULL == pPageName)) {
        return;
    }

    len = snprintf(buffer, sizeof(buffer), "page %s", pPageName);
    if (0 < len) {
        nextion_appendTerminator(buffer, (uint16_t)len, (uint16_t)sizeof(buffer));
    }
}

/**
 * @brief  Push current ESU state to the display.
 * @param  state Current ESU state.
 * @param  powerDw Measured power in deci-watts.
 * @param  errors Error flags bitmask.
 * @retval None
 */
void nextion_pushStatus(AppDefs_EsuState_e state, uint16_t powerDw, uint8_t errors)
{
    nextion_sendVal("state", (int32_t)state);
    nextion_sendVal("pwr", (int32_t)powerDw);
    nextion_sendVal("err", (int32_t)errors);
}