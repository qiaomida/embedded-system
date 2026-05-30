/**
 * @file app_ui.c
 * @brief 温控面板UI应用层代码
 * @details 基于LVGL图形库实现温度控制系统的用户界面，包括主菜单、时钟、番茄钟、
 *          系统信息、PID控制和设置页面等功能模块
 */

#include "app_ui.h"
#include <stdbool.h>
#include "app_buzzer.h"
#include "app_control.h"
#include "app_sensor.h"
#include "pid.h"
#include "main.h"
#include "rtc.h"
#include "stdio.h"
#include "string.h"

/* 外部变量声明 */
extern PID_TypeDef MyPID;          /**< PID控制器实例 */
extern RTC_HandleTypeDef hrtc;     /**< RTC实时时钟句柄 */

/* UI控件全局变量（静态，仅限本文件使用） */
static lv_obj_t *date_label;                  /**< 日期显示标签 */
static lv_obj_t *time_label;                  /**< 时间显示标签 */
static lv_obj_t *label_temp;                  /**< 温度显示标签 */
static lv_obj_t *label_vdd;                   /**< 电源电压显示标签 */
static lv_obj_t *chart_temp;                  /**< 温度趋势图表 */
static lv_chart_series_t *ser_temp;           /**< 温度数据序列 */
static lv_obj_t *roller_kp, *roller_ki, *roller_kd, *roller_target;  /**< PID参数滚动选择器 */
static lv_obj_t *led_heater;                  /**< 加热器状态LED */
static lv_obj_t *pomodoro_arc;                /**< 番茄钟进度圆弧 */
static lv_obj_t *pomodoro_time_label;         /**< 番茄钟时间显示 */
static lv_obj_t *pomodoro_min_label;          /**< 番茄钟时长标签 */
static lv_obj_t *pomodoro_slider;             /**< 番茄钟时长滑块 */
static lv_obj_t *pomodoro_btn_start;          /**< 番茄钟开始/暂停按钮 */
static uint32_t pomodoro_total_sec = 25U * 60U;  /**< 番茄钟总时长（秒），默认25分钟 */
static uint32_t pomodoro_remain_sec = 25U * 60U; /**< 番茄钟剩余时间（秒） */
static bool pomodoro_running = false;         /**< 番茄钟运行状态标志 */
static lv_timer_t *pomodoro_timer = NULL;     /**< 番茄钟定时器 */
static lv_obj_t *time_setting_popup = NULL;   /**< 时间设置弹窗 */
static lv_obj_t *hour_slider = NULL;          /**< 小时设置滑块 */
static lv_obj_t *min_slider = NULL;           /**< 分钟设置滑块 */
static lv_obj_t *sec_slider = NULL;           /**< 秒设置滑块 */
static lv_obj_t *main_time_label = NULL;      /**< 主菜单时间显示标签 */
static lv_obj_t *main_date_label = NULL;      /**< 主菜单日期显示标签 */
static lv_timer_t *global_clock_timer = NULL;/**< 全局时钟更新定时器 */
/* 函数声明 */
static void event_to_settings_cb(lv_event_t *e);       /**< 切换到设置页面回调 */
static void event_to_info_cb(lv_event_t *e);           /**< 切换到系统信息页面回调 */
static void event_to_main_cb(lv_event_t *e);           /**< 切换到主菜单回调 */
static void event_to_pid_cb(lv_event_t *e);            /**< 切换到PID控制页面回调 */
static void event_to_clock_cb(lv_event_t *e);          /**< 切换到时钟页面回调 */
static void event_to_pomodoro_cb(lv_event_t *e);       /**< 切换到番茄钟页面回调 */
static void event_set_time_ok(lv_event_t *e);          /**< 时间设置确认回调 */
static void event_set_time_cancel(lv_event_t *e);      /**< 时间设置取消回调 */
static void time_slider_changed_cb(lv_event_t *e);     /**< 时间滑块值变化回调 */
static void event_open_time_setting(lv_event_t *e);    /**< 打开时间设置弹窗回调 */
static void value_changed_event_cb(lv_event_t *e);     /**< 滚动选择器值变化回调 */
static void update_temp_task(lv_timer_t *timer);       /**< 温度更新定时任务 */
static void pomodoro_tick_cb(lv_timer_t *timer);       /**< 番茄钟计时回调 */
static void pomodoro_duration_changed_cb(lv_event_t *e);/**< 番茄钟时长变更回调 */
static void pomodoro_start_pause_cb(lv_event_t *e);    /**< 番茄钟开始/暂停回调 */
static void pomodoro_reset_cb(lv_event_t *e);          /**< 番茄钟重置回调 */
static void update_clock_task(lv_timer_t *timer);      /**< 时钟更新定时任务 */
static void pomodoro_update_ui(void);                  /**< 更新番茄钟UI显示 */

