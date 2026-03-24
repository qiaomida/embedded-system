/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "control.h"
#include "params_store.h"
#include "pid.h"
#include "lv_port_lcd_stm32.h"
#include "lvgl.h"
#include "stdio.h"
#include "lv_port_indev.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
/* freertos.c */
extern ADC_HandleTypeDef hadc1;
extern PID_TypeDef MyPID;
extern float core_temp;
static lv_obj_t * label_value;
static int32_t encoder_count = 0;
/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LVGL_Task */
osThreadId_t LVGL_TaskHandle;
const osThreadAttr_t LVGL_Task_attributes = {
  .name = "LVGL_Task",
  .stack_size = 2048 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for myTask03 */
osThreadId_t myTask03Handle;
const osThreadAttr_t myTask03_attributes = {
  .name = "myTask03",
  .stack_size = 1024 * 4,
  .priority = (osPriority_t) osPriorityLow,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */
// 定义页面函数指针类型
typedef void (*menu_page_func_t)(lv_obj_t * parent);
// 页面函数声明
void create_main_menu(lv_obj_t * parent);
void create_settings_menu(lv_obj_t * parent);
void create_info_page(lv_obj_t * parent);
// 核心跳转函数
void switch_page(menu_page_func_t page_func);
// 当前页面和容器
static lv_obj_t * current_screen = NULL;
// 事件回调函数声明 (纯C必须独立定义)
static void event_to_settings_cb(lv_event_t * e);
static void event_to_info_cb(lv_event_t * e);
static void event_to_main_cb(lv_event_t * e);


/**
 * 页面切换核心函数
 * @param page_func 下一个页面的构造函数
 */
void switch_page(menu_page_func_t page_func) {
    // 1. 清理当前屏幕内容
    if (current_screen != NULL) {
        lv_obj_clean(lv_scr_act()); // 清除当前活动屏幕的所有子对象
    }
    
    // 2. 重新设置当前容器（直接用活动屏幕）
    current_screen = lv_scr_act();
    
    // 3. 执行页面构造
    if (page_func) {
        page_func(current_screen);
    }
}

// --- 页面 1: 主菜单 ---
void create_main_menu(lv_obj_t * parent) {
    lv_obj_t * list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_center(list);

    lv_list_add_text(list, "Main Menu");
    
    // 设置按钮
    lv_obj_t * btn_set = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Settings");
    lv_obj_add_event_cb(btn_set, event_to_settings_cb, LV_EVENT_CLICKED, NULL);


    // 信息按钮
    lv_obj_t * btn_info = lv_list_add_btn(list, LV_SYMBOL_FILE, "System Info");
    lv_obj_add_event_cb(btn_info, event_to_info_cb, LV_EVENT_CLICKED, NULL);
    //加入group
        lv_group_t * g = lv_group_get_default();
    if(g) {
        lv_group_add_obj(g, btn_set);
        lv_group_add_obj(g, btn_info);
    }
}

// --- 页面 2: 设置菜单 ---
void create_settings_menu(lv_obj_t * parent) {
    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, "Settings Page");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    // 亮度调节滑块
    lv_obj_t * slider = lv_slider_create(parent);
    lv_obj_set_width(slider, lv_pct(80));
    lv_obj_center(slider);

    // 返回按钮
    lv_obj_t * btn_back = lv_btn_create(parent);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t * label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
        lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    lv_group_t * g = lv_group_get_default();
    if(g) {
        lv_group_add_obj(g, slider);
        lv_group_add_obj(g, btn_back);
    }

}

// --- 页面 3: 信息页面 ---
void create_info_page(lv_obj_t * parent) {
    lv_obj_t * label = lv_label_create(parent);
    lv_label_set_text(label, "Version: v1.0\nCPU: STM32F4\nOS: FreeRTOS");
    lv_obj_center(label);

    // 同样添加返回逻辑...
    lv_obj_t * btn_back = lv_btn_create(parent);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t * label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");                                                     
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

}

/* --- 事件回调函数集 --- */
static void event_to_settings_cb(lv_event_t * e) {
    switch_page(create_settings_menu);
}

static void event_to_info_cb(lv_event_t * e) {
    switch_page(create_info_page);
}

static void event_to_main_cb(lv_event_t * e) {
    switch_page(create_main_menu);
}  
/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);
void StartLVGLTask(void *argument);
void StartTask03(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LVGL_Task */
  LVGL_TaskHandle = osThreadNew(StartLVGLTask, NULL, &LVGL_Task_attributes);

  /* creation of myTask03 */
  myTask03Handle = osThreadNew(StartTask03, NULL, &myTask03_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartLVGLTask */
/**
* @brief Function implementing the LVGL_Task thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartLVGLTask */
void StartLVGLTask(void *argument)
{
  /* USER CODE BEGIN StartLVGLTask */
    
  printf("LVGL Task Starting...\r\n");
  lv_port_disp_init();
  lv_port_indev_init();
   printf("LVGL Porting Done!\r\n");
  switch_page(create_main_menu); // 显示主菜单
  /* Infinite loop */  
  for(;;)                                                                                                                              
  {
    lv_timer_handler();
    osDelay(5);
  }
  /* USER CODE END StartLVGLTask */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the myTask03 thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void StartTask03(void *argument)
{
  /* USER CODE BEGIN StartTask03 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END StartTask03 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

