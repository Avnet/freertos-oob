#include "platform_gpio.h"
#include "xparameters.h"
#include "xgpio.h"
#include "xil_printf.h"
#include "xgpiops.h"
#include "xscugic.h"
#include "FreeRTOS.h"
#include "task.h"
#include "sleep.h"


#define PL_LED_DEVICE_ID  XPAR_GPIO_0_DEVICE_ID
#define PL_PB_DEVICE_ID  XPAR_GPIO_1_DEVICE_ID
#define MIO_DEVICE_ID XPAR_XGPIOPS_0_DEVICE_ID
#define GPIO_INTERRUPT_ID	XPAR_XGPIOPS_0_INTR

#define MIO_LED_0_PIN	27
#define MIO_LED_1_PIN	28
#define MIO_LED_2_PIN	29
#define MIO_LED_3_PIN	30

#define MIO_RGB_0_PIN	38
#define MIO_RGB_1_PIN	39
#define MIO_RGB_2_PIN	40

#define MIO_SW_0_PIN	41
#define MIO_SW_1_PIN	42
#define MIO_SW_2_PIN	43
#define MIO_SW_3_PIN	44

#define MIO_PS_PB_0_PIN	31
#define MIO_PS_PB_1_PIN	35

/*
MIO_SW_0_PIN is located bank 1 pin 15
MIO_SW_1_PIN is located bank 1 pin 16
MIO_SW_2_PIN is located bank 1 pin 17
MIO_SW_3_PIN is located bank 1 pin 18
MIO_PS_PB_0_PIN is located bank 1 pin 5
MIO_PS_PB_1_PIN is located bank 1 pin 9
*/

#define MIO_BANK 1

#define MIO_SW_0_PIN_IN_BANK (MIO_SW_0_PIN-26)
#define MIO_SW_1_PIN_IN_BANK (MIO_SW_1_PIN-26)
#define MIO_SW_2_PIN_IN_BANK (MIO_SW_2_PIN-26)
#define MIO_SW_3_PIN_IN_BANK (MIO_SW_3_PIN-26)
#define MIO_PS_PB_0_PIN_IN_BANK (MIO_PS_PB_0_PIN-26)
#define MIO_PS_PB_1_PIN_IN_BANK (MIO_PS_PB_1_PIN-26)

XGpio gpio_pl_led;
XGpio gpio_pl_pb;
XGpioPs gpio_mio;

// XScuGic already initialized by FreeRTOS_SetupTickInterrupt
extern XScuGic xInterruptController;

static void mio_interrupt_handler(void *CallBackRef, u32 Bank, u32 Status);

static color_t rgbled0_status = 0;
static color_t rgbled1_status = 0;
static uint32_t pl_pb_status = 0x3;

static TaskHandle_t pl_pb_task;

void pl_pb_polling_task(void *unused_arg)
{
	color_t last_rgbled0_status = 0;
	color_t last_rgbled1_status = 0;

	uint32_t current_pb_status = 0;
	while(TRUE)
	{
		current_pb_status = XGpio_DiscreteRead(&gpio_pl_pb, 1);
		if(current_pb_status != pl_pb_status)
		{
			if((current_pb_status & 0x1) != (pl_pb_status & 0x1) )
			{
				//Button 0
				if((current_pb_status & 0x1) == 0)
				{
					// button pressed, we turn the RGB LED to RED

					//save the current state for release time
					last_rgbled0_status = rgbled0_status;

					control_rgb_leds(0, COLOR_RED);
				}
				else
				{
					// button released, we restore RGB LEDs
					control_rgb_leds(0, last_rgbled0_status);
				}
			}

			if((current_pb_status & 0x2) != (pl_pb_status & 0x2) )
			{
				//Button 1
				if((current_pb_status & 0x2) == 0)
				{
					// button pressed, we turn the RGB LED to RED

					//save the current state for release time
					last_rgbled1_status = rgbled1_status;

					control_rgb_leds(1, COLOR_RED);
				}
				else
				{
					// button released, we restore RGB LEDs
					control_rgb_leds(1, last_rgbled1_status);
				}
			}

			pl_pb_status = current_pb_status;
		}
		/* Block for 100ms. */
		vTaskDelay(100 / portTICK_PERIOD_MS);
	}

    vTaskDelete( NULL );
}

