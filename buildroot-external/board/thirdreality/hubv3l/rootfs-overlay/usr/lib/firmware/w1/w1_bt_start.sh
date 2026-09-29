#!/bin/sh
# HubV3L Bluetooth bring-up (5.4 kernel, out-of-tree sdio_bt.ko from aml-bt).
#
# Mirrors the shared 6.6 script, but loads sdio_bt.ko by path: on this line it
# lives outside the kernel's own tree, so `modprobe sdio_bt` found nothing.

BT_FIRMWARE_DIR="/etc/bluetooth/aml"
BT_MODULE="/lib/modules/$(uname -r)/extra/amlogic/bt/sdio_bt.ko"
BT_TTY="/dev/ttyAML1"
HCIATTACH_LOG="/tmp/aml_hciattach.log"

log() { echo "w1_bt_start: $*"; }

if [ ! -L "$BT_FIRMWARE_DIR/w1_bt_fw_uart.bin" ] || [ ! -L "$BT_FIRMWARE_DIR/a2dp_mode_cfg.txt" ] || [ ! -L "$BT_FIRMWARE_DIR/aml_bt_rf.txt" ]; then
    mkdir -p "$BT_FIRMWARE_DIR"
    for f in a2dp_mode_cfg.txt aml_bt_rf.txt w1_bt_fw_uart.bin; do
        if [ -f "/lib/firmware/w1/$f" ]; then
            ln -sf "/lib/firmware/w1/$f" "$BT_FIRMWARE_DIR/"
        else
            log "WARNING: firmware /lib/firmware/w1/$f missing"
        fi
    done
fi
log "firmware: $(ls -l "$BT_FIRMWARE_DIR" 2>/dev/null | grep -c -- '->') links in $BT_FIRMWARE_DIR"

if [ -e /sys/class/rfkill/rfkill0/state ]; then
    log "rfkill0 ($(cat /sys/class/rfkill/rfkill0/name 2>/dev/null)) power cycle"
    echo 0 > /sys/class/rfkill/rfkill0/state
    sleep 0.5
    echo 1 > /sys/class/rfkill/rfkill0/state
else
    log "WARNING: /sys/class/rfkill/rfkill0 not present"
fi

if grep -q "^sdio_bt " /proc/modules; then
    log "sdio_bt already loaded"
elif [ -f "$BT_MODULE" ]; then
    if insmod "$BT_MODULE"; then
        log "loaded $BT_MODULE"
    else
        log "ERROR: insmod $BT_MODULE failed"
    fi
else
    log "ERROR: $BT_MODULE not found (aml-bt module missing from rootfs)"
fi
sleep 0.2

log "uart: $(stty -F "$BT_TTY" -a 2>/dev/null | head -1)"
log "starting aml_hciattach on $BT_TTY (output in $HCIATTACH_LOG)"
aml_hciattach -s 115200 "$BT_TTY" aml > "$HCIATTACH_LOG" 2>&1
rc=$?
log "aml_hciattach exited rc=$rc"
sleep 0.1

cnt=10
while [ $cnt -gt 0 ]; do
    if hciconfig hci0 > /dev/null 2>&1; then
        break
    fi
    log "waiting for hci0 ($cnt)"
    sleep 1
    cnt=$((cnt - 1))
done

if [ $cnt -eq 0 ]; then
    log "ERROR: hci0 bring up failed; aml_hciattach output follows"
    sed 's/^/w1_bt_start:   | /' "$HCIATTACH_LOG"
    exit 0
fi

rfkill unblock bluetooth
hciconfig hci0 up
hciconfig hci0 noscan
log "hci0 up: $(hciconfig hci0 | sed -n 's/.*BD Address: \([^ ]*\).*/\1/p')"
