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
#include "stdlib.h"
#include "control.h"
#include "params_store.h"
#include "pid.h"
#include "lv_port_lcd_stm32.h"
#include "lvgl.h"
#include "stdio.h"
#include "lv_port_indev.h"
#include "string.h"
#include "timers.h"
#include "buzzer.h"
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
extern float vdd_v; 
static lv_obj_t * label_value;
static int32_t encoder_count = 0;
// 外部引用的 CubeMX 生成的 RTC 句柄
extern RTC_HandleTypeDef hrtc;
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
void create_pid_control_page(lv_obj_t * parent);
// 核心跳转函数
void switch_page(menu_page_func_t page_func);
// 当前页面和容器
static lv_obj_t * current_screen = NULL;


/* --- UI 控件指针 --- */
static lv_obj_t * date_label;
static lv_obj_t * time_label;
static lv_obj_t * label_temp;
static lv_obj_t * label_vdd;
static lv_obj_t * chart_temp;
static lv_chart_series_t * ser_temp;
static lv_obj_t * roller_kp, * roller_ki, * roller_kd, * roller_target;
static lv_obj_t * led_heater; // 加热状态指示灯

// 事件回调函数声明 (纯C必须独立定义)
static void event_to_settings_cb(lv_event_t * e);
static void event_to_info_cb(lv_event_t * e);
static void event_to_main_cb(lv_event_t * e);
static void event_to_pid_cb(lv_event_t * e);
static void event_to_clock_cb(lv_event_t * e);
//更新函数声明
static void update_temp_task(lv_timer_t * timer);

/**
 * 页面切换核心函数
 * @param page_func 下一个页面的构造函数
 */
void switch_page(menu_page_func_t page_func) {
    // 1. 获取默认组
    lv_group_t * g = lv_group_get_default();
    if(g == NULL) {
        g = lv_group_create();
        lv_group_set_default(g);
    }

    // 2. 清理当前屏幕内容并清空组
    lv_obj_clean(lv_scr_act()); 
    lv_group_remove_all_objs(g);
    
    // 重置所有 UI 控件指针，防止定时器误操作已删除的对象
    label_temp = NULL;
    label_vdd = NULL;
    chart_temp = NULL;
    ser_temp = NULL;
    roller_target = NULL;
    roller_kp = NULL;
    roller_ki = NULL;
    roller_kd = NULL;
    led_heater = NULL;
    time_label = NULL;
    date_label = NULL;

    // 3. 执行页面构造
    if (page_func) {
        page_func(lv_scr_act());
    }
    
    printf("Page Switched: Screen cleaned and group cleared\r\n");
}

