#include "lps22hhtr.h"

#include <xparameters.h>
#include "xil_printf.h"
#include "xil_types.h"
#include "xspips.h"		 /* SPI device driver */
#include "xgpio.h"
#include "sleep.h"

#define SPI_DEVICE_ID					XPAR_PSU_SPI_0_DEVICE_ID

#define RREG_CMD 0x20
#define WREG_CMD 0x40

#define WHO_AM_I_REG_ADDR 0x0F
#define CTRL_REG1_REG_ADDR 0x10
#define PRESS_OUT_H_REG_ADDR  0x2A
#define PRESS_OUT_L_REG_ADDR  0x29
#define PRESS_OUT_XL_REG_ADDR  0x28
#define read_bit (1<<7)

#define WHO_AM_I_VALUE 0xB3
#define CTRL_REG1_ODR_1HZ (1<<4)

static XSpiPs SpiInstance;


static int32_t lps22hhtr_read_reg_cmd( uint8_t reg, uint8_t *value )
{
	uint8_t ReadRegCmd[2];
	uint8_t ReadRegData[2];

	ReadRegCmd[0] = read_bit | reg;
	ReadRegCmd[1] = 0x00;

    if(	XSpiPs_PolledTransfer(&SpiInstance, ReadRegCmd, ReadRegData, 2)) {
		xil_printf("Error: lps22hhtr: reading reg.\r\n");
		return XST_FAILURE;
    }

    *value = ReadRegData[1];
	return XST_SUCCESS;
}

static int32_t lps22hhtr_write_reg_cmd( uint8_t reg, uint8_t value )
{
	uint8_t WriteRegCmd[2];

	WriteRegCmd[0] = reg;
	WriteRegCmd[1] = value;

    if(	XSpiPs_PolledTransfer(&SpiInstance, WriteRegCmd, NULL, 2)) {
		xil_printf("Error: lps22hhtr: writing reg.\r\n");
		return XST_FAILURE;
    }

	return XST_SUCCESS;
}

int32_t lps22hhtr_setup(void)
{
	int Status;
	XSpiPs_Config *SpiConfig;
	uint8_t rvalue;

	/*
	 * Initialize the SPI driver so that it's ready to use
	 */
	SpiConfig = XSpiPs_LookupConfig(SPI_DEVICE_ID);
	if (NULL == SpiConfig) {
		return XST_FAILURE;
	}

	Status = XSpiPs_CfgInitialize(&SpiInstance, SpiConfig,
					SpiConfig->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Perform a self-test to check hardware build
	 */
	Status = XSpiPs_SelfTest(&SpiInstance);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XSpiPs_SetOptions(&SpiInstance, XSPIPS_MANUAL_START_OPTION |
				XSPIPS_MASTER_OPTION |
			   XSPIPS_FORCE_SSELECT_OPTION);

	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = XSpiPs_SetClkPrescaler(&SpiInstance, XSPIPS_CLK_PRESCALE_16);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*
	 * Assert the LPS22HHTR chip select
	 */
	Status = XSpiPs_SetSlaveSelect(&SpiInstance, 0x00);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	Status = lps22hhtr_read_reg_cmd(WHO_AM_I_REG_ADDR, &rvalue);
	if (Status)
	{
		xil_printf("Error: lps22hhtr: Failed to read WHO_AM_I reg\r\n");
		return XST_FAILURE;
	}

	if ( rvalue != WHO_AM_I_VALUE ) {
		xil_printf("Error: lps22hhtr: wrong whomai value (read %02X instead of %02X)\r\n", rvalue, WHO_AM_I_VALUE);
		return XST_FAILURE;
	}

	// Set Output data rate to 1Hz
	Status = lps22hhtr_write_reg_cmd(CTRL_REG1_REG_ADDR, CTRL_REG1_ODR_1HZ);
	if (Status) {
		xil_printf("Error: lps22hhtr: Failed to write CTRL_REG1 reg\r\n");
		return XST_FAILURE;
	}

	sleep(1);

	return XST_SUCCESS;
}

int32_t lps22hhtr_get_pressure(float *pressure)
{
	int32_t ret;
	uint8_t rvalue;
	int32_t press_raw = 0;


	ret = lps22hhtr_read_reg_cmd(PRESS_OUT_H_REG_ADDR, &rvalue);
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to read reg\r\n");
		return XST_FAILURE;
	}
	press_raw += rvalue << 16;

	ret = lps22hhtr_read_reg_cmd(PRESS_OUT_L_REG_ADDR, &rvalue);
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to read reg\r\n");
		return XST_FAILURE;
	}
	press_raw += rvalue << 8;

	ret = lps22hhtr_read_reg_cmd(PRESS_OUT_XL_REG_ADDR, &rvalue);
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to read reg\r\n");
		return XST_FAILURE;
	}
	press_raw += rvalue;

	*pressure = ((float)press_raw / 4096.0);

	return XST_SUCCESS;
}



int32_t lps22hhtr_run_example(void)
{
	int32_t ret;
	float pressure;

	ret = lps22hhtr_setup();
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to setup LPS22HHTR.\r\n");
		return XST_FAILURE;
	}

	ret = lps22hhtr_get_pressure(&pressure);
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to get pressure value\r\n");
		return XST_FAILURE;
	}

	printf("lps22hhtr: pressure is %f \r\n", pressure);
/*
	sleep(1);

	ret = lps22hhtr_get_pressure(&pressure);
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to get pressure value\r\n");
		return XST_FAILURE;
	}

	printf("lps22hhtr: pressure is %f \r\n", pressure);
	sleep(1);

	ret = lps22hhtr_get_pressure(&pressure);
	if (ret) {
		xil_printf("Error: lps22hhtr: Failed to get pressure value\r\n");
		return XST_FAILURE;
	}

	printf("lps22hhtr: pressure is %f \r\n", pressure);

*/
	xil_printf("lps22hhtr: Successful \r\n");

	return XST_SUCCESS;
}
