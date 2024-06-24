# K24 IOCC FreeRTOS Out of the Box Image

This repository hosts all the code needed to rebuild the FreeRTOS Out of the Box Image for the AMD K24 SOM with the Avnet K24 I/O Carrier Card.

The FreeRTOS Out of the Box Image is a lightweight FreeRTOS application to demonstrate some of the capabilities of the board.
The application starts a small webserver that allows the users to interact with the board and discover more about it.

The webpages are either stored directly on the sd card, or embedded in the BOOT.BIN file to be stored in the QSPI and loaded into RAM at boot.

## Step to rebuild

### 1. Build the Vivado design

Use the hdl repository (<https://github.com/Avnet/hdl/tree/2023.2>) to build the K24 IOCC Base design
> vivado -mode batch -source scripts/make_k24_iocc_base.tcl

This will create an XSA file, located in hdl/projects/k24_iocc_base_2023_2/k24_iocc_base.xsa.
	
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

- Modify the Boot Mode Switches to boot from SD card (SW1[4-1]: ON-OFF-ON-OFF) and push the ON_OFF Button (PB7).


#### From the QSPI

- Plug in the USB-C cable between the host PC and the USB JTAG/UART port (J19).

- Modify the Boot Mode Switches to boot from JTAG (SW1[4-1]: ON-ON-ON-ON) and push the ON_OFF Button (PB7).

- Use the following command to flash the BOOT.BIN into the QSPI:
	> ./flash_qspi.sh

- Modify the Boot Mode Switches to boot from QSPI (SW1[4-1]: ON-ON-OFF-ON) and push the PS_POR Button (PB7).


## Get the UART output

- Plug in the USB-C cable between the host PC and the USB JTAG/UART port (J19).

- Open a serial terminal on the second port of the board (Baud Rate is 115200).

	For example, on Linux, you can use:

	> picocom -b 115200 /dev/ttyUSB1

## Access the Web Server

The application will try to get an IP with DHCP for 10 seconds.

If DHCP failed, '192.168.2.10' will be used as a static IP address.

- Connect the Ethernet cable on the board.

- Look at the serial logs to see which IP address was assigned to the board

- Open a web browser on the host PC and browse to the board IP address as the URL (For example, http://192.168.2.10). The webpage
should open in the browser. Your host PC must have an IP address in the same subnet.