// --- 页面 1: 主菜单 ---
void create_main_menu(lv_obj_t * parent) {
    lv_obj_t * list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_center(list);

    lv_list_add_text(list, "Main Menu");
    //时钟按钮
    lv_obj_t * btn_clock = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Clock");
    lv_obj_add_event_cb(btn_clock, event_to_clock_cb, LV_EVENT_CLICKED, NULL);
    
    // 设置按钮
    lv_obj_t * btn_set = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Settings");
    lv_obj_add_event_cb(btn_set, event_to_settings_cb, LV_EVENT_CLICKED, NULL);


    // 信息按钮
    lv_obj_t * btn_info = lv_list_add_btn(list, LV_SYMBOL_FILE, "System Info");
    lv_obj_add_event_cb(btn_info, event_to_info_cb, LV_EVENT_CLICKED, NULL);

    //pid模块按钮
    lv_obj_t * btn_pid = lv_list_add_btn(list, LV_SYMBOL_EDIT, "PID Control");
    lv_obj_add_event_cb(btn_pid, event_to_pid_cb, LV_EVENT_CLICKED, NULL);
    //加入group
        lv_group_t * g = lv_group_get_default();
    if(g) {
        lv_group_add_obj(g, btn_set);
        lv_group_add_obj(g, btn_info);
        lv_group_add_obj(g, btn_pid);
        lv_group_add_obj(g, btn_clock);
        lv_group_focus_obj(btn_clock);
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
    lv_group_t * g = lv_group_get_default();

    /* 1. 顶部标题 */
    lv_obj_t * label_title = lv_label_create(parent);
    lv_label_set_text(label_title, "Real-time Monitoring");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);

    /* 2. 电压和温度实时显示 */
    label_temp = lv_label_create(parent);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, 0, 35);
    
    label_vdd = lv_label_create(parent);
    lv_obj_align(label_vdd, LV_ALIGN_TOP_MID, 0, 55);

    /* 3. 温度趋势图 */
    chart_temp = lv_chart_create(parent);
    lv_obj_set_size(chart_temp, 200, 120);
    lv_obj_align(chart_temp, LV_ALIGN_CENTER, 0, 20); 
    lv_chart_set_type(chart_temp, LV_CHART_TYPE_LINE);
    
    // 优化显示范围：根据室温(约25度)和目标温度(假设最高50度)动态调整，这里先设为 20-50 度，使波动更明显
    lv_chart_set_range(chart_temp, LV_CHART_AXIS_PRIMARY_Y,
                   core_temp - 5, core_temp + 5); 
    lv_chart_set_point_count(chart_temp, 50); // 增加点数，让曲线更平滑

    /* 添加网格线 */
    lv_obj_set_style_line_color(chart_temp, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_temp, 6, 5); // 调整网格线数量以匹配 20-50 的范围 (每5度一格)

    /* 添加数据系列 */
    ser_temp = lv_chart_add_series(chart_temp, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    /* 4. 返回按钮 */
    lv_obj_t * btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t * label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn_back);

    /* 5. 开启定时刷新任务 (如果尚未开启) */
    static lv_timer_t * info_timer = NULL;
    if(info_timer == NULL) {
        info_timer = lv_timer_create(update_temp_task, 200, NULL);
    }
}
/**
 * 从硬件 RTC 获取时间并更新 LVGL 标签
 */
void update_clock_from_rtc(lv_obj_t *time_label, lv_obj_t *date_label) {
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // 1. 获取时间 (必须先读时间)
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    // 2. 获取日期 (必须读日期，否则时间寄存器不刷新)
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // 3. 更新 UI
    if (time_label) {
        lv_label_set_text_fmt(time_label, "%02d:%02d:%02d", 
                              sTime.Hours, sTime.Minutes, sTime.Seconds);
    }
    if (date_label) {
        lv_label_set_text_fmt(date_label, "20%02d-%02d-%02d", 
                              sDate.Year, sDate.Month, sDate.Date);
    }
}

/* --- 页面逻辑 --- */

// 实时获取 ADC 数据并更新 UI
static void update_temp_task(lv_timer_t * timer) {
    LV_UNUSED(timer);
    
    // 更新标签
    if(label_temp) {
        lv_label_set_text_fmt(label_temp, "Core Temp: %.1f °C", core_temp);
    }
    
    if(label_vdd) {
        lv_label_set_text_fmt(label_vdd, "Vdd: %.2f V", vdd_v);
    }

    // 更新图表 (仅在图表存在时更新)
    if(chart_temp && ser_temp) {
        lv_chart_set_next_value(chart_temp, ser_temp, (int)core_temp);
    }
    
    // 更新加热指示灯状态
    if(led_heater) {
        if(MyPID.Output > 0.5f) {
            /* 保证低输出时也有明显可见度 */
            uint8_t brightness = (uint8_t)(120.0f + (MyPID.Output * 135.0f / 1000.0f));
            if(brightness > 255U) {
                brightness = 255U;
            }
            lv_led_on(led_heater);
            lv_led_set_brightness(led_heater, brightness);
        } else {
            lv_led_off(led_heater);
            lv_led_set_brightness(led_heater, 0);
        }
        
        // 调试打印输出和亮度关系
        static uint8_t dbg_div = 0;
        if(++dbg_div > 5) { // 约每秒打印一次
            printf("UI Update -> Output: %.0f, Brightness: %d\r\n", MyPID.Output, lv_led_get_brightness(led_heater));
            dbg_div = 0;
        }
    }
}
//更新时钟任务
    void update_clock_task(lv_timer_t * timer) {
    LV_UNUSED(timer);
    update_clock_from_rtc(time_label, date_label);
    }
