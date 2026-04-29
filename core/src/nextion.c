/**
 * ╔═══════════════════════════════════════════════════════════════╗
 * ║                   Electrosurgical Unit                        ║
 * ╚═══════════════════════════════════════════════════════════════╝
 * @file    nextion.c
 * @brief   Nextion display USART3 DMA + IDLE driver.
 */

#include "nextion.h"

/* Private variables ---------------------------------------------------------*/
static UART_HandleTypeDef *_huart = NULL;
static uint8_t _dma_buf[NEXTION_RX_BUF_SIZE];
static uint8_t _pkt_buf[NEXTION_RX_BUF_SIZE];
static volatile uint16_t _pkt_len = 0;
static volatile bool _pkt_ready = false;
DMA_HandleTypeDef hdma_usart3_rx;

// Nextion termination bytes
static const uint8_t _term[3] = {0xFF, 0xFF, 0xFF};


/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/

/**
 * @brief  Initialise Nextion driver
 *         Enables IDLE interrupt and starts DMA circular receive
 * @param  pHuart   Must be &huart3 (USART3, 9600 baud)
 * @retval None
 */
void nextion_init(UART_HandleTypeDef *pHuart) {
    
    _huart = pHuart;
    _pkt_ready = false;
    _pkt_len   = 0;

    // Enable IDLE line interrupt so we know when a burst is done
    __HAL_UART_ENABLE_IT(_huart, UART_IT_IDLE);

    /* Start DMA circular receive into _dma_buf */
    HAL_UART_Receive_DMA(_huart, _dma_buf, NEXTION_RX_BUF_SIZE);
}

/**
 * @brief  Called from USART3_IRQHandler on IDLE flag
 *         Calculates how many bytes DMA received since last call,
 *         copies them and validates as ESU_Packet_t.
 */
void nextion_idleCallback(void) {
    
    // Number of bytes still expected by DMA (decrements as data arrives)
    uint16_t dma_remaining = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    uint16_t received = NEXTION_RX_BUF_SIZE - dma_remaining;

    if (0 == received || received > NEXTION_RX_BUF_SIZE) {
        return;
    }

    memcpy(_pkt_buf, _dma_buf, received);
    _pkt_len = received;

    // Validate: correct size and header
    if (NEXTION_PKT_SIZE == received && NEXTION_PKT_HEADER == _pkt_buf[0]) {
        // Verify XOR checksum over bytes [0..10]
        uint8_t xor = 0;
        for (uint8_t i = 0; i < NEXTION_PKT_SIZE - 1U; i++) {
            xor ^= _pkt_buf[i];
        }
        if (xor == _pkt_buf[NEXTION_PKT_SIZE - 1U]) {
            _pkt_ready = true;
        }
    }
}

/**
 * @brief  Check whether a valid settings packet has arrived
 * @param[out] pPkt Filled with the received settings
 * @retval true if a new packet is ready (flag cleared on return)
 */
bool nextion_getPacket(ESU_Packet_t *pPkt)
{
    if (true != _pkt_ready) {
        return false;
    }
    memcpy(pPkt, _pkt_buf, sizeof(ESU_Packet_t));
    _pkt_ready = false;
    return true;
}

/**
 * @brief  Send a Nextion component value update.
 *         ex: nextion_sendVal("state", 1) → "state.val=1\xff\xff\xff"
 * @param[out] Component name string
 * @param[in] value Value to send
 */
void nextion_sendVal(const char *pComponent, int32_t value)
{
    if (NULL == _huart) {
        return;
    }

    char buf[48];
    int  len = snprintf(buf, sizeof(buf), "%s.val=%ld", pComponent, (long)value);
    if (len <= 0) return;

    // Append Nextion 3-byte terminator
    if ((size_t)(len + 3) < sizeof(buf)) {
        buf[len]     = (char)0xFF;
        buf[len + 1] = (char)0xFF;
        buf[len + 2] = (char)0xFF;
        HAL_UART_Transmit(_huart, (uint8_t *)buf, (uint16_t)(len + 3), NEXTION_TX_TIMEOUT_MS);
    }
}

/**
 * @brief  Send a Nextion page-change command.
 *         ex: nextion_sendPage("pageMain") → "page pageMain\xff\xff\xff"
 * @param[out] pPageName   Name of the Nextion page to switch to
 */
void nextion_sendPage(const char *pPageName)
{
    if (NULL == _huart) {
        return;
    }

    char buf[32];
    int  len = snprintf(buf, sizeof(buf), "page %s", pPageName);
    if (len <= 0) return;

    if ((size_t)(len + 3) < sizeof(buf)) {
        buf[len]     = (char)0xFF;
        buf[len + 1] = (char)0xFF;
        buf[len + 2] = (char)0xFF;
        HAL_UART_Transmit(_huart, (uint8_t *)buf, (uint16_t)(len + 3), NEXTION_TX_TIMEOUT_MS);
    }
}

/**
 * @brief  Push current ESU state to the display
 *         Sends state, measured power and error flags
 * @param[in] state     Current ESU state
 * @param[in] power_dw  Measured power in deci-Watts
 * @param[in] errors    Error flags bitmask
 */
void nextion_pushStatus(uint8_t state, uint16_t power_dw, uint8_t errors)
{
    nextion_sendVal("state",  (int32_t)state);
    nextion_sendVal("pwr",    (int32_t)power_dw);
    nextion_sendVal("err",    (int32_t)errors);
}