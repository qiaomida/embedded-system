#include "key.h"
Key_TypeDef key_up;
Key_TypeDef key_down;
void key_scan( void)
{
    if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET)
    {
        key_up.state = 1;
        key_up.press_counter++;
    }
    else
    {
        key_up.state = 0;
        key_up.press_counter = 0;
    }
    if(HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_1) == GPIO_PIN_RESET)
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