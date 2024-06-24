#ifndef __PLATFORM_GPIO_H_
#define __PLATFORM_GPIO_H_

typedef enum color {
	COLOR_NONE,
	COLOR_RED,
	COLOR_GREEN,
	COLOR_BLUE
} color_t;

int platform_init_gpios();
int control_rgb_leds(int led_index, color_t color);
unsigned int get_switch_state();

#endif
