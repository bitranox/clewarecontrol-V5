# AI disclosure

An honest account of the role AI played in this repository. For the general
position behind it, see [ai-stance.md](ai-stance.md).

## Summary

The maintainer ([bitranox](https://github.com/bitranox)) designed, directed, and
verified this project. An AI coding assistant (Anthropic's Claude, via the Claude
Code CLI) was a tool used to speed up the routine work, always under the
maintainer's direction and review. Every decision, and the final result, are the
maintainer's. The work was done on 2026-05-29.

## What the maintainer drove

- **Set the goal and the approach.** Get a correct reading off the firmware-v5
  USB-Temp, and decide how to get there: confirm the stock `clewarecontrol` bug,
  capture the raw HID frames, and work out the real encoding from them.
- **Made the design decisions.** Ship a small, single-purpose reader that emits
  InfluxDB line protocol instead of patching the abandoned `clewarecontrol`; settle
  how device access should work (sudo or udev); decide what the tool should and
  shouldn't do.
- **Verified it against real hardware.** The decoded value was cross-checked
  against the host's on-board thermistors (about 26 °C), and the reader has run
  live feeding a Telegraf and InfluxDB pipeline. This is the part that actually
  matters, and what counted as "correct" was the maintainer's call.
- **Drove the security review.** The maintainer asked whether the code was
  exploitable. That prompted a deliberate pass over the attack surface: buffer and
  bounds handling, argument parsing, format strings, and the data coming back from
  the USB device. It turned up one real problem. A rogue device could put arbitrary
  text in its USB serial string, which was emitted unescaped as an InfluxDB
  line-protocol tag, an injection vector. It was fixed at the maintainer's
  direction (the serial is now restricted to `[0-9A-Za-z]`), and the trust model is
  written up in [SECURITY.md](SECURITY.md). The call on acceptable residual risk,
  such as running via sudo versus a udev rule, was also the maintainer's.
- **Reviewed every change**, and maintains and answers for the result.

## Where AI helped

Under that direction, the assistant did the legwork: drafting `cleware_temp.c` and
the [`cleware_decode.h`](cleware_decode.h) decode, the unit tests, the `Makefile`,
the CI workflow, and the docs and examples. It also handled the mechanical parts of
the investigation, like building tooling, dumping frames, and trying decodings. The
maintainer checked and accepted all of it rather than taking it on trust.

## What this means for you

Judge it the way you'd judge any other code. The decode is documented and testable,
it has been checked against a physical firmware-v5 device, and there is a maintainer
behind it. Issues and pull requests are welcome, and it's worth verifying against
your own sensor. `make check` runs without any hardware.
