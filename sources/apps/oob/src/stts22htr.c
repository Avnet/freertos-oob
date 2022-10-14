#include "stts22htr.h"

#include <xparameters.h>
#include "xil_printf.h"
#include "xil_types.h"
#include "xiic.h"
#include "xtime_l.h"
#include "sleep.h"

/************************** Constant Definitions ******************************/

/*
 * The following constants map to the XPAR parameters created in the
 * xparameters.h file. They are defined here such that a user can easily
 * change all the needed parameters in one place.
 */
#define IIC_DEVICE_ID		XPAR_IIC_0_DEVICE_ID

#define STTS22HTR_SLAVE_ADDR		0x3F

#define WHO_AM_I_REG_ADDR 0x01
#define WHO_AM_I_STTS22HTR_VALUE 0xA0

#define CTRL_REG_ADDR 0x04
#define CTRL_LOW_FREERUN (1<<2)
#define CTRL_LOW_IF_ADD_INC (1<<3)

#define TEMP_L_OUT_REG_ADDR 0x06
#define TEMP_H_OUT_REG_ADDR 0x07

#define SOFTWARE_RESET_REG_ADDR 0x0C
#define SOFTWARE_RESET_SW_RESET (1<<1)
#define SOFTWARE_RESET_LOW_ODR (1<<6)

u8 SendBuffer [2];
u8 RecvBuffer [2];

static XIic IicInstance;

int32_t stts22htr_setup(void)
{
	int Status;
	XIic_Config *Cfg;

	/*
	 * Initialize the IIC driver so that it's ready to use
	 * Look up the configuration in the config table, then initialize it.
	 */
	Cfg = XIic_LookupConfig(IIC_DEVICE_ID);
	if (NULL == Cfg) {
		return XST_FAILURE;
	}

	Status = XIic_CfgInitialize(&IicInstance, Cfg, Cfg->BaseAddress);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	SendBuffer[0] = WHO_AM_I_REG_ADDR;
	XIic_Send(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&SendBuffer, 1,XIIC_REPEATED_START);
	XIic_Recv(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&RecvBuffer, 1,XIIC_STOP);

	if(RecvBuffer[0] == WHO_AM_I_STTS22HTR_VALUE){
		xil_printf("stts22htr: Temp Sensor Detected\n\r");
	}
	else{
		xil_printf("stts22htr: Error: Temp Sensor NOT Detected\n\r");
		return XST_FAILURE;
	}

	SendBuffer[0] = CTRL_REG_ADDR;
	SendBuffer[1] = CTRL_LOW_FREERUN | CTRL_LOW_IF_ADD_INC;
	XIic_Send(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&SendBuffer, 2,XIIC_STOP);

	SendBuffer[0] = CTRL_REG_ADDR;
	XIic_Send(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&SendBuffer, 1,XIIC_REPEATED_START);
	XIic_Recv(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&RecvBuffer, 1,XIIC_STOP);

	sleep(1);

	xil_printf("lps22hhtr: Setup Complete\n\r");

	return XST_SUCCESS;
}

int32_t stts22htr_get_temp(float *temp)
{
	uint16_t result;
	SendBuffer[0] = 0x06;
	XIic_Send(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&SendBuffer, 1,XIIC_REPEATED_START);
	XIic_Recv(IicInstance.BaseAddress,STTS22HTR_SLAVE_ADDR,(u8 *)&RecvBuffer, sizeof(RecvBuffer),XIIC_STOP);
	result = RecvBuffer[1] << 8 | RecvBuffer[0];
	*temp = (float) result / 100;

	return XST_SUCCESS;
}
