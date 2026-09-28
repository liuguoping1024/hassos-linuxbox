#!/bin/sh
#
# Zigbee2MQTT memory monitor — logs RSS/swap usage hourly to /mnt/data
#

LOG_FILE="/mnt/data/z2m_mem.log"
MAX_LINES=8760  # ~1 year at hourly interval

PID=$(pidof node 2>/dev/null | awk '{print $1}')
TIMESTAMP=$(date '+%Y-%m-%d %H:%M:%S')

if [ -z "$PID" ]; then
    echo "$TIMESTAMP  PID=N/A  [process not found]" >> "$LOG_FILE"
    exit 0
fi

if [ ! -f "/proc/$PID/status" ]; then
    echo "$TIMESTAMP  PID=$PID  [/proc/$PID/status not accessible]" >> "$LOG_FILE"
    exit 0
fi

VSZ=$(awk '/VmSize/{print $2}' /proc/$PID/status)
RSS=$(awk '/VmRSS/{print $2}' /proc/$PID/status)
SWAP=$(awk '/VmSwap/{print $2}' /proc/$PID/status)
ANON=$(awk '/RssAnon/{print $2}' /proc/$PID/status)
FILE=$(awk '/RssFile/{print $2}' /proc/$PID/status)

fmt() { awk "BEGIN{printf \"%.1f\",$1/1024}"; }

echo "$TIMESTAMP  PID=$PID  VSZ=$(fmt $VSZ)MB  RSS=$(fmt $RSS)MB  ANON=$(fmt $ANON)MB  FILE=$(fmt $FILE)MB  SWAP=$(fmt $SWAP)MB" >> "$LOG_FILE"

# Rotate: keep last MAX_LINES entries
if [ "$(wc -l < "$LOG_FILE")" -gt "$MAX_LINES" ]; then
    tail -n "$MAX_LINES" "$LOG_FILE" > "$LOG_FILE.tmp" && mv "$LOG_FILE.tmp" "$LOG_FILE"
fi