static int setup_mio_interrupt(void)
{
	int Status;

	// The interrupt controller should already by ready
	if (xInterruptController.IsReady != XIL_COMPONENT_IS_READY) {
		return XST_FAILURE;
	}

	/*
	 * Connect the device driver handler that will be called when an
	 * interrupt for the device occurs, the handler defined above performs
	 * the specific interrupt processing for the device.
	 */
	Status = XScuGic_Connect(&xInterruptController, GPIO_INTERRUPT_ID,
				(Xil_ExceptionHandler)XGpioPs_IntrHandler,
				(void *)&gpio_mio);
	if (Status != XST_SUCCESS) {
		return Status;
	}

	/* Enable interrupts on all edges for all the pins in GPIO bank. */
	XGpioPs_SetIntrType(&gpio_mio, MIO_BANK, 0xFFFFFFFF, 0x00, 0xFFFFFFFF);

	/* Set the handler for gpio interrupts. */
	XGpioPs_SetCallbackHandler(&gpio_mio, (void *)&gpio_mio, mio_interrupt_handler);

	/* Enable the GPIO interrupts of GPIO Bank. */
	XGpioPs_IntrEnable(&gpio_mio, MIO_BANK,
			(1 << MIO_SW_0_PIN_IN_BANK) |
			(1 << MIO_SW_1_PIN_IN_BANK) |
			(1 << MIO_SW_2_PIN_IN_BANK) |
			(1 << MIO_SW_3_PIN_IN_BANK) |
			(1 << MIO_PS_PB_0_PIN_IN_BANK) |
			(1 << MIO_PS_PB_1_PIN_IN_BANK));

	/* Enable the interrupt for the GPIO device. */
	XScuGic_Enable(&xInterruptController, GPIO_INTERRUPT_ID);

	/* Enable interrupts in the Processor. */
	Xil_ExceptionEnableMask(XIL_EXCEPTION_IRQ);

	return XST_SUCCESS;
}

int32_t platform_init_gpios()
{
	int Status;
	XGpioPs_Config *ConfigPtr;
	BaseType_t stat;

	/* Initialize the GPIO driver */
	Status = XGpio_Initialize(&gpio_pl_led, PL_LED_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
		return XST_FAILURE;
	}

	/* Set the direction for all gpio_rbg_led_0 signals as output */
	XGpio_SetDataDirection(&gpio_pl_led, 1, 0);
	XGpio_SetDataDirection(&gpio_pl_led, 2, 0);

	/* Initialize the GPIO driver */
	Status = XGpio_Initialize(&gpio_pl_pb, PL_PB_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Gpio Initialization Failed\r\n");
		return XST_FAILURE;
	}

	/* Set the direction for all gpio_pl_pb signals as input */
	XGpio_SetDataDirection(&gpio_pl_pb, 1, 0xFFFFFFFF);

	/* Initialize the GPIO driver. */
	ConfigPtr = XGpioPs_LookupConfig(MIO_DEVICE_ID);
	Status = XGpioPs_CfgInitialize(&gpio_mio, ConfigPtr,
					ConfigPtr->BaseAddr);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Set the direction for the specified pin to be input. */
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_0_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_1_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_2_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_SW_3_PIN, 0x0);

	XGpioPs_SetDirectionPin(&gpio_mio, MIO_PS_PB_0_PIN, 0x0);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_PS_PB_1_PIN, 0x0);

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

	XGpioPs_SetDirectionPin(&gpio_mio, MIO_RGB_0_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_RGB_0_PIN, 1);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_RGB_1_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_RGB_1_PIN, 1);
	XGpioPs_SetDirectionPin(&gpio_mio, MIO_RGB_2_PIN, 1);
	XGpioPs_SetOutputEnablePin(&gpio_mio, MIO_RGB_2_PIN, 1);

	/* Set the MIO output to reflect MIO SW positions. */
	XGpioPs_WritePin(&gpio_mio, MIO_LED_3_PIN, XGpioPs_ReadPin(&gpio_mio, MIO_SW_0_PIN));
	XGpioPs_WritePin(&gpio_mio, MIO_LED_2_PIN, XGpioPs_ReadPin(&gpio_mio, MIO_SW_1_PIN));
	XGpioPs_WritePin(&gpio_mio, MIO_LED_1_PIN, XGpioPs_ReadPin(&gpio_mio, MIO_SW_2_PIN));
	XGpioPs_WritePin(&gpio_mio, MIO_LED_0_PIN, XGpioPs_ReadPin(&gpio_mio, MIO_SW_3_PIN));

	/*
	 * Setup the interrupts such that interrupt processing can occur. If
	 * an error occurs then exit.
	 */
	Status = setup_mio_interrupt();
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	stat = xTaskCreate(pl_pb_polling_task, ( const char * ) "pl_pb_polling_task",
				1024, NULL, 2, &pl_pb_task);
	if (stat != pdPASS) {
		xil_printf("Error: cannot create task\r\n");
		return XST_FAILURE;
	}

	return XST_SUCCESS;
}