/**
 * @brief 页面切换函数
 * @param page_func 目标页面创建函数指针
 * @details 清理当前屏幕，重置控件指针，调用指定的页面创建函数
 */
void switch_page(menu_page_func_t page_func)
{
    /* 获取默认控件组，如果不存在则创建 */
    lv_group_t *g = lv_group_get_default();
    if (g == NULL) {
        g = lv_group_create();
        lv_group_set_default(g);
    }

    /* 清空当前活动屏幕的所有控件和控件组 */
    lv_obj_clean(lv_scr_act());
    lv_group_remove_all_objs(g);

    /* 重置所有UI控件指针 */
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
    main_time_label = NULL;
    main_date_label = NULL;
    pomodoro_arc = NULL;
    pomodoro_time_label = NULL;
    pomodoro_min_label = NULL;
    pomodoro_slider = NULL;
    pomodoro_btn_start = NULL;
    pomodoro_running = false;

    /* 调用目标页面创建函数 */
    if (page_func) {
        page_func(lv_scr_act());
    }

    printf("Page Switched: Screen cleaned and group cleared\r\n");
}

/**
 * @brief 创建主菜单页面
 * @param parent 父容器对象
 * @details 包含时钟、番茄钟、设置、系统信息、PID控制等选项的主菜单
 */
void create_main_menu(lv_obj_t *parent)
{
    /* 创建主容器，使用垂直弹性布局 */
    lv_obj_t *main_cont = lv_obj_create(parent);
    lv_obj_set_size(main_cont, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_remove_style(main_cont, NULL, LV_PART_SCROLLBAR);

    /* 创建时间显示区域容器 */
    lv_obj_t *time_cont = lv_obj_create(main_cont);
    lv_obj_set_size(time_cont, lv_pct(100), 80);
    lv_obj_set_style_border_width(time_cont, 0, 0);
    lv_obj_set_style_bg_opa(time_cont, 0, 0);
    lv_obj_set_flex_flow(time_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(time_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 创建时间标签 */
    main_time_label = lv_label_create(time_cont);
    lv_label_set_text(main_time_label, "00:00:00");
    lv_obj_set_style_text_font(main_time_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(main_time_label, lv_palette_main(LV_PALETTE_BLUE), 0);

    /* 创建日期标签 */
    main_date_label = lv_label_create(time_cont);
    lv_label_set_text(main_date_label, "2000-01-01");
    lv_obj_set_style_text_font(main_date_label, &lv_font_montserrat_14, 0);

    /* 创建列表控件 */
    lv_obj_t *list = lv_list_create(main_cont);
    lv_obj_set_width(list, lv_pct(100));
    lv_obj_set_flex_grow(list, 1);
    /* 添加时钟按钮 */
    lv_obj_t *btn_clock = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Clock");
    lv_obj_add_event_cb(btn_clock, event_to_clock_cb, LV_EVENT_CLICKED, NULL);

    /* 添加番茄钟按钮 */
    lv_obj_t *btn_pomodoro = lv_list_add_btn(list, LV_SYMBOL_REFRESH, "Pomodoro");
    lv_obj_add_event_cb(btn_pomodoro, event_to_pomodoro_cb, LV_EVENT_CLICKED, NULL);

    /* 添加设置按钮 */
    lv_obj_t *btn_set = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Settings");
    lv_obj_add_event_cb(btn_set, event_to_settings_cb, LV_EVENT_CLICKED, NULL);

    /* 添加系统信息按钮 */
    lv_obj_t *btn_info = lv_list_add_btn(list, LV_SYMBOL_FILE, "System Info");
    lv_obj_add_event_cb(btn_info, event_to_info_cb, LV_EVENT_CLICKED, NULL);

    /* 添加PID控制按钮 */
    lv_obj_t *btn_pid = lv_list_add_btn(list, LV_SYMBOL_EDIT, "PID Control");
    lv_obj_add_event_cb(btn_pid, event_to_pid_cb, LV_EVENT_CLICKED, NULL);

    /* 将所有按钮添加到默认控件组 */
    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_add_obj(g, btn_set);
        lv_group_add_obj(g, btn_info);
        lv_group_add_obj(g, btn_pid);
        lv_group_add_obj(g, btn_clock);
        lv_group_add_obj(g, btn_pomodoro);
        lv_group_focus_obj(btn_clock);  /* 默认聚焦时钟按钮 */
    }

    /* 立即更新一次时钟显示 */
    update_clock_from_rtc(main_time_label, main_date_label);

    /* 创建全局时钟更新定时器（只初始化一次） */
    if (global_clock_timer == NULL) {
        global_clock_timer = lv_timer_create(update_clock_task, 1000, NULL);
    }
}

/**
 * @brief 创建设置页面
 * @param parent 父容器对象
 * @details 包含一个滑块控件和返回按钮的简单设置页面
 */
void create_settings_menu(lv_obj_t *parent)
{
    /* 创建标题标签 */
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Settings Page");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    /* 创建滑块控件 */
    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, lv_pct(80));
    lv_obj_center(slider);

    /* 创建返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    /* 添加到控件组 */
    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_add_obj(g, slider);
        lv_group_add_obj(g, btn_back);
    }
}

/**
 * @brief 创建系统信息页面
 * @param parent 父容器对象
 * @details 显示实时温度、电源电压和温度趋势图表
 */
void create_info_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    /* 创建标题标签 */
    lv_obj_t *label_title = lv_label_create(parent);
    lv_label_set_text(label_title, "Real-time Monitoring");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);

    /* 创建温度显示标签 */
    label_temp = lv_label_create(parent);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, 0, 35);

    /* 创建电压显示标签 */
    label_vdd = lv_label_create(parent);
    lv_obj_align(label_vdd, LV_ALIGN_TOP_MID, 0, 55);

    /* 创建温度趋势图表 */
    chart_temp = lv_chart_create(parent);
    lv_obj_set_size(chart_temp, 200, 120);
    lv_obj_align(chart_temp, LV_ALIGN_CENTER, 0, 20);
    lv_chart_set_type(chart_temp, LV_CHART_TYPE_LINE);  /* 设置为折线图 */
    lv_chart_set_range(chart_temp, LV_CHART_AXIS_PRIMARY_Y, core_temp - 5, core_temp + 5);  /* Y轴范围 */
    lv_chart_set_point_count(chart_temp, 50);  /* 数据点数量 */

    /* 设置图表样式 */
    lv_obj_set_style_line_color(chart_temp, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_temp, 6, 5);  /* 网格线数量 */

    /* 添加温度数据序列（红色） */
    ser_temp = lv_chart_add_series(chart_temp, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    /* 创建返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn_back);

    /* 创建温度更新定时器（200ms更新一次） */
    static lv_timer_t *info_timer = NULL;
    if (info_timer == NULL) {
        info_timer = lv_timer_create(update_temp_task, 200, NULL);
    }
}

