#include "app_tasks.h"
#include "app_ui.h"
#include "app_control.h"
#include "app_buzzer.h"
#include "cmsis_os.h"
#include "lv_port_lcd_stm32.h"
#include "lv_port_indev.h"
#include "lvgl.h"
#include "pid.h"
#include "stdio.h"

extern PID_TypeDef MyPID;

static osThreadId_t defaultTaskHandle;
static const osThreadAttr_t defaultTask_attributes = {
    .name = "defaultTask",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

static osThreadId_t LVGL_TaskHandle;
static const osThreadAttr_t LVGL_Task_attributes = {
    .name = "LVGL_Task",
    .stack_size = 2048 * 4,
    .priority = (osPriority_t)osPriorityNormal,
};

static osThreadId_t myTask03Handle;
static const osThreadAttr_t myTask03_attributes = {
    .name = "myTask03",
    .stack_size = 1024 * 4,
    .priority = (osPriority_t)osPriorityLow,
};

void AppTasks_Init(void)
{
    Buzzer_Init();

    defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);
    LVGL_TaskHandle = osThreadNew(StartLVGLTask, NULL, &LVGL_Task_attributes);
    myTask03Handle = osThreadNew(StartTask03, NULL, &myTask03_attributes);
}

void StartDefaultTask(void *argument)
{
    (void)argument;

    for (;;) {
        osDelay(1);
    }
}

void StartLVGLTask(void *argument)
{
    (void)argument;

    printf("LVGL Task Starting...\r\n");
    lv_port_disp_init();
    lv_port_indev_init();
    printf("LVGL Porting Done!\r\n");
    printf("PID Output: %.2f\r\n", MyPID.Output);

    switch_page(create_main_menu);
    Buzzer_Beep(200);

    for (;;) {
        lv_timer_handler();
        osDelay(5);
    }
}

void StartTask03(void *argument)
{
    (void)argument;

    for (;;) {
        Control_Loop();
        lv_port_indev_scan();
        osDelay(10);
    }
}
