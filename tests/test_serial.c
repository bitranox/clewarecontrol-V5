/*
 * Unit tests for cleware_sanitize_serial() — no hardware or hidapi needed.
 * Build & run:  cc -O2 -Wall -Wextra tests/test_serial.c -o test_serial && ./test_serial
 */
#include <stdio.h>
#include <string.h>

#include "../cleware_serial.h"

static int failures = 0;

static void check(const char *name, const char *in, const char *expect) {
    char out[64];
    cleware_sanitize_serial(out, sizeof(out), in);
    if (strcmp(out, expect) != 0) {
        printf("FAIL %-22s in=%-30s expected \"%s\", got \"%s\"\n",
               name, in ? in : "(null)", expect, out);
        failures++;
    } else {
        printf("ok   %-22s -> \"%s\"\n", name, out);
    }
}

int main(void) {
    /* a real hex serial passes through unchanged */
    check("hex_passthrough", "0018CE0", "0018CE0");

    /* line-protocol injection attempt: spaces, '=', commas, newline all dropped */
    check("injection_stripped", "x temperature=9\ninjected,a=b value=1",
          "xtemperature9injectedabvalue1");

    /* assorted separators that would break line protocol */
    check("separators_dropped", "a b,c=d\te\nf", "abcdef");

    /* letters (upper+lower) and digits kept */
    check("alnum_kept", "AbC123xyz", "AbC123xyz");

    /* empty and NULL yield empty string */
    check("empty", "", "");
    check("null", NULL, "");

    /* only-illegal yields empty string */
    check("only_illegal", " ,=\n\t", "");

    /* truncation: 70 'A's into a 64-byte buffer -> 63 chars */
    {
        char in[71];
        memset(in, 'A', 70);
        in[70] = '\0';
        char expect[64];
        memset(expect, 'A', 63);
        expect[63] = '\0';
        check("truncation", in, expect);
    }

    /* tiny destination buffer is respected and NUL-terminated */
    {
        char out[4];
        cleware_sanitize_serial(out, sizeof(out), "12345");
        if (strcmp(out, "123") != 0) {
            printf("FAIL %-22s expected \"123\", got \"%s\"\n", "tiny_buffer", out);
            failures++;
        } else {
            printf("ok   %-22s -> \"%s\"\n", "tiny_buffer", out);
        }
    }

    if (failures) {
        printf("\n%d test(s) FAILED\n", failures);
        return 1;
    }
    printf("\nall serial tests passed\n");
    return 0;
}