/**
 * @brief 从RTC更新时钟显示
 * @param time_lbl 时间显示标签对象
 * @param date_lbl 日期显示标签对象
 * @details 从RTC实时时钟模块获取时间日期并更新UI显示
 */
void update_clock_from_rtc(lv_obj_t *time_lbl, lv_obj_t *date_lbl)
{
    RTC_TimeTypeDef sTime = {0};  /* RTC时间结构体 */
    RTC_DateTypeDef sDate = {0};  /* RTC日期结构体 */

    /* 从RTC获取时间和日期 */
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    /* 更新时间显示 */
    if (time_lbl) {
        lv_label_set_text_fmt(time_lbl, "%02d:%02d:%02d",
                              sTime.Hours, sTime.Minutes, sTime.Seconds);
    }
    /* 更新日期显示 */
    if (date_lbl) {
        lv_label_set_text_fmt(date_lbl, "20%02d-%02d-%02d",
                              sDate.Year, sDate.Month, sDate.Date);
    }
}

/**
 * @brief 温度更新定时任务
 * @param timer 定时器对象（未使用）
 * @details 定时更新温度显示、电压显示、温度图表和加热器状态LED
 */
static void update_temp_task(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    /* 更新温度显示 */
    if (label_temp) {
        lv_label_set_text_fmt(label_temp, "Core Temp: %.1f °C", core_temp);
    }

    /* 更新电压显示 */
    if (label_vdd) {
        lv_label_set_text_fmt(label_vdd, "Vdd: %.2f V", vdd_v);
    }

    /* 更新温度趋势图表 */
    if (chart_temp && ser_temp) {
        lv_chart_set_next_value(chart_temp, ser_temp, (int)core_temp);
    }

    /* 更新加热器状态LED */
    if (led_heater) {
        if (MyPID.Output > 0.5f) {
            /* PID输出大于0.5时点亮LED，亮度与输出成正比 */
            uint8_t brightness = (uint8_t)(120.0f + (MyPID.Output * 135.0f / 1000.0f));
            if (brightness > 255U) {
                brightness = 255U;
            }
            lv_led_on(led_heater);
            lv_led_set_brightness(led_heater, brightness);
        } else {
            /* PID输出小于等于0.5时熄灭LED */
            lv_led_off(led_heater);
            lv_led_set_brightness(led_heater, 0);
        }

        /* 调试输出（每5次更新打印一次） */
        static uint8_t dbg_div = 0;
        if (++dbg_div > 5) {
            printf("UI Update -> Output: %.0f, Brightness: %d\r\n", MyPID.Output, lv_led_get_brightness(led_heater));
            dbg_div = 0;
        }
    }
}

