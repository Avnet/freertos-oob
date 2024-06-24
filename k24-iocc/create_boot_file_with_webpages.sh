#!/bin/sh -e

cd workspace/output

# Create FAT image to be used withwebserver application
# This requires root (sudo) access on the Linux host

# create image file of 3MB
dd if=/dev/zero of=ramfs.img bs=512 count=6144 status=none

# format image with FAT
/sbin/mkfs.vfat ramfs.img

# mount it
mkdir mount_point
sudo mount -t vfat -o loop,rw ramfs.img mount_point

# copy your webpages
sudo cp -r  webpages/* mount_point/
sudo umount mount_point/

cat <<EOT > system.bif
//arch = zynqmp; split = false; format = BIN
the_ROM_image:
{
    [bootloader, destination_cpu = a53-0]../k24_iocc_base/export/k24_iocc_base/sw/k24_iocc_base/boot/fsbl.elf
    [destination_device = pl]../k24_iocc_base/export/k24_iocc_base/hw/k24_iocc_base.bit
    [destination_cpu = a53-0]../oob/Debug/oob.elf
    [load = 0x10000000, destination_cpu = a53-0]../output/ramfs.img
}
EOT

bootgen -arch zynqmp -image system.bif -w -o BOOT.BIN

exit 0