// 滚轮数值改变回调
static void value_changed_event_cb(lv_event_t * e) {
    lv_obj_t * obj = lv_event_get_target(e);
    uint16_t sel = lv_roller_get_selected(obj);
    
    if(obj == roller_target) {
        // 目标温度范围 20-100度
        MyPID.Target = 20.0f + (float)sel;
        printf("Target Temp Updated: %.1f\n", MyPID.Target);
    } else if(obj == roller_kp) {
        MyPID.Kp = (float)sel * 5.0f; // Kp: 0 - 200, 步长 5
        printf("Kp Updated: %.1f\n", MyPID.Kp);
    } else if(obj == roller_ki) {
        MyPID.Ki = (float)sel / 10.0f; // Ki: 0.0 - 10.0
        printf("Ki Updated: %.2f\n", MyPID.Ki);
    } else if(obj == roller_kd) {
        MyPID.Kd = (float)sel * 5.0f; // Kd: 0 - 100, 步长 5
        printf("Kd Updated: %.1f\n", MyPID.Kd);
    }
}

//pid页面
void create_pid_control_page(lv_obj_t * parent) {
    lv_group_t * g = lv_group_get_default();

    /* 1. 顶部温度显示和加热指示灯 */
    label_temp = lv_label_create(parent);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, -20, 10);

    led_heater = lv_led_create(parent);
    lv_obj_set_size(led_heater, 18, 18); // 稍微加大
    lv_obj_align_to(led_heater, label_temp, LV_ALIGN_OUT_RIGHT_MID, 40, 0); // 间距调近
    lv_led_set_color(led_heater, lv_color_hex(0xFF0000)); // 显式设置纯红
    lv_led_on(led_heater); // 初始设为ON状态，由亮度控制视觉效果
    lv_led_set_brightness(led_heater, 255);

    /* 2. 参数调节区 (使用容器横向排列) */
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 230, 140);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // 生成滚轮选项 (使用静态分配或动态分配，避免栈溢出)
    // 目标温度: 20-100 (81个选项，每个约3-4字节，总计约300字节)
    static char opts_target[400];
    if(opts_target[0] == '\0') {
        char *p = opts_target;
        for(int i=20; i<=100; i++) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_target) {
            *(p-1) = '\0'; // Remove last newline
        }
    }

    // Kp: 0-200 步长5 (41个选项，总计约150字节)
    static char opts_kp[200];
    if(opts_kp[0] == '\0') {
        char *p = opts_kp;
        for(int i=0; i<=200; i+=5) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_kp) *(p-1) = '\0';
    }

    // Ki: 0.0-10.0 步长0.1 (101个选项，总计约400字节)
    static char opts_ki[500];
    if(opts_ki[0] == '\0') {
        char *p = opts_ki;
        for(int i=0; i<=100; i++) {
            p += sprintf(p, "%.1f\n", (float)i/10.0f);
        }
        if (p > opts_ki) *(p-1) = '\0';
    }

    // Kd: 0-100 步长5 (21个选项，总计约80字节)
    static char opts_kd[150];
    if(opts_kd[0] == '\0') {
        char *p = opts_kd;
        for(int i=0; i<=100; i+=5) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_kd) *(p-1) = '\0';
    }

    // 创建四个滚轮 Target, Kp, Ki, Kd
    roller_target = lv_roller_create(cont);
    lv_roller_set_options(roller_target, opts_target, LV_ROLLER_MODE_NORMAL);
    // 确保初始值在范围内
    uint16_t target_idx = (MyPID.Target >= 20.0f && MyPID.Target <= 100.0f) ? (uint16_t)(MyPID.Target - 20.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_target, target_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_target, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_target);

    roller_kp = lv_roller_create(cont);
    lv_roller_set_options(roller_kp, opts_kp, LV_ROLLER_MODE_NORMAL);
    uint16_t kp_idx = (MyPID.Kp >= 0 && MyPID.Kp <= 200.0f) ? (uint16_t)(MyPID.Kp / 5.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_kp, kp_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_kp, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_kp);

    roller_ki = lv_roller_create(cont);
    lv_roller_set_options(roller_ki, opts_ki, LV_ROLLER_MODE_NORMAL);
    uint16_t ki_idx = (MyPID.Ki >= 0 && MyPID.Ki <= 10.0f) ? (uint16_t)(MyPID.Ki * 10.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_ki, ki_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_ki, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_ki);

    roller_kd = lv_roller_create(cont);
    lv_roller_set_options(roller_kd, opts_kd, LV_ROLLER_MODE_NORMAL);
    uint16_t kd_idx = (MyPID.Kd >= 0 && MyPID.Kd <= 100.0f) ? (uint16_t)(MyPID.Kd / 5.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_kd, kd_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_kd, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_kd);

    /* 3. 底部目标值提示 */
    lv_obj_t * info = lv_label_create(parent);
    lv_label_set_text(info, "Target   Kp   Ki   Kd");
    lv_obj_align_to(info, cont, LV_ALIGN_OUT_TOP_MID, 0, -5);

    /* 4. 返回按钮 (可选) */
    lv_obj_t * btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn_back);

    /* 5. 开启定时刷新任务 */
    static lv_timer_t * pid_timer = NULL;
    if(pid_timer == NULL) {
        pid_timer = lv_timer_create(update_temp_task, 200, NULL);
    }
    
    // 默认聚焦第一个滚轮
    lv_group_focus_obj(roller_target);
    printf("PID Page: Objects added to default group and focused\r\n");
}

