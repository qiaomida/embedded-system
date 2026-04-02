#include "key.h"

/**
 * @brief 初始化按键结构体
 * @param key 按键结构体指针
 * @param GPIOx GPIO 端口
 * @param GPIO_Pin GPIO 引脚
 * @param activeLevel 按下时的有效电平 (通常为 GPIO_PIN_RESET)
 */
void Key_Init(Key_t* key, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState activeLevel) {
    key->GPIOx = GPIOx;
    key->GPIO_Pin = GPIO_Pin;
    key->ActiveLevel = activeLevel;
    
    key->State = KEY_STATE_IDLE;
    key->Timer = 0;
    
    key->LongPressTicks = 100;   /* 1000ms (假设 10ms 扫描一次) */
    key->DoubleClickTicks = 30; /* 300ms */
    key->DebounceTicks = 2;    /* 20ms */
}

/**
 * @brief 按键扫描函数 (建议 10ms 周期性调用)
 * @param key 按键结构体指针
 * @return Key_Event_t 返回触发的事件
 */
Key_Event_t Key_Scan(Key_t* key) {
    GPIO_PinState currentLevel = HAL_GPIO_ReadPin(key->GPIOx, key->GPIO_Pin);
    uint8_t isPressed = (currentLevel == key->ActiveLevel);
    Key_Event_t result = KEY_EVENT_NONE;

    switch (key->State) {
        case KEY_STATE_IDLE:
            if (isPressed) {
                key->State = KEY_STATE_DEBOUNCE;
                key->Timer = 0;
            }
            break;

        case KEY_STATE_DEBOUNCE:
            if (isPressed) {
                key->Timer++;
                if (key->Timer >= key->DebounceTicks) {
                    key->State = KEY_STATE_WAIT_RELEASE;
                    key->Timer = 0;
                }
            } else {
                key->State = KEY_STATE_IDLE;
            }
            break;

        case KEY_STATE_WAIT_RELEASE:
            if (isPressed) {
                key->Timer++;
                if (key->Timer >= key->LongPressTicks) {
                    result = KEY_EVENT_LONG_PRESS;
                    key->State = KEY_STATE_WAIT_LONG_RELEASE;
                    key->Timer = 0;
                }
            } else {
                /* 松开了 */
                key->State = KEY_STATE_WAIT_DOUBLE;
                key->Timer = 0;
            }
            break;

        case KEY_STATE_WAIT_DOUBLE:
            key->Timer++;
            if (isPressed) {
                /* 第二次按下，触发双击 */
                result = KEY_EVENT_DOUBLE_CLICK;
                key->State = KEY_STATE_WAIT_LONG_RELEASE; // 复用长按释放状态，防止双击还没松手
                key->Timer = 0;
            } else if (key->Timer >= key->DoubleClickTicks) {
                /* 超时未有第二次按下，触发单击 */
                result = KEY_EVENT_SINGLE_CLICK;
                key->State = KEY_STATE_IDLE;
            }
            break;

        case KEY_STATE_WAIT_LONG_RELEASE:
            if (!isPressed) {
                key->State = KEY_STATE_IDLE;
            }
            break;

        default:
            key->State = KEY_STATE_IDLE;
            break;
    }

    return result;
}
