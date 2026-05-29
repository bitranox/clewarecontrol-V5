# Security Policy

## Supported versions

This is a small single-purpose tool. Security fixes go onto the latest release
and `master`, so please run a current version before reporting.

| Version                 | Supported |
|-------------------------|-----------|
| `master` / latest `1.x` | Yes       |
| older                   | No        |

## Reporting a vulnerability

Please report security issues privately. Don't open a public issue for an unfixed
vulnerability.

The preferred channel is GitHub's private vulnerability reporting: open the
repository's Security tab and click "Report a vulnerability". Include what you
found, how to reproduce it, and the impact you expect.

You can expect an acknowledgement within a few days. Once a fix is available it
will be released and your report credited, unless you'd rather stay anonymous.

## Security model

`cleware_temp` reads a Cleware USB-Temp sensor over USB-HID and prints the
temperature to stdout (InfluxDB line protocol or a plain number). It makes no
network connections and opens no listening sockets, runs no shell and spawns no
child processes, writes only to stdout and stderr (no files), and takes a small,
fixed set of command-line options.

### Trust boundaries

There are two sources of input:

1. The command line, supplied by whoever runs the tool (in the Telegraf setup,
   that's a fixed command in the config). Arguments are only compared and printed,
   never used as a format string or to build paths.
2. The USB device. A physical attacker could attach a rogue device that advertises
   the Cleware vendor id (`0d50`) and returns arbitrary HID data and a crafted
   serial string. This is the main untrusted input.

### How untrusted device data is handled

The HID frame is read into a fixed buffer with a bounded length, and only the
temperature bytes are interpreted, as a 12-bit signed value. The temperature is
emitted as a numeric field (`%.4f`), so it can't carry injected text.

The device serial is attacker-controllable, and it is emitted as an InfluxDB
line-protocol tag, so it is restricted to `[0-9A-Za-z]`. That stops line-protocol
injection: no spaces, commas, `=`, or newlines can reach the output. Real Cleware
serials are hex, so the restriction is lossless in practice.

String handling uses bounded copies with explicit termination. There is no
dynamic-format parsing and no use of `strcpy`, `strcat`, `sprintf`, or `gets`.

### Privileges

libusb needs access to the USB device node, which is root-only by default. Two
options are documented in [the examples](examples/): sudo, which is the simple
route, or a udev rule that grants a group access so the reader runs without root.
The udev route is the least-privilege option, for example for the `telegraf`
service user.

### Non-security caveats

A device that only ever returns invalid frames makes the tool retry for a few
seconds before exiting non-zero; it does not hang indefinitely. Run it under a
supervisor with a timeout (Telegraf's `exec` `timeout` already does this).

## Scope

This tool only reads temperature from the USB-Temp on firmware v5. It does not
control switches, watchdogs, or LEDs, and it is not a fork of `clewarecontrol`.
