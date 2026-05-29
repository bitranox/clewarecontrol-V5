/*
 * Unit tests for cleware_decode() — no hardware or hidapi needed.
 * Build & run:  cc -O2 -Wall -Wextra tests/test_decode.c -o test_decode && ./test_decode
 */
#include <math.h>
#include <stdio.h>

#include "../cleware_decode.h"

static int failures = 0;

static void check_ok(const char *name, const unsigned char *f, double expect) {
    double t = 0.0;
    int rc = cleware_decode(f, &t);
    if (rc != 0) {
        printf("FAIL %-28s expected ok, got rc=%d\n", name, rc);
        failures++;
    } else if (fabs(t - expect) > 1e-9) {
        printf("FAIL %-28s expected %.4f, got %.4f\n", name, expect, t);
        failures++;
    } else {
        printf("ok   %-28s %.4f C\n", name, t);
    }
}

static void check_invalid(const char *name, const unsigned char *f) {
    double t = 0.0;
    int rc = cleware_decode(f, &t);
    if (rc == 0) {
        printf("FAIL %-28s expected rejection, got %.4f\n", name, t);
        failures++;
    } else {
        printf("ok   %-28s rejected (valid bit clear)\n", name);
    }
}

int main(void) {
    /* Real frame captured from a firmware-v5 sensor: 26.3125 C. */
    check_ok("real_room_temp", (unsigned char[]){0xae, 0x51, 0x8d, 0x2b, 0x00, 0x00}, 26.3125);

    /* Same data bytes, valid bit (byte0 bit7) cleared -> must be rejected. */
    check_invalid("valid_bit_clear", (unsigned char[]){0x2e, 0x51, 0x8d, 0x2b, 0x00, 0x00});

    /* Zero temperature. */
    check_ok("zero", (unsigned char[]){0x80, 0x00, 0x00, 0x00, 0x00, 0x00}, 0.0);

    /* One LSB = 0.0625 C: byte3 = 0x08 -> (0x08>>3)=1 -> 1 * 0.0625. */
    check_ok("one_lsb", (unsigned char[]){0x80, 0x00, 0x00, 0x08, 0x00, 0x00}, 0.0625);

    /* Negative: -20 C = -320 * 0.0625. 12-bit two's complement 3776=0xEC0,
     * (byte2&0x7f)=0x76, status bit set -> byte2=0xF6, byte3=0. */
    check_ok("minus_20", (unsigned char[]){0x80, 0x00, 0xf6, 0x00, 0x00, 0x00}, -20.0);

    /* Status bit on byte2 must NOT be treated as a sign bit (the v5 bug). */
    check_ok("status_bit_not_sign", (unsigned char[]){0x80, 0x00, 0x8d, 0x2b, 0x00, 0x00}, 26.3125);

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nall decode tests passed\n");
    return 0;
}
