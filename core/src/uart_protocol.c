/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 *
 * @file   uart_protocol.c
 * @brief  USART3 DMA + IDLE-line Nextion protocol driver.
 */

#include "uart_protocol.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Module state                                                               */
/* -------------------------------------------------------------------------- */

static UART_HandleTypeDef* g_pUart = NULL;
static uint8_t gRxBuf[UART_PROTO_RX_BUF_SIZE];
static uint8_t gPktBuf[UART_PROTO_RX_BUF_SIZE];
static volatile bool gPktReady = false;
static volatile uint16_t gPktLen = 0U;

static const uint8_t gTerminator[3U] = {0xFFU, 0xFFU, 0xFFU};

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

/**
  * @brief Initialise UART protocol module.
  * @param pUart USART3 handle.
  * @retval 0 on success, error code otherwise
  */
void uartProto_init(UART_HandleTypeDef* pUart)
{
    g_pUart = pUart;
    gPktReady = false;
    gPktLen = 0U;

    if (NULL != g_pUart) {
        __HAL_UART_ENABLE_IT(g_pUart, UART_IT_IDLE);
        (void)HAL_UART_Receive_DMA(g_pUart, gRxBuf, UART_PROTO_RX_BUF_SIZE);
    }
}

/**
  * @brief USART3 IDLE-line callback.
  * @param None
  * @retval 0 on success, error code otherwise
  */
void uartProto_cbIdleIsr(void)
{
    uint16_t remaining = 0U;
    uint16_t received = 0U;
    uint8_t xorValue = 0U;
    uint16_t index = 0U;

    if (NULL == g_pUart) {
        return;
    }

    remaining = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    received = (uint16_t)(UART_PROTO_RX_BUF_SIZE - remaining);

    if ((0U == received) || (received > UART_PROTO_RX_BUF_SIZE)) {
        return;
    }

    (void)memcpy(gPktBuf, gRxBuf, received);
    gPktLen = received;

    /*
     * Validate the packet length and checksum.
     * Packet layout depends on AppDefs_EsuPacket_t definition.
     */
    if (received == (uint16_t)sizeof(AppDefs_EsuPacket_t)) {
        for (index = 0U; index < (uint16_t)(sizeof(AppDefs_EsuPacket_t) - 1U); index++) {
            xorValue ^= gPktBuf[index];
        }

        if (xorValue == gPktBuf[(uint16_t)(sizeof(AppDefs_EsuPacket_t) - 1U)]) {
            gPktReady = true;
        }
    }
}

/**
  * @brief Check whether a valid settings packet has arrived.
  * @param pPkt Receives the decoded packet if available.
  * @retval true if a fresh packet is ready, false otherwise.
  */
bool uartProto_getPacket(AppDefs_EsuPacket_t* pPkt)
{
    if ((NULL == pPkt) || (false == gPktReady)) {
        return false;
    }

    (void)memcpy(pPkt, gPktBuf, sizeof(AppDefs_EsuPacket_t));
    gPktReady = false;

    return true;
}

/* -------------------------------------------------------------------------- */
/* Private helpers                                                            */
/* -------------------------------------------------------------------------- */

/**
  * @brief Transmit raw bytes over USART3.
  * @param pBuf Byte buffer.
  * @param len Number of bytes to send.
  * @retval 0 on success, error code otherwise
  */
static void uartProto_txRaw(const uint8_t* pBuf, uint16_t len)
{
    if ((NULL != g_pUart) && (NULL != pBuf) && (0U != len)) {
        (void)HAL_UART_Transmit(g_pUart, (uint8_t*)pBuf, len, UART_PROTO_TX_TIMEOUT_MS);
    }
}

/**
  * @brief Append Nextion terminator and transmit the buffer.
  * @param pBuf Command buffer.
  * @param len Current command length.
  * @param maxLen Maximum buffer size.
  * @retval 0 on success, error code otherwise
  */
static void uartProto_appendTerminator(char* pBuf, uint16_t len, uint16_t maxLen)
{
    if ((NULL != pBuf) && ((uint16_t)(len + 3U) <= maxLen)) {
        pBuf[len] = (char)gTerminator[0U];
        pBuf[len + 1U] = (char)gTerminator[1U];
        pBuf[len + 2U] = (char)gTerminator[2U];
        uartProto_txRaw((const uint8_t*)pBuf, (uint16_t)(len + 3U));
    }
}

/* -------------------------------------------------------------------------- */
/* Send commands                                                              */
/* -------------------------------------------------------------------------- */

/**
  * @brief Send a Nextion numeric component update.
  * @param pComponent Nextion component name.
  * @param value Numeric value to send.
  * @retval 0 on success, error code otherwise
  */
void uartProto_sendVal(const char* pComponent, int32_t value)
{
    char buffer[56];
    int32_t length = 0;

    if (NULL == pComponent) {
        return;
    }

    length = snprintf(buffer, sizeof(buffer), "%s.val=%ld", pComponent, (long)value);
    if (0 < length) {
        uartProto_appendTerminator(buffer, (uint16_t)length, (uint16_t)sizeof(buffer));
    }
}

/**
  * @brief Send a Nextion text component update.
  * @param pComponent Nextion component name.
  * @param pText Text to send.
  * @retval 0 on success, error code otherwise
  */
void uartProto_sendTxt(const char* pComponent, const char* pText)
{
    char buffer[56];
    int32_t length = 0;

    if ((NULL == pComponent) || (NULL == pText)) {
        return;
    }

    length = snprintf(buffer, sizeof(buffer), "%s.txt=\"%s\"", pComponent, pText);
    if (0 < length) {
        uartProto_appendTerminator(buffer, (uint16_t)length, (uint16_t)sizeof(buffer));
    }
}

/**
  * @brief Navigate the Nextion display to a named page.
  * @param pPageName Target page name.
  * @retval 0 on success, error code otherwise
  */
void uartProto_sendPage(const char* pPageName)
{
    char buffer[40];
    int32_t length = 0;

    if (NULL == pPageName) {
        return;
    }

    length = snprintf(buffer, sizeof(buffer), "page %s", pPageName);
    if (0 < length) {
        uartProto_appendTerminator(buffer, (uint16_t)length, (uint16_t)sizeof(buffer));
    }
}

/**
  * @brief Push a complete ESU status update to the Nextion.
  * @param state ESU state.
  * @param powerDw Measured output power in deci-watts.
  * @param errors Error bitmask.
  * @retval 0 on success, error code otherwise
  */
void uartProto_pushStatus(AppDefs_EsuState_e state, uint16_t powerDw, uint8_t errors)
{
    uartProto_sendVal("state", (int32_t)state);
    uartProto_sendVal("pwr", (int32_t)powerDw);
    uartProto_sendVal("err", (int32_t)errors);
}