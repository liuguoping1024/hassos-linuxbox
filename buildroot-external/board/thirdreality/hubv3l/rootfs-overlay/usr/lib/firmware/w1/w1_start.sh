#!/bin/bash
# HubV3L WiFi driver loader (5.4 kernel, out-of-tree build)
#
# Modules installed to /lib/modules/$(uname -r)/kernel/amlogic/wifi/
# by the aml-wifi buildroot package.

MODDIR="/lib/modules/$(uname -r)/kernel/amlogic/wifi"

# Power on the WiFi chip before loading modules. The /sys/class/aml_wifi/power
# node is provided by the in-kernel aml_wifi platform driver (present at boot,
# before insmod). Powering on enables the SDIO slot so aml_sdio can enumerate.
if [ -e /sys/class/aml_wifi/power ]; then
    echo "Powering on WiFi (aml_wifi)..."
    echo 1 > /sys/class/aml_wifi/power
    sleep 1
fi

# Load WiFi SDIO transport
if ! lsmod | grep -q "^aml_sdio"; then
    echo "Loading module: aml_sdio.ko ..."
    insmod ${MODDIR}/aml_sdio.ko
else
    echo "Module aml_sdio.ko is already loaded."
fi

# Load main WiFi driver
if ! lsmod | grep -q "^vlsicomm"; then
    echo "Loading module: vlsicomm.ko ..."
    insmod ${MODDIR}/vlsicomm.ko
else
    echo "Module vlsicomm.ko is already loaded."
fi

# Function to check and bring up a network interface.
# NOTE: buildroot/busybox rootfs has no `ifconfig`; use iproute2 `ip` instead.
bring_up_interface() {
    local iface="$1"
    if ip link show "$iface" &>/dev/null; then
        if ip link show "$iface" | grep -q "state UP"; then
            echo "Interface $iface exists and is already up."
        else
            echo "Interface $iface is down, bringing it up..."
            ip link set "$iface" up
        fi
    else
        echo "Interface $iface does not exist."
    fi
}

# Function to check and bring down a network interface.
bring_down_interface() {
    local iface="$1"
    if ip link show "$iface" &>/dev/null; then
        if ip link show "$iface" | grep -q "state UP"; then
            echo "Interface $iface is up, bringing it down..."
            ip link set "$iface" down
        else
            echo "Interface $iface exists but is already down."
        fi
    else
        echo "Interface $iface does not exist."
    fi
}

# Bring up wlan0, bring down wlan1 and p2p0.
# WARNING: Do NOT delete p2p0! The driver keeps a resident timer/work thread
# (hal_work_thread -> wifi_mac_set_scan_time -> wifi_mac_get_wnet_vif_by_vid)
# that references the p2p0 vif (vid 1). Deleting it (`iw dev p2p0 del`) makes
# that timer dereference a freed/NULL vif and triggers a kernel panic shortly
# after boot. p2p0 is harmless when just left down + unmanaged (the NM config
# already excludes it via unmanaged-devices).
bring_up_interface "wlan0"
bring_down_interface "wlan1"
bring_down_interface "p2p0"
