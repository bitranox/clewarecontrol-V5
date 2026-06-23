#!/usr/bin/env bash
# Example: push a reading straight to InfluxDB v2 without Telegraf, e.g. from a
# systemd timer or cron. Adjust the variables, then run on a schedule.
set -euo pipefail

INFLUX_URL="${INFLUX_URL:-http://influxdb.example:8086}"
INFLUX_ORG="${INFLUX_ORG:-your-org}"
INFLUX_BUCKET="${INFLUX_BUCKET:-sensors}"
INFLUX_TOKEN="${INFLUX_TOKEN:?set INFLUX_TOKEN}"
HOSTTAG="${HOSTTAG:-$(hostname -s)}"

line="$(/usr/local/bin/cleware_temp)"            # cleware_temp,sensor=usbtemp,... temperature=NN
line="${line/cleware_temp,/cleware_temp,host=${HOSTTAG},}"

curl -sS --max-time 10 \
  -X POST "${INFLUX_URL}/api/v2/write?org=${INFLUX_ORG}&bucket=${INFLUX_BUCKET}&precision=s" \
  -H "Authorization: Token ${INFLUX_TOKEN}" \
  -H "Content-Type: text/plain; charset=utf-8" \
  --data-binary "${line}"