/**
 * @brief 时钟更新定时任务
 * @param timer 定时器对象（未使用）
 * @details 每秒从RTC更新一次时钟显示
 */
static void update_clock_task(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    update_clock_from_rtc(time_label, date_label);
    update_clock_from_rtc(main_time_label, main_date_label);
}

/**
 * @brief 更新番茄钟UI显示
 * @details 更新进度圆弧、剩余时间、时长标签和按钮文字
 */
static void pomodoro_update_ui(void)
{
    /* 计算进度百分比 */
    uint32_t total = (pomodoro_total_sec == 0U) ? 1U : pomodoro_total_sec;
    uint32_t percent = (pomodoro_remain_sec * 100U) / total;

    /* 更新进度圆弧 */
    if (pomodoro_arc) {
        lv_arc_set_value(pomodoro_arc, (int32_t)percent);
    }
    /* 更新剩余时间显示 */
    if (pomodoro_time_label) {
        uint32_t min = pomodoro_remain_sec / 60U;
        uint32_t sec = pomodoro_remain_sec % 60U;
        lv_label_set_text_fmt(pomodoro_time_label, "%02lu:%02lu", (unsigned long)min, (unsigned long)sec);
    }
    /* 更新时长标签 */
    if (pomodoro_min_label && pomodoro_slider) {
        lv_label_set_text_fmt(pomodoro_min_label, "Duration: %d min", (int)lv_slider_get_value(pomodoro_slider));
    }
    /* 更新按钮文字（运行时显示"暂停"，暂停时显示"开始"） */
    if (pomodoro_btn_start) {
        lv_obj_t *btn_lbl = lv_obj_get_child(pomodoro_btn_start, 0);
        if (btn_lbl) {
            lv_label_set_text(btn_lbl, pomodoro_running ? "Pause" : "Start");
        }
    }
}

/**
 * @brief 番茄钟计时回调函数
 * @param timer 定时器对象（未使用）
 * @details 每秒调用一次，更新剩余时间，计时结束时触发蜂鸣器提示
 */
static void pomodoro_tick_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    /* 如果番茄钟未运行，直接返回 */
    if (!pomodoro_running) {
        return;
    }

    /* 剩余时间减1秒并更新UI */
    if (pomodoro_remain_sec > 0U) {
        pomodoro_remain_sec--;
        pomodoro_update_ui();
    }

    /* 计时结束，停止计时并触发蜂鸣器 */
    if (pomodoro_remain_sec == 0U) {
        pomodoro_running = false;
        pomodoro_update_ui();
        Buzzer_Beep(300);  /* 蜂鸣300ms */
    }
}

/**
 * @brief 番茄钟时长变更回调
 * @param e 事件对象（未使用）
 * @details 仅在番茄钟未运行时允许修改时长
 */
static void pomodoro_duration_changed_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    /* 番茄钟运行中或滑块为空时不处理 */
    if (!pomodoro_slider || pomodoro_running) {
        return;
    }
    /* 更新总时长和剩余时长 */
    uint32_t minutes = (uint32_t)lv_slider_get_value(pomodoro_slider);
    pomodoro_total_sec = minutes * 60U;
    pomodoro_remain_sec = pomodoro_total_sec;
    pomodoro_update_ui();
}

/**
 * @brief 番茄钟开始/暂停回调
 * @param e 事件对象（未使用）
 * @details 切换番茄钟运行状态
 */
static void pomodoro_start_pause_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    pomodoro_running = !pomodoro_running;
    pomodoro_update_ui();
}

/**
 * @brief 番茄钟重置回调
 * @param e 事件对象（未使用）
 * @details 停止番茄钟并重置到设定的时长
 */
