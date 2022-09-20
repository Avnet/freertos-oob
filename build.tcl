#
# Usage: To re-create this platform project launch xsct with below options.
# xsct build.tcl
#

set pfm_name "zub1cg_sbc_factest"
set pfm_xsa ../fpga/zub1cg_sbc_factest.xsa
set sources_folder ./sources
set apps_sources_folder $sources_folder/apps
set bsp_sources_folder $sources_folder/bsp

set workspace ./workspace
setws $workspace

repo -set $bsp_sources_folder

puts "******* Creating Platform"
platform create -name $pfm_name -hw $pfm_xsa -arch {64-bit} -fsbl-target {psu_cortexa53_0} -out $workspace
platform write
domain create -name {freertos10_xilinx_psu_cortexa53_0} -os {freertos10_xilinx} -proc {psu_cortexa53_0} -runtime {cpp} -arch {64-bit}

domain active {freertos10_xilinx_psu_cortexa53_0}
bsp setlib -name xilffs -ver 4.6
bsp config num_logical_vol "1"
bsp config set_fs_rpath "2"
bsp config use_lfn "3"
bsp setlib -name lwip211 -ver 1.6
bsp config api_mode "SOCKET_API"

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
