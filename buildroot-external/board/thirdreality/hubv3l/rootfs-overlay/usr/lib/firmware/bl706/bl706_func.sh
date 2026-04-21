#!/bin/sh
# refer to pinctrl-meson-axg.c
# 3R-LinuxBOx_MA v0.1 2022.11.11

reset_module()
{
    if [ "$1" = "zigbee" ]; then
        # A113X_RST_ZG: GPIOA_17
        gpioset 0 43=0
        sleep 0.1
        gpioset 0 43=1
    elif [ "$1" = "thread" ]; then
        # Thread reset
        gpioset 0 2=0
        sleep 0.1
        gpioset 0 2=1
    else
        echo "Invalid mode: $1. Use 'zigbee' or 'thread'."
        exit 1
    fi
    sleep 0.1
}

enter_isp_mode()
{
    if [ "$1" = "zigbee" ]; then
        # Z_ISP: GPIOA_16
        gpioset 0 42=1
        sleep 0.1

        reset_module "zigbee"

        gpioset 0 42=0
    elif [ "$1" = "thread" ]; then
        # Thread boot
        gpioset 0 4=1
        sleep 0.1

        reset_module "thread"

        gpioset 0 4=0
    else
        echo "Invalid mode: $1. Use 'zigbee' or 'thread'."
        exit 1
    fi

    sleep 0.1
}

disable_isp()
{
    if [ "$1" = "zigbee" ]; then
        # Z_ISP: GPIOA_16
        gpioset 0 42=0
    elif [ "$1" = "thread" ]; then
        gpioset 0 4=0
    fi
    sleep 0.1
}

BFLB_IOT_DIR="/usr/lib/firmware/bl706/bflb_iot"
export PYTHONWARNINGS="ignore::SyntaxWarning"

check_thread_support()
{
    if [ ! -c "/dev/ttyAML6" ]; then
        echo "Error: Thread is not supported on this hardware (V3A). /dev/ttyAML6 not found."
        exit 1
    fi
}

flash_firmware()
{
    mode=$1
    image_size_dir="partition_images"

    if [ "$mode" = "zigbee" ]; then
        port="/dev/ttyAML3"
        firmware="/usr/lib/firmware/bl706/${image_size_dir}/blz_whole_img.bin"
    elif [ "$mode" = "zigate" ]; then
        port="/dev/ttyAML3"
        firmware="/usr/lib/firmware/bl706/${image_size_dir}/zigate_whole_img.bin"
        mode="zigbee"
    elif [ "$mode" = "blz" ]; then
        port="/dev/ttyAML3"
        firmware="/usr/lib/firmware/bl706/${image_size_dir}/blz_whole_img.bin"
        mode="zigbee"
    elif [ "$mode" = "thread" ]; then
        check_thread_support
        port="/dev/ttyAML6"
        firmware="/usr/lib/firmware/bl706/${image_size_dir}/thread_whole_img.bin"
    else
        echo "Invalid mode: $mode. Use 'zigbee', 'zigate', 'blz' or 'thread'."
        exit 1
    fi

    if [ ! -d "${BFLB_IOT_DIR}" ]; then
        echo "Error: ${BFLB_IOT_DIR} not found. Firmware tool not installed."
        exit 1
    fi

    enter_isp_mode $mode

    echo "Burning Image, mode: $mode. port: $port . firmware: $firmware"
    python3 "${BFLB_IOT_DIR}/core/bflb_iot_tool.py" --chipname=bl702 --port=$port --baudrate=921600 --addr=0x0 --firmware="$firmware" --single

    if [ $? -eq 0 ]; then
        echo "Burn successfully"
    else
        echo "Burning failed, retrying..."
        python3 "${BFLB_IOT_DIR}/core/bflb_iot_tool.py" --chipname=bl702 --port=$port --baudrate=921600 --addr=0x0 --firmware="$firmware" --single
    fi

    disable_isp $mode
}

case "$1" in
    start)
    mode=${2:-zigbee}
    [ "$mode" = "thread" ] && check_thread_support
    echo "BL706: start $mode ..."
    disable_isp $mode
    reset_module $mode
    ;;

    restart)
    mode=${2:-zigbee}
    [ "$mode" = "thread" ] && check_thread_support
    echo "BL706: restart $mode ..."
    disable_isp $mode
    reset_module $mode
    ;;

    flash)
    mode=${2:-zigbee}
    echo "BL706: flash $mode ..."
    flash_firmware $mode
    disable_isp $mode
    reset_module $mode
    ;;

    *)
    echo "Usage: $0 {start [zigbee|thread]|restart [zigbee|thread]|flash [zigbee|thread]}"
    exit 1
    ;;
esac
