# AI disclosure

This documents, honestly, the role AI played in this repository. For the general
position behind it, see [ai-stance.md](ai-stance.md).

## Summary

This project was designed, directed, and verified by the maintainer
([bitranox](https://github.com/bitranox)). An AI coding assistant (Anthropic's
Claude, via the Claude Code CLI) was used as a tool to speed up the routine work —
always under the maintainer's direction and review. Every decision, and the final
result, are the maintainer's. The work was done on 2026-05-29.

## What the maintainer drove

- **Set the goal and the approach** — get a correct reading off the firmware-v5
  USB-Temp, and the strategy for getting there: confirm the stock `clewarecontrol`
  bug, capture the raw HID frames, and work out the real encoding from them.
- **Made the design decisions** — e.g. shipping a small, single-purpose reader that
  emits InfluxDB line protocol rather than patching the abandoned `clewarecontrol`;
  how device access should work (sudo vs udev); what the tool should and shouldn't do.
- **Verified it against real hardware** — the decoded value was cross-checked against
  the host's on-board thermistors (≈ 26 °C) and the reader has run live feeding a
  Telegraf → InfluxDB pipeline. This is the part that actually matters, and it was
  the maintainer's call on what counted as "correct".
- **Drove the security review** — the maintainer specifically asked whether the code
  was exploitable, which prompted a deliberate pass over the attack surface: buffer
  and bounds handling, argument parsing, format strings, and the data coming back from
  the USB device. That review found that a rogue device could put arbitrary text in its
  USB serial string, which was emitted unescaped as an InfluxDB line-protocol tag — an
  injection vector. At the maintainer's direction it was fixed (the serial is now
  restricted to `[0-9A-Za-z]`), and the trust model is written up in
  [SECURITY.md](SECURITY.md). The remaining call on acceptable risk (e.g. running via
  `sudo` vs a udev rule) was the maintainer's.
- **Reviewed every change** and **maintains and answers for the result**.

## Where AI helped

Under that direction, the assistant did the legwork: drafting `cleware_temp.c` and
the [`cleware_decode.h`](cleware_decode.h) decode, the unit tests, `Makefile`, CI
workflow, and the docs/examples, plus the mechanical parts of the
investigation (building tooling, dumping frames, trying decodings). All of it was
checked and accepted by the maintainer rather than taken on trust.

## What this means for you

Judge it the way you'd judge any other code: the decode is documented and testable,
it has been checked against a physical firmware-v5 device, and there's a maintainer
behind it. Issues and pull requests are welcome, and independent verification against
your own sensor is encouraged — `make check` runs without any hardware.
