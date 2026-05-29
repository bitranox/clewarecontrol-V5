/*
 * cleware_decode.h — pure decode of a Cleware USB-Temp (firmware v5) HID frame.
 *
 * Kept header-only and free of any I/O / hidapi dependency so it can be unit
 * tested in isolation (see tests/test_decode.c). See README.md for the frame
 * layout and the rationale behind the v5 12-bit signed decode.
 *
 * License: MIT.
 */
#ifndef CLEWARE_DECODE_H
#define CLEWARE_DECODE_H

/*
 * Decode a 6-byte Cleware USB-Temp v5 input report into degrees Celsius.
 *
 *   byte 0 : bit7 = valid flag, bits6..0 = time high
 *   byte 1 : time low
 *   byte 2 : bit7 = status flag, bits6..0 = temperature high bits
 *   byte 3 : bits7..3 = temperature low bits (bits2..0 unused)
 *
 * Returns 0 and writes *out on success, or -1 if the frame's valid bit is
 * clear (in which case *out is left untouched).
 */
static inline int cleware_decode(const unsigned char *buf, double *out) {
    if (!(buf[0] & 0x80))                            /* valid bit */
        return -1;
    int v = ((buf[2] & 0x7f) << 5) | (buf[3] >> 3);  /* 12-bit value */
    if (v & 0x800)                                   /* 12-bit two's complement */
        v -= 0x1000;
    *out = v * 0.0625;
    return 0;
}

#endif /* CLEWARE_DECODE_H */
