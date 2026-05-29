/*
 * cleware_temp — read a Cleware USB-Temp sensor (firmware version 5) and print
 * the temperature, either as InfluxDB line protocol (default, for Telegraf's
 * exec input) or as a plain number.
 *
 * Why this exists
 * ---------------
 * The Cleware USB-Temp (USB 0d50:0010, HID device type 0x10) ships in several
 * firmware revisions. On firmware *version 5* the venerable `clewarecontrol`
 * tool mis-decodes the temperature and reports a fixed ~ -229 C. Its decode
 * treats HID byte 2 as eight data bits with bit 12 acting as a sign bit. On v5
 * that is wrong: byte 2 bit 7 is a *status flag* and the temperature is a
 * 12-bit signed field. The Cleware manual confirms the resolution is 0.0625 C.
 *
 * Read protocol
 * -------------
 * Open the HID device, send the 3-byte feature report {0x00, seq, 0x81}, then
 * read a 6-byte input report. Example frame "ae 51 8d 2b 00 00":
 *
 *   byte 0 : bit7 = valid flag, bits6..0 = time high
 *   byte 1 : time low
 *   byte 2 : bit7 = status flag, bits6..0 = temperature high bits
 *   byte 3 : bits7..3 = temperature low bits (bits2..0 unused)
 *   byte 4,5 : 0
 *
 *   v = ((byte2 & 0x7f) << 5) | (byte3 >> 3);   // 12-bit
 *   if (v & 0x800) v -= 0x1000;                  // 12-bit two's complement
 *   tempC = v * 0.0625;
 *
 * Example: 8d 2b -> ((0x8d & 0x7f)<<5) | (0x2b>>3) = (13<<5)|5 = 421
 *          -> 421 * 0.0625 = 26.3125 C
 *
 * Build:
 *   gcc cleware_temp.c -O2 -Wall -o cleware_temp \
 *       $(pkg-config --cflags --libs hidapi-libusb)
 *
 * License: MIT. See LICENSE.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <hidapi/hidapi.h>

#include "cleware_decode.h"

#ifndef CLEWARE_TEMP_VERSION
#define CLEWARE_TEMP_VERSION "1.0.0"
#endif

#define CLEWARE_VID   0x0d50
#define CLEWARE_TEMP  0x0010   /* USB-Temp product id */
#define SAMPLES       9        /* collect this many valid frames, take median */
#define MAX_TRIES     (SAMPLES * 3)
#define READ_TIMEOUT  1000     /* ms per hid_read */
#define OPEN_TRIES    6        /* retry hid_open_path: libusb can briefly race */
#define OPEN_DELAY_US 200000   /* with the kernel driver re-grabbing the device */
#define TEMP_MIN     -30.0     /* sanity window (sensor spec is -20..+80 C) */
#define TEMP_MAX      90.0

static int cmp_double(const void *a, const void *b) {
    double d = *(const double *)a - *(const double *)b;
    return (d > 0) - (d < 0);
}

static void usage(const char *argv0) {
    printf(
        "cleware_temp %s — read a Cleware USB-Temp (firmware v5) sensor\n"
        "\n"
        "Usage: %s [options]\n"
        "\n"
        "  -p, --plain          print just the temperature in C (e.g. 26.3125)\n"
        "                       (default: InfluxDB line protocol)\n"
        "  -s, --serial HEX     only use the device with this serial (e.g. 0018CE0)\n"
        "  -h, --help           show this help and exit\n"
        "  -V, --version        show version and exit\n"
        "\n"
        "Default output (for Telegraf [[inputs.exec]] with data_format=\"influx\"):\n"
        "  cleware_temp,sensor=usbtemp,serial=0018CE0 temperature=26.3125\n"
        "\n"
        "Exit codes: 0 ok, 2 hid_init, 3 no device, 4 open failed, 5 no valid read\n",
        CLEWARE_TEMP_VERSION, argv0);
}

