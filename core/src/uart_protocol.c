/**
 * @file   uart_protocol.c
 * @brief  USART3 DMA + IDLE-line Nextion protocol driver.
 */

#include "uart_protocol.h"
#include <string.h>
#include <stdio.h>

/* ----------------------------------------------------------------
 * Internals
 * ----------------------------------------------------------------*/
static UART_HandleTypeDef *_huart = NULL;
static uint8_t  _dma_buf[UART_PROTO_RX_BUF];
static uint8_t  _pkt_buf[UART_PROTO_RX_BUF];
static volatile bool     _pkt_ready = false;
static volatile uint16_t _pkt_len   = 0;

/* Nextion 3-byte terminator */
static const uint8_t _TERM[3] = {0xFF, 0xFF, 0xFF};

/* DMA handle — must match the DMA stream configured for USART3 RX
 * in the CubeMX / MX_DMA_Init code.  Declared extern in header. */
DMA_HandleTypeDef hdma_usart3_rx;

/* ----------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------*/

void uart_proto_init(UART_HandleTypeDef *huart) {
    
    _huart     = huart;
    _pkt_ready = false;
    _pkt_len   = 0;

    /* IDLE interrupt fires after the last byte of a burst */
    __HAL_UART_ENABLE_IT(_huart, UART_IT_IDLE);

    /* DMA in circular mode — buffer never empties, IDLE tells us when
     * the sender has stopped. */
    HAL_UART_Receive_DMA(_huart, _dma_buf, UART_PROTO_RX_BUF);
}

void uart_proto_idle_isr(void) {
    /* How many bytes did DMA capture since the last IDLE? */
    uint16_t remaining = (uint16_t)__HAL_DMA_GET_COUNTER(&hdma_usart3_rx);
    uint16_t received  = UART_PROTO_RX_BUF - remaining;

    if (received == 0 || received > UART_PROTO_RX_BUF) return;

    /* Snapshot and validate */
    memcpy(_pkt_buf, _dma_buf, received);
    _pkt_len = received;

    if (received == NEXTION_PKT_SIZE && _pkt_buf[0] == NEXTION_PKT_HEADER) {
        /* XOR checksum over bytes [0 .. N-2] */
        uint8_t xor = 0;
        for (uint8_t i = 0; i < NEXTION_PKT_SIZE - 1U; i++) {
            xor ^= _pkt_buf[i];
        }
        if (xor == _pkt_buf[NEXTION_PKT_SIZE - 1U]) {
            _pkt_ready = true;
        }
        /* Bad checksum → silently discard; display will resend on next touch */
    }
}

bool uart_proto_get_packet(ESU_Packet_t *pkt) {
    if (!_pkt_ready) return false;
    memcpy(pkt, _pkt_buf, sizeof(ESU_Packet_t));
    _pkt_ready = false;
    return true;
}

/* ----------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------*/
static void _tx(const char *buf, uint16_t len) {
    if (_huart == NULL || len == 0) return;
    HAL_UART_Transmit(_huart, (uint8_t *)buf, len, UART_PROTO_TX_TIMEOUT);
}

static void _append_term(char *buf, int len, int max) {
    if (len + 3 < max) {
        buf[len]     = (char)0xFF;
        buf[len + 1] = (char)0xFF;
        buf[len + 2] = (char)0xFF;
        _tx(buf, (uint16_t)(len + 3));
    }
}

/* ----------------------------------------------------------------
 * Send commands
 * ----------------------------------------------------------------*/

void uart_proto_send_val(const char *component, int32_t value) {
    char buf[56];
    int  len = snprintf(buf, sizeof(buf), "%s.val=%ld", component, (long)value);
    if (len > 0) _append_term(buf, len, (int)sizeof(buf));
}

void uart_proto_send_txt(const char *component, const char *text) {
    char buf[56];
    int  len = snprintf(buf, sizeof(buf), "%s.txt=\"%s\"", component, text);
    if (len > 0) _append_term(buf, len, (int)sizeof(buf));
}

void uart_proto_send_page(const char *page_name) {
    char buf[40];
    int  len = snprintf(buf, sizeof(buf), "page %s", page_name);
    if (len > 0) _append_term(buf, len, (int)sizeof(buf));
}

void uart_proto_push_status(ESU_State_e state, uint16_t power_dw, uint8_t errors) {
    uart_proto_send_val("state", (int32_t)state);
    uart_proto_send_val("pwr",   (int32_t)power_dw);
    uart_proto_send_val("err",   (int32_t)errors);
}