static void mio_interrupt_handler(void *CallBackRef, u32 Bank, u32 Status)
{
	XGpioPs *Gpio = (XGpioPs *)CallBackRef;

	if (Bank != MIO_BANK)
	{
		//unknown interrupt
		return;
	}

	switch(Status)
	{
		case (1 << MIO_SW_0_PIN_IN_BANK):
			XGpioPs_WritePin(Gpio, MIO_LED_3_PIN, XGpioPs_ReadPin(Gpio, MIO_SW_0_PIN));
			break;
		case (1 << MIO_SW_1_PIN_IN_BANK):
			XGpioPs_WritePin(Gpio, MIO_LED_2_PIN, XGpioPs_ReadPin(Gpio, MIO_SW_1_PIN));
			break;
		case (1 << MIO_SW_2_PIN_IN_BANK):
			XGpioPs_WritePin(Gpio, MIO_LED_1_PIN, XGpioPs_ReadPin(Gpio, MIO_SW_2_PIN));
			break;
		case (1 << MIO_SW_3_PIN_IN_BANK):
			XGpioPs_WritePin(Gpio, MIO_LED_0_PIN, XGpioPs_ReadPin(Gpio, MIO_SW_3_PIN));
			break;
		case (1 << MIO_PS_PB_0_PIN_IN_BANK):
			if(XGpioPs_ReadPin(Gpio, MIO_PS_PB_0_PIN)==0)
			{
				// button is pressed, we turn on the led
				XGpio_DiscreteSet(&gpio_pl_led, 1, 1);
			}
			else
			{
				XGpio_DiscreteClear(&gpio_pl_led, 1, 1);
			}
		//			u32 pin_value= XGpioPs_ReadPin(Gpio, MIO_PS_PB_0_PIN);
		//			u32 previous_value= XGpio_DiscreteRead(&gpio_pl_led, 1);
		//			u32 value_to_write = XGpio_DiscreteRead(&gpio_pl_led, 1) ^ 1;
		//			XGpio_DiscreteWrite(&gpio_pl_led, 1, XGpio_DiscreteRead(&gpio_pl_led, 1) ^ 1);
			break;
		case (1 << MIO_PS_PB_1_PIN_IN_BANK):
			if(XGpioPs_ReadPin(Gpio, MIO_PS_PB_1_PIN)==0)
			{
				// button is pressed, we turn on the led
				XGpio_DiscreteSet(&gpio_pl_led, 1, 1<<1);
			}
			else
			{
				XGpio_DiscreteClear(&gpio_pl_led, 1, 1<<1);
			}
			break;
		default:
			//unknown interrupt
			return;
	}
}

int control_rgb_leds(int led_index, color_t color)
{
	if (led_index == 0)
	{
		switch(color) {
			case COLOR_NONE:
			    XGpio_DiscreteWrite(&gpio_pl_led, 2, 0);
			    rgbled0_status=COLOR_NONE;
				break;
			case COLOR_RED:
			    XGpio_DiscreteWrite(&gpio_pl_led, 2, 1<<0);
			    rgbled0_status=COLOR_RED;
				break;
			case COLOR_GREEN:
			    XGpio_DiscreteWrite(&gpio_pl_led, 2, 1<<1);
			    rgbled0_status=COLOR_GREEN;
				break;
			case COLOR_BLUE:
			    XGpio_DiscreteWrite(&gpio_pl_led, 2, 1<<2);
			    rgbled0_status=COLOR_BLUE;
				break;
			default:
				xil_printf("control_rgb_leds: unknown color\r\n");
				return XST_FAILURE;
		}
	}
	else if (led_index == 1)
	{
		switch(color) {
			case COLOR_NONE:
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_0_PIN, 0);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_1_PIN, 0);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_2_PIN, 0);
			    rgbled1_status=COLOR_NONE;
				break;
			case COLOR_RED:
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_0_PIN, 1);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_1_PIN, 0);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_2_PIN, 0);
			    rgbled1_status=COLOR_RED;
				break;
			case COLOR_GREEN:
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_0_PIN, 0);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_1_PIN, 1);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_2_PIN, 0);
			    rgbled1_status=COLOR_GREEN;
				break;
			case COLOR_BLUE:
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_0_PIN, 0);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_1_PIN, 0);
				XGpioPs_WritePin(&gpio_mio, MIO_RGB_2_PIN, 1);
			    rgbled1_status=COLOR_BLUE;
				break;
			default:
				xil_printf("control_rgb_leds: unknown color\r\n");
				return XST_FAILURE;
		}
	}
	else
	{
		xil_printf("control_rgb_leds: unknown RGB led\r\n");
		return XST_FAILURE;
	}

    return XST_SUCCESS;
}


unsigned int get_switch_state()
{
	return XGpioPs_ReadPin(&gpio_mio, MIO_SW_3_PIN) << 3 |
			XGpioPs_ReadPin(&gpio_mio, MIO_SW_2_PIN) << 2 |
			XGpioPs_ReadPin(&gpio_mio, MIO_SW_1_PIN) << 1 |
			XGpioPs_ReadPin(&gpio_mio, MIO_SW_0_PIN);
}
