#include "buzzer.h"
#include "gpio.h"
#include "main.h"
#include "FreeRTOS.h"
#include "timers.h"

/* 软件定时器句柄 */
static TimerHandle_t buzzerTimer;
/* 定时器回调函数（到时间关闭蜂鸣器） */
static void Buzzer_TimerCallback(TimerHandle_t xTimer)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}
void Buzzer_On(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);

}

void Buzzer_Off(void)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
}
/* 初始化 */
void Buzzer_Init(void)
{
    /* 创建软件定时器（单次触发） */
    buzzerTimer = xTimerCreate(
        "buzzerTimer",
        pdMS_TO_TICKS(100),   // 默认值（无所谓）
        pdFALSE,              // 单次定时器
        NULL,
        Buzzer_TimerCallback
    );

    Buzzer_Off();
}
/* 蜂鸣指定时间（非阻塞） */
void Buzzer_Beep(uint16_t time_ms)
{
    /* 开蜂鸣器 */
    Buzzer_On();

    /* 修改定时器时间 */
    xTimerChangePeriod(buzzerTimer, pdMS_TO_TICKS(time_ms), 0);

    /* 启动（或重启）定时器 */
    xTimerStart(buzzerTimer, 0);
}