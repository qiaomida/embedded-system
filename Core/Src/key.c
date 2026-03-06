#include "key.h"
void key_scan( void)
{
    if(hal_gpio_read_pin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
    {
        key_up.state = 1;
        key_up.press_counter++;
    }
    else
    {
        key_up.state = 0;
        key_up.press_counter = 0;
    }
    if(hal_gpio_read_pin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET)
    {
        key_down.state = 1;
        key_down.press_counter++;
    }
    else
    {
        key_down.state = 0;
        key_down.press_counter = 0;
    }
}