/*
 * cleware_serial.h — sanitize a Cleware device serial for safe emission.
 *
 * The serial comes from the USB string descriptor, i.e. it is
 * attacker-controllable if a rogue device is attached, and `cleware_temp` emits
 * it as an InfluxDB line-protocol tag. Restricting it to [0-9A-Za-z] means it
 * cannot inject extra tags/fields or whole lines (no spaces, commas, '=', or
 * newlines can reach the output). Real Cleware serials are hex, so this is
 * lossless in practice.
 *
 * Header-only and free of I/O so it can be unit tested in isolation
 * (see tests/test_serial.c).
 *
 * License: MIT.
 */
#ifndef CLEWARE_SERIAL_H
#define CLEWARE_SERIAL_H

#include <stddef.h>

/*
 * Copy the [0-9A-Za-z] characters of `src` into `dst`, dropping everything
 * else, writing at most `dstsz - 1` characters plus a NUL terminator. `dst` is
 * always NUL-terminated (provided dstsz > 0). A NULL `src` yields an empty
 * string.
 */
static inline void cleware_sanitize_serial(char *dst, size_t dstsz, const char *src) {
    size_t j = 0;
    if (dstsz == 0)
        return;
    if (src) {
        for (size_t k = 0; src[k] && j < dstsz - 1; k++) {
            unsigned char c = (unsigned char)src[k];
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                dst[j++] = (char)c;
        }
    }
    dst[j] = '\0';
}

#endif /* CLEWARE_SERIAL_H */
