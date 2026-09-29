#!/bin/sh
# HubV3C (MB v0.2) BL706 control. Zigbee only - no Thread radio is fitted.
#   Zigbee: GPIOZ_1 = DB_RSTN1 (line 1), GPIOZ_3 = DB_ISP1 (line 3) on the
#   periphs bank, gpiochip1; UART /dev/ttyAML3 (uart_AO_B).
# uart_B is disabled in the dts here, so /dev/ttyAML6 never appears.

GPIOCHIP="gpiochip1"
RST_LINE=1
ISP_LINE=3
PORT=/dev/ttyAML3

check_mode()
{
    case "$1" in
        zigbee|blz|zigate) ;;
        thread) echo "Thread is not supported on HubV3C (no Thread radio fitted)."; exit 1 ;;
        *) echo "Invalid mode: $1. Use 'zigbee', 'zigate' or 'blz'."; exit 1 ;;
    esac
}

reset_module()
{
    gpioset "$GPIOCHIP" "$RST_LINE=0"
    sleep 0.1
    gpioset "$GPIOCHIP" "$RST_LINE=1"
    sleep 0.1
}

enter_isp_mode()
{
    gpioset "$GPIOCHIP" "$ISP_LINE=1"
    sleep 0.1
    reset_module
    gpioset "$GPIOCHIP" "$ISP_LINE=0"
    sleep 0.1
}

disable_isp()
{
    gpioset "$GPIOCHIP" "$ISP_LINE=0"
    sleep 0.1
}

BFLB_IOT_DIR="/usr/lib/firmware/bl706/bflb_iot"
export PYTHONWARNINGS="ignore::SyntaxWarning"

flash_firmware()
{
    image_dir="/usr/lib/firmware/bl706/partition_images"

    case "$1" in
        zigate) firmware="${image_dir}/zigate_whole_img.bin" ;;
        *)      firmware="${image_dir}/blz_whole_img.bin" ;;
    esac

    if [ ! -d "${BFLB_IOT_DIR}" ]; then
        echo "Error: ${BFLB_IOT_DIR} not found. Firmware tool not installed."
        exit 1
    fi

    enter_isp_mode

    echo "Burning Image, mode: zigbee. port: $PORT . firmware: $firmware"
    if ! python3 "${BFLB_IOT_DIR}/core/bflb_iot_tool.py" --chipname=bl702 --port="$PORT" --baudrate=921600 --addr=0x0 --firmware="$firmware" --single; then
        echo "Burning failed, retrying..."
        python3 "${BFLB_IOT_DIR}/core/bflb_iot_tool.py" --chipname=bl702 --port="$PORT" --baudrate=921600 --addr=0x0 --firmware="$firmware" --single
    fi

    disable_isp
}

case "$1" in
    start|restart)
        mode=${2:-zigbee}
        check_mode "$mode"
        echo "BL706: $1 zigbee ..."
        disable_isp
        reset_module
        ;;
    flash)
        mode=${2:-zigbee}
        check_mode "$mode"
        echo "BL706: flash $mode ..."
        flash_firmware "$mode"
        reset_module
        ;;
    *)
        echo "Usage: $0 {start|restart|flash} [zigbee|zigate|blz]"
        exit 1
        ;;
esac
