#include "app_buzzer.h"
#include "gpio.h"
#include "main.h"
#include "FreeRTOS.h"
#include "timers.h"

#ifndef BUZZER_ACTIVE_LEVEL
#define BUZZER_ACTIVE_LEVEL GPIO_PIN_SET
#endif
#if (BUZZER_ACTIVE_LEVEL == GPIO_PIN_SET)
#define BUZZER_INACTIVE_LEVEL GPIO_PIN_RESET
#else
#define BUZZER_INACTIVE_LEVEL GPIO_PIN_SET
#endif

#ifndef BUZZER_PASSIVE
#define BUZZER_PASSIVE 0
#endif

#ifndef BUZZER_TOGGLE_PERIOD_MS
#define BUZZER_TOGGLE_PERIOD_MS 1U
#endif

static TimerHandle_t buzzerStopTimer;
static TimerHandle_t buzzerToggleTimer;
static uint8_t buzzer_level = 0;

static void Buzzer_Write(GPIO_PinState state)
{
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, state);
    buzzer_level = (state == BUZZER_ACTIVE_LEVEL) ? 1U : 0U;
}

static void Buzzer_StopTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;

    if (buzzerToggleTimer != NULL) {
        xTimerStop(buzzerToggleTimer, 0);
    }
    Buzzer_Write(BUZZER_INACTIVE_LEVEL);
}

static void Buzzer_ToggleTimerCallback(TimerHandle_t xTimer)
{
    (void)xTimer;
    Buzzer_Write(buzzer_level ? BUZZER_INACTIVE_LEVEL : BUZZER_ACTIVE_LEVEL);
}

void Buzzer_On(void)
{
    Buzzer_Write(BUZZER_ACTIVE_LEVEL);
}

void Buzzer_Off(void)
{
    if (buzzerToggleTimer != NULL) {
        xTimerStop(buzzerToggleTimer, 0);
    }
    if (buzzerStopTimer != NULL) {
        xTimerStop(buzzerStopTimer, 0);
    }
    Buzzer_Write(BUZZER_INACTIVE_LEVEL);
}

void Buzzer_Init(void)
{
    buzzerStopTimer = xTimerCreate(
        "buzzerStop",
        pdMS_TO_TICKS(100),
        pdFALSE,
        NULL,
        Buzzer_StopTimerCallback
    );

    buzzerToggleTimer = xTimerCreate(
        "buzzerToggle",
        pdMS_TO_TICKS(BUZZER_TOGGLE_PERIOD_MS),
        pdTRUE,
        NULL,
        Buzzer_ToggleTimerCallback
    );

    Buzzer_Off();
}

void Buzzer_Beep(uint16_t time_ms)
{
    if (buzzerStopTimer == NULL || time_ms == 0U) {
        return;
    }

    Buzzer_Write(BUZZER_ACTIVE_LEVEL);
#if BUZZER_PASSIVE
    if (buzzerToggleTimer != NULL) {
        xTimerChangePeriod(buzzerToggleTimer, pdMS_TO_TICKS(BUZZER_TOGGLE_PERIOD_MS), 0);
        xTimerStart(buzzerToggleTimer, 0);
    }
#endif
    xTimerChangePeriod(buzzerStopTimer, pdMS_TO_TICKS(time_ms), 0);
    xTimerStart(buzzerStopTimer, 0);
}