static void pomodoro_reset_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    pomodoro_running = false;
    if (pomodoro_slider) {
        pomodoro_total_sec = (uint32_t)lv_slider_get_value(pomodoro_slider) * 60U;
    }
    pomodoro_remain_sec = pomodoro_total_sec;
    pomodoro_update_ui();
}

/**
 * @brief 滚动选择器值变化回调
 * @param e 事件对象
 * @details 根据不同的滚动选择器更新对应的PID参数
 */
static void value_changed_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uint16_t sel = lv_roller_get_selected(obj);

    /* 根据触发事件的对象更新对应的PID参数 */
    if (obj == roller_target) {
        /* 目标温度：20-100℃ */
        MyPID.Target = 20.0f + (float)sel;
        printf("Target Temp Updated: %.1f\n", MyPID.Target);
    } else if (obj == roller_kp) {
        /* 比例系数Kp：0-200，步长5 */
        MyPID.Kp = (float)sel * 5.0f;
        printf("Kp Updated: %.1f\n", MyPID.Kp);
    } else if (obj == roller_ki) {
        /* 积分系数Ki：0-10，步长0.1 */
        MyPID.Ki = (float)sel / 10.0f;
        printf("Ki Updated: %.2f\n", MyPID.Ki);
    } else if (obj == roller_kd) {
        /* 微分系数Kd：0-100，步长5 */
        MyPID.Kd = (float)sel * 5.0f;
        printf("Kd Updated: %.1f\n", MyPID.Kd);
    }
}

/**
 * @brief 创建PID控制页面
 * @param parent 父容器对象
 * @details 提供目标温度和PID参数（Kp、Ki、Kd）的调节界面
 */
void create_pid_control_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    /* 创建温度显示标签 */
    label_temp = lv_label_create(parent);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, -20, 10);

    /* 创建加热器状态LED（红色） */
    led_heater = lv_led_create(parent);
    lv_obj_set_size(led_heater, 18, 18);
    lv_obj_align_to(led_heater, label_temp, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_led_set_color(led_heater, lv_color_hex(0xFF0000));
    lv_led_on(led_heater);
    lv_led_set_brightness(led_heater, 255);

    /* 创建容器用于放置滚动选择器（使用弹性布局） */
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 230, 140);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);  /* 横向排列 */
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* 初始化目标温度选项（20-100℃） */
    static char opts_target[400];
    if (opts_target[0] == '\0') {
        char *p = opts_target;
        for (int i = 20; i <= 100; i++) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_target) {
            *(p - 1) = '\0';  /* 移除最后一个换行符 */
        }
    }

    /* 初始化Kp选项（0-200，步长5） */
    static char opts_kp[200];
    if (opts_kp[0] == '\0') {
        char *p = opts_kp;
        for (int i = 0; i <= 200; i += 5) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_kp) {
            *(p - 1) = '\0';
        }
    }

    /* 初始化Ki选项（0-10，步长0.1） */
    static char opts_ki[500];
    if (opts_ki[0] == '\0') {
        char *p = opts_ki;
        for (int i = 0; i <= 100; i++) {
            p += sprintf(p, "%.1f\n", (float)i / 10.0f);
        }
        if (p > opts_ki) {
            *(p - 1) = '\0';
        }
    }

    /* 初始化Kd选项（0-100，步长5） */
    static char opts_kd[150];
    if (opts_kd[0] == '\0') {
        char *p = opts_kd;
        for (int i = 0; i <= 100; i += 5) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_kd) {
            *(p - 1) = '\0';
        }
    }

    /* 创建目标温度滚动选择器 */
    roller_target = lv_roller_create(cont);
    lv_roller_set_options(roller_target, opts_target, LV_ROLLER_MODE_NORMAL);
    uint16_t target_idx = (MyPID.Target >= 20.0f && MyPID.Target <= 100.0f) ? (uint16_t)(MyPID.Target - 20.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_target, target_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_target, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_target);

    /* 创建Kp滚动选择器 */
    roller_kp = lv_roller_create(cont);
    lv_roller_set_options(roller_kp, opts_kp, LV_ROLLER_MODE_NORMAL);
    uint16_t kp_idx = (MyPID.Kp >= 0 && MyPID.Kp <= 200.0f) ? (uint16_t)(MyPID.Kp / 5.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_kp, kp_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_kp, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_kp);

    /* 创建Ki滚动选择器 */
    roller_ki = lv_roller_create(cont);
    lv_roller_set_options(roller_ki, opts_ki, LV_ROLLER_MODE_NORMAL);
    uint16_t ki_idx = (MyPID.Ki >= 0 && MyPID.Ki <= 10.0f) ? (uint16_t)(MyPID.Ki * 10.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_ki, ki_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_ki, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_ki);

    /* 创建Kd滚动选择器 */
    roller_kd = lv_roller_create(cont);
    lv_roller_set_options(roller_kd, opts_kd, LV_ROLLER_MODE_NORMAL);
    uint16_t kd_idx = (MyPID.Kd >= 0 && MyPID.Kd <= 100.0f) ? (uint16_t)(MyPID.Kd / 5.0f + 0.5f) : 0;
    lv_roller_set_selected(roller_kd, kd_idx, LV_ANIM_OFF);
    lv_obj_add_event_cb(roller_kd, value_changed_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_group_add_obj(g, roller_kd);

    /* 创建参数标签 */
    lv_obj_t *info = lv_label_create(parent);
    lv_label_set_text(info, "Target   Kp   Ki   Kd");
    lv_obj_align_to(info, cont, LV_ALIGN_OUT_TOP_MID, 0, -5);

    /* 创建返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn_back);

    /* 创建温度更新定时器（200ms更新一次） */
    static lv_timer_t *pid_timer = NULL;
    if (pid_timer == NULL) {
        pid_timer = lv_timer_create(update_temp_task, 200, NULL);
    }

    /* 默认聚焦目标温度选择器 */
    lv_group_focus_obj(roller_target);
    printf("PID Page: Objects added to default group and focused\r\n");
}

