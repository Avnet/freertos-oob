#
# Usage: To re-create this platform project launch xsct with below options.
# xsct build.tcl
#

set pfm_name "zub1cg_sbc_oob"
set pfm_xsa ../fpga/zub1cg_sbc_oob.xsa
set sources_folder ./sources
set apps_sources_folder $sources_folder/apps
set bsp_sources_folder $sources_folder/bsp
set webpages_sources_folder $sources_folder/webpages


# set WEBPAGES_ON_SD_CARD to 0 to create a RAM based FAT FS to store webpages,
#    that will be included in the BOOT.BIN
# set WEBPAGES_ON_SD_CARD to 1 if you want to store the webpages directly on the sd card
set WEBPAGES_ON_SD_CARD 1

set workspace ./workspace
setws $workspace

set output_folder $workspace/output
file mkdir $output_folder

repo -set $bsp_sources_folder

puts "******* Creating Platform"
platform create -name $pfm_name -hw $pfm_xsa -arch {64-bit} -fsbl-target {psu_cortexa53_0} -out $workspace
platform write
domain create -name {freertos10_xilinx_psu_cortexa53_0} -os {freertos10_xilinx} -proc {psu_cortexa53_0} -runtime {cpp} -arch {64-bit}

domain active {freertos10_xilinx_psu_cortexa53_0}
bsp setlib -name xilffs -ver 4.6
bsp config num_logical_vol "1"

if { $WEBPAGES_ON_SD_CARD == 0 } {
    bsp config fs_interface "2"
    bsp config ramfs_start_addr "0x10000000"
}

bsp config set_fs_rpath "2"
bsp config use_lfn "3"
bsp setlib -name lwip211 -ver 1.6
bsp config api_mode "SOCKET_API"
bsp config phy_link_speed "CONFIG_LINKSPEED10"

# # Used to make the platform seen in the workspace
importprojects $workspace
platform generate

set oob_sys "oob_system"
set oob_app "oob"
puts "******* Creating $oob_app App"
app create -name $oob_app -template {Empty Application(C)} -lang c -platform $pfm_name -domain {freertos10_xilinx_psu_cortexa53_0} -sysproj $oob_sys

puts "******* Importing $oob_app sources, from [file normalize $apps_sources_folder/$oob_app/src]"
importsources -name $oob_app -path [file normalize $apps_sources_folder/$oob_app/src/ ] -soft-link
app config -name $oob_app -set linker-script [file normalize $apps_sources_folder/$oob_app/src/lscript.ld]

app build -all

puts ""
puts ""

if { $WEBPAGES_ON_SD_CARD == 0 } {
    puts "******* Webpages will be stored on a RAM Filesystem, included in the BOOT.BIN"
    puts ""
    puts "** Copying Webpages and FSBL in the output folder ($output_folder)"
    file copy -force [file normalize $webpages_sources_folder] [file normalize $output_folder]
    file copy -force [file normalize $workspace/$pfm_name/export/$pfm_name/sw/$pfm_name/boot/fsbl.elf] [file normalize $output_folder]

    puts "** Creating the RAM fs and the BOOT.bin file"
    if { [catch { exec ./create_boot_file_with_webpages.sh } msg] } {
       puts "Something seems to have gone wrong:"
       puts "Information about it: $::errorInfo"
    }

    puts ""
    puts "$msg"

    puts "******* Done! You can now use the BOOT.BIN in the output folder, or use the flash_qspi.sh script to load it in the QSPI"

} else {
    puts "******* Webpages will be stored externally (on a SD card)"
    puts ""
    file copy {*}[glob $webpages_sources_folder/*] [file normalize $output_folder]
    file link -hard [file normalize $output_folder/BOOT.BIN] [file normalize $workspace/$oob_sys/Debug/sd_card/BOOT.BIN]

    puts "******* Done! You can now copy the content of the output folder ($output_folder) on a SD card"
}

puts ""