void create_clock_page(lv_obj_t *parent)
{
    lv_group_t * g = lv_group_get_default();
    // 时间显示
    time_label = lv_label_create(parent);
    lv_label_set_text(time_label, "--:--:--");
    //时间显示
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);
    //日期显示
    date_label = lv_label_create(parent);
    lv_label_set_text_fmt(date_label, "Date: --/--/----");
    lv_obj_align_to(date_label,time_label,LV_ALIGN_OUT_BOTTOM_MID,0,10);
    //返回按钮
    lv_obj_t * btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    //加入编码器
    if(g) {
        lv_group_add_obj(g, btn_back);
    }
    static lv_timer_t * clock_timer = NULL;
    if(clock_timer == NULL) {
        clock_timer = lv_timer_create(update_clock_task, 1000, NULL);
    }
    update_clock_from_rtc(time_label, date_label);
}


/* --- 事件回调函数集 --- */
static void event_to_clock_cb(lv_event_t * e) {
    switch_page(create_clock_page);
    Buzzer_Beep(100);
}

static void event_to_settings_cb(lv_event_t * e) {
    switch_page(create_settings_menu);
    Buzzer_Beep(100);
}

static void event_to_info_cb(lv_event_t * e) {
    switch_page(create_info_page);
    Buzzer_Beep(100);
}

static void event_to_main_cb(lv_event_t * e) {
    switch_page(create_main_menu);
    Buzzer_Beep(100); // 返回主菜单时短促蜂鸣提示
}  
static void event_to_pid_cb(lv_event_t * e) {
    switch_page(create_pid_control_page);
    Buzzer_Beep(100);
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
  Buzzer_Init();
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
  printf("PID Output: %.2f\r\n", MyPID.Output);

  switch_page(create_main_menu); // 显示主菜单
  Buzzer_Beep(200); // 启动时短促蜂鸣提示
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
    Control_Loop();      // 执行 PID 计算和数据更新
    lv_port_indev_scan(); // 扫描按键事件 (10ms)
    osDelay(10);         // 10ms 周期
  }
  /* USER CODE END StartTask03 */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

