# clewarecontrol-V5 (`cleware_temp`)

[![CI](https://github.com/bitranox/clewarecontrol-V5/actions/workflows/ci.yml/badge.svg)](https://github.com/bitranox/clewarecontrol-V5/actions/workflows/ci.yml)

A tiny, dependency-light Linux reader for the **Cleware USB-Temp** temperature
sensor with **firmware version 5**, where the classic
[`clewarecontrol`](https://github.com/xrobau/clewarecontrol) tool reports a
fixed bogus value (≈ **−229 °C**).

`cleware_temp` reads the sensor over USB-HID and prints the temperature either
as **InfluxDB line protocol** (default — drop-in for Telegraf's `exec` input) or
as a **plain number**.

```console
$ cleware_temp
cleware_temp,sensor=usbtemp,serial=0018CE0 temperature=26.3125
$ cleware_temp --plain
26.3125
```

## Platform

**Linux only.** It talks to the sensor through the hidapi **libusb** backend and
the device-access setup (sudo / udev) is Linux-specific. It should build and run
on any modern Linux distribution with hidapi installed; it has **not** been tested
on Windows or macOS.

Developed and tested on:

| Component | Tested with                                                     |
|-----------|-----------------------------------------------------------------|
| OS        | Debian 13 (trixie), x86_64                                      |
| Kernel    | Linux 7.0.2                                                     |
| hidapi    | 0.14.0 (`hidapi-libusb`)                                        |
| Compilers | gcc 14.2.0 (build host); CI: gcc 13.2.0 + clang 18.0, `-Werror` |

## The problem with stock clewarecontrol on firmware v5

The Cleware USB-Temp (USB `0d50:0010`, HID device type `0x10`) exists in several
firmware revisions. `clewarecontrol` decodes the temperature like this:

```c
value = (buf[2] << 5) + (buf[3] >> 3);   // treats byte2 as 8 data bits
if (value & 0x1000) value -= 0x1000;     // bit 12 = sign
temp  = value * 0.0625;
```

On **firmware v5** that is wrong. Byte 2 bit 7 is a **status flag**, not data,
and the temperature is a **12-bit signed** field. With byte 2 = `0x8d`, bit 7 is
set, so the old code mistakes the reading for a large negative number and prints
a constant ≈ −229 °C. The correct decode masks that bit and uses 12-bit
two's-complement (the 0.0625 °C step is documented in the Cleware manual):

```c
v = ((buf[2] & 0x7f) << 5) | (buf[3] >> 3);   // 12-bit value
if (v & 0x800) v -= 0x1000;                    // 12-bit signed
tempC = v * 0.0625;
```

Example frame `ae 51 8d 2b 00 00` → `((0x8d & 0x7f)<<5) | (0x2b>>3)` =
`(13<<5)|5` = `421` → `421 × 0.0625` = **26.3125 °C** — matching a reference
thermometer.

### HID frame layout (6-byte input report)

| byte | meaning                                                |
|------|--------------------------------------------------------|
| 0    | bit 7 = valid flag, bits 6..0 = time high              |
| 1    | time low                                               |
| 2    | bit 7 = status flag, bits 6..0 = temperature high bits |
| 3    | bits 7..3 = temperature low bits (bits 2..0 unused)    |
| 4, 5 | 0                                                      |

Read sequence: send the 3-byte HID **feature report** `{0x00, seq, 0x81}`, then
read the 6-byte input report. `cleware_temp` takes the **median of several valid
frames** and applies a −30…+90 °C sanity window.

## Build

Requires a C compiler and **hidapi** (libusb backend).

Debian / Ubuntu:

```bash
sudo apt-get install -y gcc make pkg-config libhidapi-dev libhidapi-libusb0
make
```

This produces the `cleware_temp` binary. Install it system-wide with:

```bash
sudo make install            # -> /usr/local/bin/cleware_temp
```

Or use the convenience installer (also sets up Telegraf sudo access with
`--telegraf`):

```bash
sudo ./install.sh --telegraf
```

## Testing

The decode logic ([`cleware_decode.h`](cleware_decode.h)) and the serial
sanitizer ([`cleware_serial.h`](cleware_serial.h)) are covered by hardware-free
unit tests — known frames → expected °C (including the v5 status-bit case that
trips up `clewarecontrol`), and serial inputs → injection-safe output:

```bash
make check
```

CI (GitHub Actions, see [`.github/workflows/ci.yml`](.github/workflows/ci.yml))
builds with `gcc` and `clang` using `-Werror`, runs the unit tests, checks the
CLI and the no-device exit code, and verifies `make install` / `uninstall`.

## Usage

```text
cleware_temp [options]

  -p, --plain          print just the temperature in C (e.g. 26.3125)
                       (default: InfluxDB line protocol)
  -s, --serial HEX     only use the device with this serial (e.g. 0018CE0)
  -h, --help           show help
  -V, --version        show version

Exit codes: 0 ok, 2 hid_init, 3 no device, 4 open failed, 5 no valid read
```

Find your sensor's serial:

```bash
lsusb -d 0d50:0010 -v 2>/dev/null | grep iSerial
```

## Device access (run without root)

`cleware_temp` talks to the device through libusb, which needs access to the USB
device node (root by default). For a service such as Telegraf, pick one:

- **sudo** (simplest): install
  [`examples/sudoers.d/telegraf-cleware`](examples/sudoers.d/telegraf-cleware)
  to `/etc/sudoers.d/telegraf-cleware` (mode `0440`) and call
  `sudo /usr/local/bin/cleware_temp`.
- **udev** (least privilege): install
  [`examples/udev/99-cleware-usbtemp.rules`](examples/udev/99-cleware-usbtemp.rules),
  reload udev, add the service user to the group, and call the binary directly.

## Telegraf → InfluxDB

Use [`examples/telegraf/cleware_temp.conf`](examples/telegraf/cleware_temp.conf)
(copy into `/etc/telegraf/telegraf.d/`):

```toml
[[inputs.exec]]
  commands = ["sudo /usr/local/bin/cleware_temp"]
  timeout = "5s"
  data_format = "influx"
```

The measurement is `cleware_temp`, field `temperature`, tags `sensor=usbtemp`
and `serial=…`; Telegraf adds the global `host` tag and the timestamp. If your
`influxdb_v2` output uses a `namepass` allow-list, add `"cleware_temp"` to it.

Verify the data arrived (Flux):

```flux
from(bucket: "sensors")
  |> range(start: -5m)
  |> filter(fn: (r) => r._measurement == "cleware_temp")
  |> last()
```

### Without Telegraf

[`examples/systemd/cleware-temp-influx.sh`](examples/systemd/cleware-temp-influx.sh)
shows how to POST a reading straight to the InfluxDB v2 write API from a cron job
or systemd timer.

## Troubleshooting

| Symptom                                 | Cause / fix                                                                                                 |
|-----------------------------------------|-------------------------------------------------------------------------------------------------------------|
| `no Cleware USB-Temp (0d50:0010) found` | sensor not plugged in, or you filtered by a serial that doesn't match (`lsusb -d 0d50:0010`)                |
| `hid_open_path(...) failed` (exit 4)    | permissions (see *Device access*), or a transient libusb/kernel race — the tool already retries a few times |
| `no valid reading` (exit 5)             | device returned only frames with the valid bit clear; check the cable/USB port                              |
| reads ≈ −229 °C with `clewarecontrol`   | that's exactly the v5 bug this tool fixes — use `cleware_temp`                                              |

## Notes / scope

This tool deliberately does **one thing**: read temperature from the USB-Temp on
firmware v5. It is an independent reimplementation based on the HID protocol, not
a fork of `clewarecontrol`, and does not control switches, watchdogs, LEDs, etc.
The decode has been verified on a firmware-v5 device (type `0x10`); other Cleware
temperature variants (USB-Temp2 `0x11`, Temp5 `0x15`, USB-Humidity) use different
frame layouts and are out of scope.

## Security

See [SECURITY.md](SECURITY.md) for the security model, how untrusted USB-device
data is handled, and how to report a vulnerability.

## AI transparency

This project was built with AI assistance. See [ai-disclosure.md](ai-disclosure.md)
for exactly how, and [ai-stance.md](ai-stance.md) for the reasoning behind it.

## License

MIT — see [LICENSE](LICENSE).