/**
 * @brief 创建时钟页面
 * @param parent 父容器对象
 * @details 显示当前时间和日期，提供时间设置功能
 */
void create_clock_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    /* 创建时间显示标签（大字体，蓝色） */
    time_label = lv_label_create(parent);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);

    /* 创建日期显示标签 */
    date_label = lv_label_create(parent);
    lv_label_set_text_fmt(date_label, "Date: --/--/----");
    lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* 创建返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    /* 创建设置时间按钮 */
    lv_obj_t *btn_set_time = lv_btn_create(parent);
    lv_obj_set_size(btn_set_time, 120, 30);
    lv_obj_align(btn_set_time, LV_ALIGN_BOTTOM_RIGHT, 0, -5);
    lv_obj_t *lbl_set_time = lv_label_create(btn_set_time);
    lv_label_set_text(lbl_set_time, "Set Time");
    lv_obj_add_event_cb(btn_set_time, event_open_time_setting, LV_EVENT_CLICKED, NULL);

    /* 添加按钮到控件组 */
    if (g) {
        lv_group_add_obj(g, btn_back);
        lv_group_add_obj(g, btn_set_time);
    }

    /* 立即更新一次时钟显示 */
    update_clock_from_rtc(time_label, date_label);
}

/**
 * @brief 创建番茄钟页面
 * @param parent 父容器对象
 * @details 提供番茄钟计时功能，支持开始/暂停、重置和时长设置
 */
void create_pomodoro_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    /* 创建标题 */
    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Pomodoro Timer");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    /* 创建进度圆弧 */
    pomodoro_arc = lv_arc_create(parent);
    lv_obj_set_size(pomodoro_arc, 150, 150);
    lv_obj_align(pomodoro_arc, LV_ALIGN_CENTER, 0, -20);
    lv_arc_set_range(pomodoro_arc, 0, 100);
    lv_obj_remove_flag(pomodoro_arc, LV_OBJ_FLAG_CLICKABLE);  /* 禁止点击 */

    /* 创建时间显示标签 */
    pomodoro_time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(pomodoro_time_label, &lv_font_montserrat_24, 0);
    lv_obj_align_to(pomodoro_time_label, pomodoro_arc, LV_ALIGN_CENTER, 0, 0);

    /* 创建时长标签 */
    pomodoro_min_label = lv_label_create(parent);
    lv_obj_align_to(pomodoro_min_label, pomodoro_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    /* 创建时长滑块（1-60分钟） */
    pomodoro_slider = lv_slider_create(parent);
    lv_obj_set_width(pomodoro_slider, 180);
    lv_obj_align(pomodoro_slider, LV_ALIGN_BOTTOM_MID, 0, -55);
    lv_slider_set_range(pomodoro_slider, 1, 60);
    lv_slider_set_value(pomodoro_slider, (int32_t)(pomodoro_total_sec / 60U), LV_ANIM_OFF);
    lv_obj_add_event_cb(pomodoro_slider, pomodoro_duration_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 创建开始/暂停按钮 */
    pomodoro_btn_start = lv_btn_create(parent);
    lv_obj_set_size(pomodoro_btn_start, 70, 30);
    lv_obj_align(pomodoro_btn_start, LV_ALIGN_BOTTOM_LEFT, 15, -12);
    lv_obj_t *start_lbl = lv_label_create(pomodoro_btn_start);
    lv_label_set_text(start_lbl, "Start");
    lv_obj_center(start_lbl);
    lv_obj_add_event_cb(pomodoro_btn_start, pomodoro_start_pause_cb, LV_EVENT_CLICKED, NULL);

    /* 创建重置按钮 */
    lv_obj_t *btn_reset = lv_btn_create(parent);
    lv_obj_set_size(btn_reset, 70, 30);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_t *reset_lbl = lv_label_create(btn_reset);
    lv_label_set_text(reset_lbl, "Reset");
    lv_obj_center(reset_lbl);
    lv_obj_add_event_cb(btn_reset, pomodoro_reset_cb, LV_EVENT_CLICKED, NULL);

    /* 创建返回按钮 */
    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_RIGHT, -15, -12);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    /* 添加控件到控件组 */
    if (g) {
        lv_group_add_obj(g, pomodoro_slider);
        lv_group_add_obj(g, pomodoro_btn_start);
        lv_group_add_obj(g, btn_reset);
        lv_group_add_obj(g, btn_back);
        lv_group_focus_obj(pomodoro_slider);
    }

    /* 创建番茄钟定时器（每秒更新一次） */
    if (pomodoro_timer == NULL) {
        pomodoro_timer = lv_timer_create(pomodoro_tick_cb, 1000, NULL);
    }
    /* 更新UI显示 */
    pomodoro_update_ui();
}

