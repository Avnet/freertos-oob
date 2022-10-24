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
    [bootloader, destination_cpu = a53-0]../zub1cg_sbc_oob/export/zub1cg_sbc_oob/sw/zub1cg_sbc_oob/boot/fsbl.elf
    [destination_device = pl]../zub1cg_sbc_oob/export/zub1cg_sbc_oob/hw/zub1cg_sbc_oob.bit
    [destination_cpu = a53-0]../oob/Debug/oob.elf
    [load = 0x10000000, destination_cpu = a53-0]../output/ramfs.img
}
EOT

bootgen -arch zynqmp -image system.bif -w -o BOOT.BIN

exit 0