#ifndef __PLATFORM_GPIO_H_
#define __PLATFORM_GPIO_H_

void platform_init_gpios();
int toggle_leds();
unsigned int get_switch_state();

#endif