/**
 * @brief 切换到时钟页面回调
 * @param e 事件对象（未使用）
 * @details 切换页面并蜂鸣提示
 */
static void event_to_clock_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_clock_page);
    Buzzer_Beep(100);
}

/**
 * @brief 切换到番茄钟页面回调
 * @param e 事件对象（未使用）
 * @details 切换页面并蜂鸣提示
 */
static void event_to_pomodoro_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_pomodoro_page);
    Buzzer_Beep(100);
}

/**
 * @brief 切换到设置页面回调
 * @param e 事件对象（未使用）
 * @details 切换页面并蜂鸣提示
 */
static void event_to_settings_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_settings_menu);
    Buzzer_Beep(100);
}

/**
 * @brief 切换到系统信息页面回调
 * @param e 事件对象（未使用）
 * @details 切换页面并蜂鸣提示
 */
static void event_to_info_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_info_page);
    Buzzer_Beep(100);
}

/**
 * @brief 切换到主菜单回调
 * @param e 事件对象（未使用）
 * @details 切换页面并蜂鸣提示
 */
static void event_to_main_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_main_menu);
    Buzzer_Beep(100);
}

/**
 * @brief 切换到PID控制页面回调
 * @param e 事件对象（未使用）
 * @details 切换页面并蜂鸣提示
 */
static void event_to_pid_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_pid_control_page);
    Buzzer_Beep(100);
}

/**
 * @brief 打开时间设置弹窗回调
 * @param e 事件对象（未使用）
 * @details 创建时间设置界面，包含时、分、秒滑块
 */
