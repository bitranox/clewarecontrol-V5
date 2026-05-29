# Security Policy

## Supported versions

This is a small single-purpose tool. Security fixes are applied to the latest
release / `master`; please run a current version before reporting.

| Version | Supported |
|---------|-----------|
| `master` / latest `1.x` | ✅ |
| older | ❌ |

## Reporting a vulnerability

Please report security issues **privately** — do not open a public issue for an
unfixed vulnerability.

- Preferred: GitHub → the repository's **Security** tab → **Report a
  vulnerability** (private vulnerability reporting).
- Include what you found, how to reproduce it, and the impact you expect.

You can expect an acknowledgement within a few days. Once a fix is available it
will be released and the report credited (unless you prefer to stay anonymous).

## Security model

`cleware_temp` reads a Cleware USB-Temp sensor over USB-HID and prints the
temperature to stdout (InfluxDB line protocol or a plain number). It:

- makes **no network connections** and opens **no listening sockets**,
- runs **no shell** and spawns no child processes,
- writes only to **stdout/stderr** (no files), and
- takes a small, fixed set of command-line options.

### Trust boundaries

1. **The command line** — supplied by whoever runs the tool (in the Telegraf
   setup, a fixed command in the config). Arguments are only compared and
   printed, never used as a format string or to build paths.
2. **The USB device** — a physical attacker could attach a rogue device that
   advertises the Cleware vendor id (`0d50`) and return arbitrary HID data and a
   crafted serial string. This is the main untrusted input.

### How untrusted device data is handled

- The HID frame is read into a fixed buffer with a bounded length; only the
  temperature bytes are interpreted, as a 12-bit signed value. The temperature
  is emitted as a numeric field (`%.4f`), so it cannot carry injected text.
- The device **serial** is attacker-controllable and is emitted as an InfluxDB
  line-protocol tag, so it is restricted to `[0-9A-Za-z]`. This prevents
  line-protocol injection (no spaces, commas, `=`, or newlines can reach the
  output). Real Cleware serials are hex, so this is lossless in practice.
- String handling uses bounded copies with explicit termination; there is no
  dynamic-format parsing and no use of `strcpy`/`strcat`/`sprintf`/`gets`.

### Privileges

libusb needs access to the USB device node, which is root-only by default.
Two options are documented in [the examples](examples/):

- **sudo** (simple), or
- **udev rule** granting a group access so the reader runs **without root**
  (preferred for least privilege, e.g. for the `telegraf` service user).

### Non-security caveats

A device that only ever returns invalid frames makes the tool retry for up to a
few seconds before exiting non-zero; it does not hang indefinitely. Run it under
a supervisor with a timeout (Telegraf's `exec` `timeout` already does this).

## Scope

This tool only reads temperature from the USB-Temp on firmware v5. It does not
control switches, watchdogs, LEDs, etc., and is not a fork of `clewarecontrol`.
