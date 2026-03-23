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
/**
 * @brief 更新 UI 上的计数器数值
 */
void update_encoder_ui(int32_t value) {
    if(label_value) {
        lv_label_set_text_fmt(label_value, "Count: %d", (int)value);
    }
}

/**
 * @brief 编码器 UI 初始化
 */
void setup_encoder_ui(void) {
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0x000000), 0); // 黑色背景

    /* 创建标题 */
    lv_obj_t * title = lv_label_create(scr);
    lv_label_set_text(title, "ENCODER TEST");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    /* 创建数值显示标签 */
    label_value = lv_label_create(scr);
    lv_label_set_text(label_value, "Count: 0");
    lv_obj_set_style_text_font(label_value, &lv_font_montserrat_14, 0); // 假设启用了此字体
    lv_obj_set_style_text_color(label_value, lv_color_hex(0x00FF00), 0); // 绿色数值
    lv_obj_center(label_value);
}

/**
 * @brief 在 StartLVGLTask 的循环中调用
 */
void process_encoder(void) {
    /* 读取 TIM4 计数器 (假设为编码器模式) */
    int16_t current_val = (int16_t)__HAL_TIM_GET_COUNTER(&htim4);
    
    /* 简单的增量逻辑，也可以根据需要清零计数器 */
    if(current_val != 0) {
        encoder_count += current_val;
        __HAL_TIM_SET_COUNTER(&htim4, 0); // 复位
        update_encoder_ui(encoder_count);
    }
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
//   printf("LVGL Porting Done!\r\n");
  HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL); // 启动编码器接口
  setup_encoder_ui(); // 初始化编码器 UI
  /* Infinite loop */
  for(;;)
  {
    lv_timer_handler();
    process_encoder(); // 处理编码器输入
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

