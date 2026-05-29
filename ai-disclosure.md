# AI disclosure

This documents, concretely, how AI was used to build this repository. For the
general position behind it, see [ai-stance.md](ai-stance.md).

## Summary

Effectively all of the code, tests, build setup, CI and documentation in this
repository were written by an AI coding agent — Anthropic's Claude, driven through
the Claude Code CLI — working interactively under the direction and review of the
maintainer ([bitranox](https://github.com/bitranox)). The work was done on
2026-05-29.

## What the AI did

- **Reverse-engineered the firmware-v5 decode.** Stock `clewarecontrol` reports a
  fixed ≈ −229 °C for this sensor. The agent built `clewarecontrol`, reproduced the
  bad reading, dumped the raw 6-byte HID frames from the device, and worked out that
  on firmware v5 byte 2 bit 7 is a status flag (not a sign bit) and the temperature
  is a 12-bit signed field — the decode now in
  [`cleware_decode.h`](cleware_decode.h).
- **Wrote the implementation** [`cleware_temp.c`](cleware_temp.c) (HID read loop,
  open-retry, median sampling, CLI, InfluxDB line-protocol output).
- **Wrote the tests, build and CI**: [`tests/test_decode.c`](tests/test_decode.c),
  the [`Makefile`](Makefile), and the GitHub Actions workflow.
- **Wrote the docs and examples**: this file, the README, and the Telegraf /
  sudoers / udev / systemd examples.

## How it was directed and verified

- The maintainer set the goal, made the design decisions (e.g. shipping a small
  purpose-built reader instead of patching `clewarecontrol`), and reviewed the
  output.
- The decode was **validated against real hardware**: the computed temperature was
  cross-checked against the host's on-board thermistors and matched (≈ 26 °C), and
  the reader has run live feeding a Telegraf → InfluxDB pipeline.
- The code builds warning-clean under `-Wall -Wextra -Werror` (gcc and clang), and
  the decode is covered by unit tests with known input frames.

## What this means for you

Treat this as you would any other code: the decode is documented and testable, it
has been checked against a physical firmware-v5 device, and there is a maintainer
who will answer for it. Issues and pull requests are welcome. Independent
verification against your own sensor is encouraged — `make check` runs without any
hardware.
