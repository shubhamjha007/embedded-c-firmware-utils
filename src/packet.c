#include "packet.h"
#include "crc16.h"

void pkt_parser_init(pkt_parser_t *p)
{
    p->state = PKT_WAIT_START;
    p->len = 0u;
    p->index = 0u;
    p->crc_calc = CRC16_INIT;
    p->crc_rx = 0u;
}

pkt_result_t pkt_parser_feed(pkt_parser_t *p, uint8_t byte)
{
    switch (p->state) {
    case PKT_WAIT_START:
        if (byte == PKT_START_BYTE) {
            p->crc_calc = CRC16_INIT;
            p->state = PKT_READ_LEN;
        }
        /* any other byte is line noise: ignore and keep hunting for START */
        return PKT_NONE;

    case PKT_READ_LEN:
        if (byte == 0u || byte > PKT_MAX_PAYLOAD) {
            p->state = PKT_WAIT_START; /* resync */
            return PKT_ERR_LENGTH;
        }
        p->len = byte;
        p->index = 0u;
        p->crc_calc = crc16_update(p->crc_calc, byte);
        p->state = PKT_READ_PAYLOAD;
        return PKT_NONE;

    case PKT_READ_PAYLOAD:
        p->payload[p->index++] = byte;
        p->crc_calc = crc16_update(p->crc_calc, byte);
        if (p->index == p->len) {
            p->state = PKT_READ_CRC_HI;
        }
        return PKT_NONE;

    case PKT_READ_CRC_HI:
        p->crc_rx = (uint16_t)((uint16_t)byte << 8);
        p->state = PKT_READ_CRC_LO;
        return PKT_NONE;

    case PKT_READ_CRC_LO:
        p->crc_rx = (uint16_t)(p->crc_rx | byte);
        p->state = PKT_WAIT_START;
        return (p->crc_rx == p->crc_calc) ? PKT_COMPLETE : PKT_ERR_CRC;
    }

    p->state = PKT_WAIT_START; /* unreachable; defensive */
    return PKT_NONE;
}

size_t pkt_encode(const uint8_t *payload, size_t len,
                  uint8_t *out, size_t out_size)
{
    if (payload == NULL || out == NULL || len == 0u || len > PKT_MAX_PAYLOAD ||
        out_size < len + PKT_OVERHEAD) {
        return 0u;
    }
    size_t n = 0u;
    uint16_t crc = CRC16_INIT;

    out[n++] = PKT_START_BYTE;
    out[n++] = (uint8_t)len;
    crc = crc16_update(crc, (uint8_t)len);
    for (size_t i = 0u; i < len; i++) {
        out[n++] = payload[i];
        crc = crc16_update(crc, payload[i]);
    }
    out[n++] = (uint8_t)(crc >> 8);
    out[n++] = (uint8_t)(crc & 0xFFu);
    return n;
}
