#include <stdio.h>
#include <string.h>
#include "ring_buffer.h"
#include "crc16.h"
#include "packet.h"

static int g_checks = 0;
static int g_failures = 0;

#define CHECK(cond)                                                        \
    do {                                                                   \
        g_checks++;                                                        \
        if (!(cond)) {                                                     \
            g_failures++;                                                  \
            printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);       \
        }                                                                  \
    } while (0)

/* feed a whole buffer, return the last non-NONE result (or NONE) */
static pkt_result_t feed_all(pkt_parser_t *p, const uint8_t *buf, size_t n)
{
    pkt_result_t last = PKT_NONE;
    for (size_t i = 0; i < n; i++) {
        pkt_result_t r = pkt_parser_feed(p, buf[i]);
        if (r != PKT_NONE) {
            last = r;
        }
    }
    return last;
}

static void test_ring_buffer_basic(void)
{
    puts("ring buffer: basic push/pop");
    uint8_t store[4];
    ring_buffer_t rb;
    uint8_t b;
    CHECK(rb_init(&rb, store, sizeof store));
    CHECK(rb_is_empty(&rb));
    CHECK(rb_capacity(&rb) == 3u);
    CHECK(!rb_pop(&rb, &b));
    CHECK(rb_push(&rb, 1));
    CHECK(rb_push(&rb, 2));
    CHECK(rb_count(&rb) == 2u);
    CHECK(rb_pop(&rb, &b) && b == 1);
    CHECK(rb_pop(&rb, &b) && b == 2);
    CHECK(rb_is_empty(&rb));
}

static void test_ring_buffer_full_and_overflow(void)
{
    puts("ring buffer: full / overflow rejected");
    uint8_t store[4];
    ring_buffer_t rb;
    uint8_t b;
    rb_init(&rb, store, sizeof store);
    CHECK(rb_push(&rb, 10));
    CHECK(rb_push(&rb, 20));
    CHECK(rb_push(&rb, 30));
    CHECK(rb_is_full(&rb));
    CHECK(!rb_push(&rb, 40));          /* must not overwrite */
    CHECK(rb_pop(&rb, &b) && b == 10); /* oldest data intact */
    CHECK(rb_push(&rb, 40));           /* space freed        */
}

static void test_ring_buffer_wraparound(void)
{
    puts("ring buffer: wraparound");
    uint8_t store[4];
    ring_buffer_t rb;
    uint8_t b;
    rb_init(&rb, store, sizeof store);
    for (uint8_t round = 0; round < 10; round++) {
        CHECK(rb_push(&rb, (uint8_t)(round * 2)));
        CHECK(rb_push(&rb, (uint8_t)(round * 2 + 1)));
        CHECK(rb_pop(&rb, &b) && b == (uint8_t)(round * 2));
        CHECK(rb_pop(&rb, &b) && b == (uint8_t)(round * 2 + 1));
    }
    CHECK(rb_is_empty(&rb));
}

static void test_ring_buffer_bad_init(void)
{
    puts("ring buffer: bad init arguments");
    uint8_t store[1];
    ring_buffer_t rb;
    CHECK(!rb_init(&rb, store, 1));
    CHECK(!rb_init(&rb, NULL, 8));
    CHECK(!rb_init(NULL, store, 8));
}

static void test_crc_known_vector(void)
{
    puts("crc16: standard check value");
    const uint8_t msg[] = "123456789";
    CHECK(crc16_ccitt(msg, 9) == 0x29B1u);
    CHECK(crc16_ccitt(msg, 0) == 0xFFFFu);
}

static void test_packet_roundtrip(void)
{
    puts("packet: encode then parse");
    const uint8_t payload[] = {0x01, 0x02, 0x03, 0xAA, 0xFF};
    uint8_t frame[PKT_MAX_PAYLOAD + PKT_OVERHEAD];
    size_t n = pkt_encode(payload, sizeof payload, frame, sizeof frame);
    CHECK(n == sizeof payload + PKT_OVERHEAD);

    pkt_parser_t p;
    pkt_parser_init(&p);
    CHECK(feed_all(&p, frame, n) == PKT_COMPLETE);
    CHECK(p.len == sizeof payload);
    CHECK(memcmp(p.payload, payload, sizeof payload) == 0);
}

static void test_packet_bad_crc(void)
{
    puts("packet: corrupted payload -> CRC error, then recovers");
    const uint8_t payload[] = {0x10, 0x20, 0x30};
    uint8_t good[16], bad[16];
    size_t n = pkt_encode(payload, sizeof payload, good, sizeof good);
    memcpy(bad, good, n);
    bad[3] ^= 0x01; /* flip one payload bit */

    pkt_parser_t p;
    pkt_parser_init(&p);
    CHECK(feed_all(&p, bad, n) == PKT_ERR_CRC);
    CHECK(feed_all(&p, good, n) == PKT_COMPLETE); /* parser resynced */
    CHECK(memcmp(p.payload, payload, sizeof payload) == 0);
}