static void event_open_time_setting(lv_event_t *e)
{
    LV_UNUSED(e);

    /* 创建弹窗容器 */
    time_setting_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(time_setting_popup, 300, 200);
    lv_obj_center(time_setting_popup);
    lv_obj_set_style_bg_color(time_setting_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(time_setting_popup, 2, 0);
    lv_obj_set_style_border_color(time_setting_popup, lv_color_hex(0x333333), 0);

    /* 创建标题 */
    lv_obj_t *title = lv_label_create(time_setting_popup);
    lv_label_set_text(title, "Set Time");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

    /* 创建小时设置区域 */
    lv_obj_t *hour_cont = lv_obj_create(time_setting_popup);
    lv_obj_set_size(hour_cont, 200, 40);
    lv_obj_align(hour_cont, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *hour_text = lv_label_create(time_setting_popup);
    lv_label_set_text(hour_text, "Hour");
    lv_obj_align(hour_text, LV_ALIGN_TOP_LEFT, 0, 20);

    hour_slider = lv_slider_create(hour_cont);
    lv_obj_set_size(hour_slider, 140, 20);
    lv_obj_align(hour_slider, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_slider_set_range(hour_slider, 0, 23);

    lv_obj_t *hour_label = lv_label_create(hour_cont);
    lv_label_set_text_fmt(hour_label, "%02d", 0);
    lv_obj_align(hour_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(hour_slider, time_slider_changed_cb, LV_EVENT_VALUE_CHANGED, hour_label);

    /* 创建分钟设置区域 */
    lv_obj_t *min_cont = lv_obj_create(time_setting_popup);
    lv_obj_set_size(min_cont, 200, 40);
    lv_obj_align(min_cont, LV_ALIGN_TOP_MID, 0, 60);

    lv_obj_t *min_text = lv_label_create(time_setting_popup);
    lv_label_set_text(min_text, "Minute");
    lv_obj_align(min_text, LV_ALIGN_LEFT_MID, 0, 0);

    min_slider = lv_slider_create(min_cont);
    lv_obj_set_size(min_slider, 140, 20);
    lv_obj_align(min_slider, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_slider_set_range(min_slider, 0, 59);

    lv_obj_t *min_label = lv_label_create(min_cont);
    lv_label_set_text_fmt(min_label, "%02d", 0);
    lv_obj_align(min_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(min_slider, time_slider_changed_cb, LV_EVENT_VALUE_CHANGED, min_label);

    /* 创建秒设置区域 */
    lv_obj_t *sec_cont = lv_obj_create(time_setting_popup);
    lv_obj_set_size(sec_cont, 200, 40);
    lv_obj_align(sec_cont, LV_ALIGN_TOP_MID, 0, 100);

    lv_obj_t *sec_text = lv_label_create(time_setting_popup);
    lv_label_set_text(sec_text, "Second");
    lv_obj_align(sec_text, LV_ALIGN_BOTTOM_LEFT, 0, -40);

    sec_slider = lv_slider_create(sec_cont);
    lv_obj_set_size(sec_slider, 140, 20);
    lv_obj_align(sec_slider, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_slider_set_range(sec_slider, 0, 59);

    lv_obj_t *sec_label = lv_label_create(sec_cont);
    lv_label_set_text_fmt(sec_label, "%02d", 0);
    lv_obj_align(sec_label, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_add_event_cb(sec_slider, time_slider_changed_cb, LV_EVENT_VALUE_CHANGED, sec_label);

    /* 创建确认按钮 */
    lv_obj_t *btn_ok = lv_btn_create(time_setting_popup);
    lv_obj_set_size(btn_ok, 100, 40);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "OK");
    lv_obj_center(label_ok);
    lv_obj_add_event_cb(btn_ok, event_set_time_ok, LV_EVENT_CLICKED, NULL);

    /* 创建取消按钮 */
    lv_obj_t *btn_cancel = lv_btn_create(time_setting_popup);
    lv_obj_set_size(btn_cancel, 100, 40);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "Cancel");
    lv_obj_center(label_cancel);
    lv_obj_add_event_cb(btn_cancel, event_set_time_cancel, LV_EVENT_CLICKED, NULL);

    /* 读取当前RTC时间并设置滑块初始值 */
    RTC_TimeTypeDef sTime;
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    lv_slider_set_value(hour_slider, sTime.Hours, LV_ANIM_ON);
    lv_slider_set_value(min_slider, sTime.Minutes, LV_ANIM_ON);
    lv_slider_set_value(sec_slider, sTime.Seconds, LV_ANIM_ON);
}

/**
 * @brief 时间设置确认回调
 * @param e 事件对象（未使用）
 * @details 将滑块值设置到RTC并关闭弹窗
 */
static void event_set_time_ok(lv_event_t *e)
{
    LV_UNUSED(e);

    /* 获取滑块值 */
    uint8_t hour = (uint8_t)lv_slider_get_value(hour_slider);
    uint8_t min = (uint8_t)lv_slider_get_value(min_slider);
    uint8_t sec = (uint8_t)lv_slider_get_value(sec_slider);

    /* 设置RTC时间 */
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = hour;
    sTime.Minutes = min;
    sTime.Seconds = sec;

    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    /* 关闭弹窗 */
    lv_obj_del(time_setting_popup);
    time_setting_popup = NULL;
}

/**
 * @brief 时间设置取消回调
 * @param e 事件对象（未使用）
 * @details 关闭弹窗不做任何修改
 */
static void event_set_time_cancel(lv_event_t *e)
{
    LV_UNUSED(e);

    /* 关闭弹窗 */
    lv_obj_del(time_setting_popup);
    time_setting_popup = NULL;
}

/**
 * @brief 时间滑块值变化回调
 * @param e 事件对象
 * @details 更新对应的数值显示标签
 */
static void time_slider_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);

    if (label) {
        int32_t value = lv_slider_get_value(slider);
        lv_label_set_text_fmt(label, "%02d", value);
    }
}
