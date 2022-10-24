#!/bin/sh -e

OUTPUT_DIR=workspace/output

# Use this script to flash the QSPI through JTAG

# Set the Boot mode switch to JTAG (ON - ON - ON - ON)

program_flash -f $OUTPUT_DIR/BOOT.BIN -fsbl $OUTPUT_DIR/fsbl.elf -flash_type qspi-x4-single

exit 0