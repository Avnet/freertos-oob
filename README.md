# ZUBoard 1CG FreeRTOS Out of the Box Image

This repository hosts all the code needed to rebuild the FreeRTOS Out of the Box Image for the Avnet ZUBoard 1CG board.

The FreeRTOS Out of the Box Image is a lightweight FreeRTOS application to demonstrate some of the capabilities of the board.
The application starts a small webserver that allows the users to interact with the board and discover more about it.

The webpages are either stored directly on the sd card, or embedded in the BOOT.BIN file to be stored in the QSPI and loaded into RAM at boot.

## Step to rebuild

### 1. Build the Vivado design

Use the hdl repository (<https://github.com/Avnet/hdl/tree/2021.2>) to build the ZUBoard 1CG OOB design
> vivado -mode batch -source scripts/make_zub1cg_sbc_oob.tcl

This will create an XSA file, located in hdl/projects/zub1cg_sbc_oob_2021_2/zub1cg_sbc_oob.xsa.
	
### 2. Modify the FreeRTOS OOB build script
	
In the 'build.tcl' in this repository, you can modify the folowing variables:
> 'pfm_xsa' to modify the location of your XSA file from step 1

> 'WEBPAGES_ON_SD_CARD' 0 to create a RAM based FAT FS to store webpages, or 1 if you want to store the webpages directly on the sd card

### 3. Launch the build script

> xsct build.tcl

Once built, you can use Vitis and select the workspace folder to modify the BSP or the application.

Warning: each time you want to rebuild the project, you should remove the workpace folder.


## Boot the board

#### From the SD card
- Copy everything from the workspace/output folder into an SD card.

- Insert the SD card into the board.

- Modify the Boot Mode Switches to boot from SD card (OFF-ON-OFF-ON) and push the ON_OFF Button (SW7).


#### From the QSPI

- Plug in the micro‐USB cable between the host PC and the USB JTAG/UART port (J16).

- Modify the Boot Mode Switches to boot from JTAG (ON-ON-ON-ON) and push the ON_OFF Button (SW7).

- Use the following command to flash the BOOT.BIN into the QSPI:
	> ./flash_qspi.sh

- Modify the Boot Mode Switches to boot from QSPI (ON-OFF-ON-ON) and push the PS_POR Button (SW7).


## Get the UART output

- Plug in the micro‐USB cable between the host PC and the USB JTAG/UART port (J16).

- Open a serial terminal on the second port of the board (Baud Rate is 115200).

	For example, on Linux, you can use:

	> picocom -b 115200 /dev/ttyUSB1

## Access the Web Server

The application is set to use '192.168.2.10' as its static IP address.

This can be changed to use a different static address or DHCP (not tested).


- Connect the board and the host PC with an Ethernet cable.

- Set the host PC to use a static address in the same subnet ('192.168.2.2' for example).

- Open a web browser on the host PC and browse to the board IP address as the URL (For example, http://192.168.2.10). The webpage
should open in the browser.



![image](https://user-images.githubusercontent.com/55467813/196303037-7f288c03-3f3c-41e8-b291-d7a622734172.png)
