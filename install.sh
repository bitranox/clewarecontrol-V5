#!/usr/bin/env bash
set -euo pipefail

# install.sh — build and install cleware_temp on a Debian/Ubuntu host, and
# (optionally) set up sudo access for the telegraf user.
#
# Usage:
#   sudo ./install.sh                # build + install /usr/local/bin/cleware_temp
#   sudo ./install.sh --telegraf     # ...and install the telegraf sudoers entry
#
# For other distros just install the hidapi (libusb backend) dev package and
# run `make && sudo make install` yourself.

PREFIX="${PREFIX:-/usr/local}"
WITH_TELEGRAF=0
[[ "${1:-}" == "--telegraf" ]] && WITH_TELEGRAF=1

[[ $(id -u) -ne 0 ]] && { echo "Run as root (sudo)."; exit 1; }
cd "$(dirname "$0")"

if command -v apt-get >/dev/null 2>&1; then
  export DEBIAN_FRONTEND=noninteractive
  apt-get install -y --no-install-recommends \
    gcc make pkg-config libhidapi-dev libhidapi-libusb0
else
  echo "Non-apt system: ensure a C compiler, make, pkg-config and the hidapi"
  echo "libusb-backend dev package are installed, then re-run."
fi

make clean
make
make install PREFIX="$PREFIX"

if [[ $WITH_TELEGRAF -eq 1 ]]; then
  install -m 0440 examples/sudoers.d/telegraf-cleware /etc/sudoers.d/telegraf-cleware
  visudo -cf /etc/sudoers.d/telegraf-cleware
  echo "Installed /etc/sudoers.d/telegraf-cleware"
  echo "Now add the telegraf input — see examples/telegraf/cleware_temp.conf"
fi

echo "--- smoke test ---"
"$PREFIX/bin/cleware_temp" --plain || echo "(no reading — is the sensor plugged in?)"
echo "done."
