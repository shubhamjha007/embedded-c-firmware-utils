#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>

/*
 * Frame format:
 *   [START 0xAA] [LEN] [PAYLOAD ... LEN bytes] [CRC_HI] [CRC_LO]
 * CRC-16/CCITT-FALSE is computed over LEN and PAYLOAD (not START).
 * LEN must be 1..PKT_MAX_PAYLOAD.
 */
#define PKT_START_BYTE  0xAAu
#define PKT_MAX_PAYLOAD 64u
#define PKT_OVERHEAD    4u /* start + len + 2 crc bytes */

typedef enum {
    PKT_NONE = 0,     /* need more bytes            */
    PKT_COMPLETE,     /* valid frame in parser      */
    PKT_ERR_LENGTH,   /* LEN was 0 or too large     */
    PKT_ERR_CRC       /* checksum mismatch          */
} pkt_result_t;

typedef enum {
    PKT_WAIT_START = 0,
    PKT_READ_LEN,
    PKT_READ_PAYLOAD,
    PKT_READ_CRC_HI,
    PKT_READ_CRC_LO
} pkt_state_t;

typedef struct {
    pkt_state_t state;
    uint8_t len;
    uint8_t index;
    uint8_t payload[PKT_MAX_PAYLOAD];
    uint16_t crc_calc;
    uint16_t crc_rx;
} pkt_parser_t;

void         pkt_parser_init(pkt_parser_t *p);
/* Feed one received byte. After PKT_COMPLETE, p->payload / p->len are valid
 * until the next frame's payload bytes arrive. */
pkt_result_t pkt_parser_feed(pkt_parser_t *p, uint8_t byte);
/* Build a frame into `out`. Returns frame length, or 0 on bad arguments. */
size_t       pkt_encode(const uint8_t *payload, size_t len,
                        uint8_t *out, size_t out_size);

#endif
