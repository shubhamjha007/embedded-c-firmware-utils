# Embedded C Firmware Utilities

Small, dependency-free C99 building blocks of the kind used in firmware:

| Module | What it does |
|---|---|
| `ring_buffer` | Fixed-size circular byte buffer. Caller-supplied storage, no `malloc`. Rejects pushes when full instead of overwriting. Safe for one producer (e.g. UART ISR) and one consumer (main loop). |
| `crc16` | CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF). Bitwise implementation, no lookup table, plus an incremental `crc16_update`. |
| `packet` | Byte-by-byte framed packet parser (state machine) and encoder. Detects bad length and bad CRC, ignores line noise, and resynchronises after errors. |

## Frame format

```
[0xAA][LEN][PAYLOAD x LEN][CRC_HI][CRC_LO]
```
CRC covers `LEN` and `PAYLOAD`. `LEN` is 1..64.

## Build and test

```
make test    # builds with -Wall -Wextra -Wpedantic -Werror and runs the tests
make asan    # same tests under AddressSanitizer + UBSan
```

Tests cover: empty/full/wraparound/overflow on the ring buffer, the standard
CRC check value (`"123456789"` -> `0x29B1`), encode/parse round trip, corrupted
payload, invalid length, noise before a frame, back-to-back frames, and a
UART-style pipeline (ring buffer feeding the parser).

## Design notes

- One ring-buffer slot is left empty so "full" and "empty" are distinguishable
  without a separate counter. This keeps `head` writable only by the producer
  and `tail` only by the consumer.
- The parser consumes one byte per call, so it can be driven directly from a
  UART RX interrupt or by draining a ring buffer.
- On an error the parser returns to `WAIT_START` and scans for the next
  `0xAA`. A payload byte equal to `0xAA` is not a problem inside a frame
  (length-based framing), but after an error it can cause a false start; the
  CRC then rejects it. Byte stuffing would remove that ambiguity.

## Possible extensions

- Table-driven CRC and a speed comparison
- Byte stuffing / escape sequences
- A timeout to drop half-received frames
- Run on a real or simulated microcontroller (see the Wokwi sensor project)