static void test_packet_bad_length(void)
{
    puts("packet: LEN = 0 and LEN > max");
    pkt_parser_t p;
    pkt_parser_init(&p);
    CHECK(pkt_parser_feed(&p, PKT_START_BYTE) == PKT_NONE);
    CHECK(pkt_parser_feed(&p, 0) == PKT_ERR_LENGTH);
    CHECK(pkt_parser_feed(&p, PKT_START_BYTE) == PKT_NONE);
    CHECK(pkt_parser_feed(&p, PKT_MAX_PAYLOAD + 1) == PKT_ERR_LENGTH);
}

static void test_packet_noise_before_frame(void)
{
    puts("packet: garbage before START is ignored");
    const uint8_t noise[] = {0x00, 0x13, 0x77, 0xFE};
    const uint8_t payload[] = {0x42};
    uint8_t frame[8];
    size_t n = pkt_encode(payload, 1, frame, sizeof frame);

    pkt_parser_t p;
    pkt_parser_init(&p);
    CHECK(feed_all(&p, noise, sizeof noise) == PKT_NONE);
    CHECK(feed_all(&p, frame, n) == PKT_COMPLETE);
    CHECK(p.payload[0] == 0x42);
}

static void test_packet_back_to_back(void)
{
    puts("packet: two frames back to back");
    const uint8_t a[] = {1, 2, 3};
    const uint8_t b[] = {9, 8};
    uint8_t fa[16], fb[16];
    size_t na = pkt_encode(a, sizeof a, fa, sizeof fa);
    size_t nb = pkt_encode(b, sizeof b, fb, sizeof fb);

    pkt_parser_t p;
    pkt_parser_init(&p);
    CHECK(feed_all(&p, fa, na) == PKT_COMPLETE);
    CHECK(p.len == 3);
    CHECK(feed_all(&p, fb, nb) == PKT_COMPLETE);
    CHECK(p.len == 2 && p.payload[0] == 9);
}

static void test_packet_encode_rejects_bad_args(void)
{
    puts("packet: encode rejects bad arguments");
    uint8_t payload[PKT_MAX_PAYLOAD + 1] = {0};
    uint8_t out[PKT_MAX_PAYLOAD + PKT_OVERHEAD];
    CHECK(pkt_encode(payload, 0, out, sizeof out) == 0);
    CHECK(pkt_encode(payload, PKT_MAX_PAYLOAD + 1, out, sizeof out) == 0);
    CHECK(pkt_encode(payload, 8, out, 8) == 0); /* output too small */
    CHECK(pkt_encode(payload, PKT_MAX_PAYLOAD, out, sizeof out) ==
          PKT_MAX_PAYLOAD + PKT_OVERHEAD);
}

/* Simulates UART RX: an "ISR" pushes bytes (including a corrupted frame and
 * noise) into the ring buffer; the "main loop" drains it into the parser. */
static void test_uart_style_pipeline(void)
{
    puts("integration: ring buffer -> parser (UART RX style)");
    uint8_t store[32];
    ring_buffer_t rb;
    rb_init(&rb, store, sizeof store);

    const uint8_t p1[] = {0xDE, 0xAD};
    const uint8_t p2[] = {0xBE, 0xEF, 0x01};
    uint8_t f1[16], f2[16], stream[40];
    size_t n1 = pkt_encode(p1, sizeof p1, f1, sizeof f1);
    size_t n2 = pkt_encode(p2, sizeof p2, f2, sizeof f2);
    f1[2] ^= 0x80; /* corrupt first frame */

    size_t total = 0;
    stream[total++] = 0x00; /* noise */
    memcpy(stream + total, f1, n1); total += n1;
    memcpy(stream + total, f2, n2); total += n2;

    pkt_parser_t p;
    pkt_parser_init(&p);
    int ok = 0, crc_err = 0;
    size_t sent = 0;
    while (sent < total || !rb_is_empty(&rb)) {
        /* "ISR": push a few bytes if there is room */
        for (int k = 0; k < 5 && sent < total; k++) {
            if (!rb_push(&rb, stream[sent])) break;
            sent++;
        }
        /* "main loop": drain */
        uint8_t b;
        while (rb_pop(&rb, &b)) {
            pkt_result_t r = pkt_parser_feed(&p, b);
            if (r == PKT_COMPLETE) ok++;
            if (r == PKT_ERR_CRC) crc_err++;
        }
    }
    CHECK(crc_err == 1);
    CHECK(ok == 1);
    CHECK(p.len == 3 && p.payload[0] == 0xBE);
}

int main(void)
{
    test_ring_buffer_basic();
    test_ring_buffer_full_and_overflow();
    test_ring_buffer_wraparound();
    test_ring_buffer_bad_init();
    test_crc_known_vector();
    test_packet_roundtrip();
    test_packet_bad_crc();
    test_packet_bad_length();
    test_packet_noise_before_frame();
    test_packet_back_to_back();
    test_packet_encode_rejects_bad_args();
    test_uart_style_pipeline();

    printf("\n%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
