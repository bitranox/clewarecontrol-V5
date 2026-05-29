# clewarecontrol-V5 — project instructions

Single-purpose Linux tool: read temperature from a Cleware USB-Temp sensor on
**firmware version 5** and print it (InfluxDB line protocol or a plain number).
See [README.md](README.md) for usage and the v5 decode rationale.

## Build / test / lint

```bash
make                 # build ./cleware_temp (needs hidapi-libusb)
make check           # hardware-free unit tests (decode + serial sanitizer)
make install         # -> $(PREFIX)/bin/cleware_temp   (PREFIX defaults to /usr/local)
shellcheck install.sh examples/systemd/*.sh

# build with warnings-as-errors (what CI does):
make EXTRA_CFLAGS=-Werror
make check EXTRA_CFLAGS=-Werror
```

CI (`.github/workflows/ci.yml`) builds with **gcc and clang** under `-Werror`,
runs `make check`, checks the CLI and the no-device exit code, and verifies
`make install` / `uninstall`.

## Layout

- `cleware_decode.h` — pure 12-bit-signed frame decode (header-only, unit-tested).
- `cleware_serial.h` — pure serial sanitizer that blocks line-protocol injection
  (header-only, unit-tested).
- `cleware_temp.c` — the I/O: arg parsing, `find_device()`, open-with-retry,
  `read_temperature()` (median of valid frames), output.
- `tests/` — `test_decode.c`, `test_serial.c` (no hardware/hidapi needed).
- `examples/` — Telegraf, sudoers, udev, and a no-Telegraf InfluxDB push.

Keep pure logic in the headers (so it stays unit-testable) and I/O in
`cleware_temp.c`. Exit codes are part of the contract: `0` ok, `2` hid_init,
`3` no device, `4` open failed, `5` no valid read.

# Code Quality

Deliberately accepted items — do not flag in future reviews:

- **Single purpose (temperature only)**: by design the tool reads temperature
  from the USB-Temp on firmware v5 and nothing else (no switch/watchdog/LED
  control). It is not a fork or replacement for `clewarecontrol`.
- **Default `make` is not `-Werror`**: strictness is CI-only (`EXTRA_CFLAGS=-Werror`)
  so end-user builds on exotic toolchains/hidapi versions don't fail on a stray
  warning. Intentional.
- **Linux / hidapi-libusb only**: uses the libusb backend and Linux-specific
  device access (sudo / udev). Windows/macOS are out of scope.
- **Tracks `master` unless pinned**: deployments (e.g. proxmox-fw) build from the
  upstream repo; pin via a tag/`CLEWARE_REF` when reproducibility is required.
