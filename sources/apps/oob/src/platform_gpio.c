#include "xparameters.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xgpiops.h"


#define RGB_LED_0_DEVICE_ID  XPAR_GPIO_0_DEVICE_ID
#define MIO_DEVICE_ID XPAR_XGPIOPS_0_DEVICE_ID

XGpio gpio_rbg_led_0;
XGpioPs gpio_mio;

#define MIO_LED_0_PIN	7
#define MIO_LED_1_PIN	24
#define MIO_LED_2_PIN	25
#define MIO_LED_3_PIN	33

#define MIO_SW_0_PIN	44
#define MIO_SW_1_PIN	40
#define MIO_SW_2_PIN	39
#define MIO_SW_3_PIN	31

int32_t platform_init_gpios()
{
	int Status;
	XGpioPs_Config *ConfigPtr;

	/* Initialize the GPIO driver */
	Status = XGpio_Initialize(&gpio_rbg_led_0, RGB_LED_0_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
		return XST_FAILURE;
	}

	/* Initialize the GPIO driver. */
	ConfigPtr = XGpioPs_LookupConfig(MIO_DEVICE_ID);
	Status = XGpioPs_CfgInitialize(&gpio_mio, ConfigPtr,
					ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Set the direction for all gpio_rbg_led_0 signals as output */
	XGpio_SetDataDirection(&gpio_rbg_led_0, 1, 0);

	/*
	 * Set the direction for the pin to be output and
	 * Enable the Output enable for the LED Pin.
	 */
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_LED_0_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_LED_0_PIN, 1);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_LED_1_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_LED_1_PIN, 1);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_LED_2_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_LED_2_PIN, 1);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_LED_3_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_LED_3_PIN, 1);

	/* Set the MIO output to be low. */
	XGpioPs_WritePin(&gpio_mio, MIO_LED_0_PIN, 0x0);
	XGpioPs_WritePin(&gpio_mio, MIO_LED_1_PIN, 0x0);
	XGpioPs_WritePin(&gpio_mio, MIO_LED_2_PIN, 0x0);
	XGpioPs_WritePin(&gpio_mio, MIO_LED_3_PIN, 0x0);

	/* Set the direction for the specified pin to be input. */
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_0_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_1_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_2_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_3_PIN, 0x0);

	return XST_SUCCESS;
}

int toggle_leds()
{
    static int state = 0;
    static int led = 0;
    state = ~state;
    XGpio_DiscreteWrite(&gpio_rbg_led_0, 1, 1 << led);
    led = (led + 1) % 3;
    return 0;
}

unsigned int get_switch_state()
{
	return XGpioPs_ReadPin(&gpio_mio, MIO_SW_3_PIN) << 3 |
			XGpioPs_ReadPin(&gpio_mio, MIO_SW_2_PIN) << 2 |
			XGpioPs_ReadPin(&gpio_mio, MIO_SW_1_PIN) << 1 |
			XGpioPs_ReadPin(&gpio_mio, MIO_SW_0_PIN);
}
