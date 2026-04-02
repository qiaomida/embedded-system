#ifndef __KEY_H
#define __KEY_H

#include "main.h"

/* 按键事件枚举 */
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_SINGLE_CLICK,
    KEY_EVENT_DOUBLE_CLICK,
    KEY_EVENT_LONG_PRESS
} Key_Event_t;

/* 按键状态机内部状态 */
typedef enum {
    KEY_STATE_IDLE = 0,
    KEY_STATE_DEBOUNCE,
    KEY_STATE_WAIT_RELEASE,
    KEY_STATE_WAIT_DOUBLE,
    KEY_STATE_WAIT_LONG_RELEASE, // 等待长按后的释放
} Key_InternalState_t;

/* 按键结构体 */
typedef struct {
    GPIO_TypeDef* GPIOx;        /* GPIO 端口 */
    uint16_t GPIO_Pin;          /* GPIO 引脚 */
    GPIO_PinState ActiveLevel;  /* 有效电平 (通常为 GPIO_PIN_RESET) */
    
    Key_InternalState_t State;  /* 内部状态 */
    uint32_t Timer;             /* 计时器 */
    
    uint16_t LongPressTicks;    /* 长按判定的滴答数 (如 10ms 一次，100 = 1000ms) */
    uint16_t DoubleClickTicks;  /* 双击判定的滴答数 (如 10ms 一次，30 = 300ms) */
    uint16_t DebounceTicks;     /* 消抖滴答数 (通常为 2 Ticks = 20ms) */
} Key_t;

/* 函数声明 */
void Key_Init(Key_t* key, GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState activeLevel);
Key_Event_t Key_Scan(Key_t* key);

#endif /* __KEY_H */