int main(int argc, char **argv) {
    int plain = 0;
    const char *want_serial = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-p") || !strcmp(argv[i], "--plain")) {
            plain = 1;
        } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--serial")) {
            if (i + 1 >= argc) { fprintf(stderr, "missing serial after %s\n", argv[i]); return 1; }
            want_serial = argv[++i];
        } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) {
            usage(argv[0]); return 0;
        } else if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
            printf("cleware_temp %s\n", CLEWARE_TEMP_VERSION); return 0;
        } else {
            fprintf(stderr, "unknown argument: %s (try --help)\n", argv[i]);
            return 1;
        }
    }

    if (hid_init() != 0) {
        fprintf(stderr, "cleware_temp: hid_init failed\n");
        return 2;
    }

    /* locate the first matching Cleware USB-Temp device */
    struct hid_device_info *devs = hid_enumerate(CLEWARE_VID, 0x0), *cur;
    char path[512] = "";
    char serial[64] = "";
    for (cur = devs; cur; cur = cur->next) {
        if (cur->product_id != CLEWARE_TEMP || path[0])
            continue;
        char s[64] = "";
        if (cur->serial_number) {
            wcstombs(s, cur->serial_number, sizeof(s) - 1);
            s[sizeof(s) - 1] = '\0';
        }
        if (want_serial && strcmp(s, want_serial) != 0)
            continue;
        snprintf(path, sizeof(path), "%s", cur->path);
        /* The serial comes from the USB string descriptor, i.e. it is
         * attacker-controllable if a rogue device is attached, and it is emitted
         * as an InfluxDB line-protocol tag. Restrict it to [0-9A-Za-z] so it
         * cannot inject extra tags/fields or lines (spaces, commas, '=', '\n').
         * Real Cleware serials are hex, so this is lossless in practice. */
        size_t j = 0;
        for (size_t k = 0; s[k] && j < sizeof(serial) - 1; k++) {
            unsigned char c = (unsigned char)s[k];
            if ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'))
                serial[j++] = (char)c;
        }
        serial[j] = '\0';
    }
    hid_free_enumeration(devs);
    if (!path[0]) {
        if (want_serial)
            fprintf(stderr, "cleware_temp: no Cleware USB-Temp with serial %s found\n", want_serial);
        else
            fprintf(stderr, "cleware_temp: no Cleware USB-Temp (%04x:%04x) found\n",
                    CLEWARE_VID, CLEWARE_TEMP);
        hid_exit();
        return 3;
    }

    /* The libusb backend detaches the kernel driver on open; if the previous
     * run only just closed, the open can transiently fail with BUSY. Retry. */
    hid_device *h = NULL;
    for (int i = 0; i < OPEN_TRIES && !(h = hid_open_path(path)); i++)
        usleep(OPEN_DELAY_US);
    if (!h) {
        fprintf(stderr, "cleware_temp: hid_open_path(%s) failed\n", path);
        hid_exit();
        return 4;
    }

    double samples[SAMPLES];
    int n = 0;
    unsigned char seq = 0;
    for (int i = 0; i < MAX_TRIES && n < SAMPLES; i++) {
        unsigned char fr[3] = { 0x00, seq++, 0x81 };
        unsigned char buf[8] = { 0 };
        double t;
        if (hid_send_feature_report(h, fr, sizeof fr) < 0)
            continue;
        if (hid_read_timeout(h, buf, 6, READ_TIMEOUT) < 6)
            continue;
        if (cleware_decode(buf, &t) != 0)
            continue;
        if (t < TEMP_MIN || t > TEMP_MAX)
            continue;
        samples[n++] = t;
        usleep(50000);
    }

    hid_close(h);
    hid_exit();

    if (n == 0) {
        fprintf(stderr, "cleware_temp: no valid reading\n");
        return 5;
    }

    qsort(samples, n, sizeof(double), cmp_double);
    double temp = samples[n / 2];   /* median is robust to the odd bad frame */

    if (plain) {
        printf("%.4f\n", temp);
    } else if (serial[0]) {
        /* Telegraf appends the global `host` tag and the timestamp. */
        printf("cleware_temp,sensor=usbtemp,serial=%s temperature=%.4f\n", serial, temp);
    } else {
        printf("cleware_temp,sensor=usbtemp temperature=%.4f\n", temp);
    }
    return 0;
}
