/*
 * Copyright (C) 2017 - 2020 Xilinx, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT
 * SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
 * OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 *
 */

#include "ff.h"
#include "xil_printf.h"

static int fat_ls(const char *path)
{
	FRESULT res;
	FILINFO fno;
	DIR dir;
	int nfiles = 0;
	int ndirs = 0;


	res = f_opendir(&dir, path);                       /* Open the directory */
	if (res == FR_OK) {
		xil_printf("ls %s:\r\n", path);
		for (;;) {
			res = f_readdir(&dir, &fno);                   /* Read a directory item */
			if (res != FR_OK || fno.fname[0] == 0) break;  /* Break on error or end of dir */
			if (fno.fattrib & AM_DIR) {                    /* It is a directory */
				xil_printf("\t%s/\r\n", fno.fname);
				ndirs++;
			} else {                                       /* It is a file. */
				xil_printf("\t%s\r\n", fno.fname);
				nfiles++;
			}
		}
		f_closedir(&dir);
	}
	xil_printf("\r\n\t(found %d files, %d directories)\r\n\r\n", nfiles, ndirs);

	return res;
}

int platform_init_fs()
{
	static FATFS fatfs;
	static FIL fil;		/* File object */
	FRESULT Res;
	TCHAR *Path = "0:/";
	int res;

	/*
	 * Register volume work area, initialize device
	 */
	Res = f_mount(&fatfs, Path, 1);
	if (Res != FR_OK) {
		xil_printf("Failed to mount FAT FS \r\n");
		return -1;
	}

	res = fat_ls("/");
	if (res)
	{
		xil_printf("Failed fat_ls (%d)\r\n", res);
		return -1;
	}

	Res = f_open(&fil, "index.html", FA_READ);
	if (Res) {
		xil_printf("%s: ERROR: unable to locate index.html in FS\r\n",
			   __func__);
	}

	/* Closing the file */
	f_close(&fil);
	return 0;
}
