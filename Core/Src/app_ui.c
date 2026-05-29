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

extern PID_TypeDef MyPID;
extern RTC_HandleTypeDef hrtc;

static lv_obj_t *date_label;
static lv_obj_t *time_label;
static lv_obj_t *label_temp;
static lv_obj_t *label_vdd;
static lv_obj_t *chart_temp;
static lv_chart_series_t *ser_temp;
static lv_obj_t *roller_kp, *roller_ki, *roller_kd, *roller_target;
static lv_obj_t *led_heater;
static lv_obj_t *pomodoro_arc;
static lv_obj_t *pomodoro_time_label;
static lv_obj_t *pomodoro_min_label;
static lv_obj_t *pomodoro_slider;
static lv_obj_t *pomodoro_btn_start;
static uint32_t pomodoro_total_sec = 25U * 60U;
static uint32_t pomodoro_remain_sec = 25U * 60U;
static bool pomodoro_running = false;
static lv_timer_t *pomodoro_timer = NULL;
static lv_obj_t *time_setting_popup = NULL;
static lv_obj_t *hour_slider = NULL;
static lv_obj_t *min_slider = NULL;
static lv_obj_t *sec_slider = NULL;

static void event_to_settings_cb(lv_event_t *e);
static void event_to_info_cb(lv_event_t *e);
static void event_to_main_cb(lv_event_t *e);
static void event_to_pid_cb(lv_event_t *e);
static void event_to_clock_cb(lv_event_t *e);
static void event_to_pomodoro_cb(lv_event_t *e);
static void event_set_time_ok(lv_event_t *e);
static void event_set_time_cancel(lv_event_t *e);
static void time_slider_changed_cb(lv_event_t *e);
static void event_open_time_setting(lv_event_t *e);
static void value_changed_event_cb(lv_event_t *e);
static void update_temp_task(lv_timer_t *timer);
static void pomodoro_tick_cb(lv_timer_t *timer);
static void pomodoro_duration_changed_cb(lv_event_t *e);
static void pomodoro_start_pause_cb(lv_event_t *e);
static void pomodoro_reset_cb(lv_event_t *e);
static void update_clock_task(lv_timer_t *timer);
static void pomodoro_update_ui(void);

void switch_page(menu_page_func_t page_func)
{
    lv_group_t *g = lv_group_get_default();
    if (g == NULL) {
        g = lv_group_create();
        lv_group_set_default(g);
    }

    lv_obj_clean(lv_scr_act());
    lv_group_remove_all_objs(g);

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
    pomodoro_arc = NULL;
    pomodoro_time_label = NULL;
    pomodoro_min_label = NULL;
    pomodoro_slider = NULL;
    pomodoro_btn_start = NULL;
    pomodoro_running = false;

    if (page_func) {
        page_func(lv_scr_act());
    }

    printf("Page Switched: Screen cleaned and group cleared\r\n");
}

void create_main_menu(lv_obj_t *parent)
{
    lv_obj_t *list = lv_list_create(parent);
    lv_obj_set_size(list, lv_pct(100), lv_pct(100));
    lv_obj_center(list);

    lv_list_add_text(list, "Main Menu");

    lv_obj_t *btn_clock = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Clock");
    lv_obj_add_event_cb(btn_clock, event_to_clock_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_pomodoro = lv_list_add_btn(list, LV_SYMBOL_REFRESH, "Pomodoro");
    lv_obj_add_event_cb(btn_pomodoro, event_to_pomodoro_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_set = lv_list_add_btn(list, LV_SYMBOL_SETTINGS, "Settings");
    lv_obj_add_event_cb(btn_set, event_to_settings_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_info = lv_list_add_btn(list, LV_SYMBOL_FILE, "System Info");
    lv_obj_add_event_cb(btn_info, event_to_info_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_pid = lv_list_add_btn(list, LV_SYMBOL_EDIT, "PID Control");
    lv_obj_add_event_cb(btn_pid, event_to_pid_cb, LV_EVENT_CLICKED, NULL);

    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_add_obj(g, btn_set);
        lv_group_add_obj(g, btn_info);
        lv_group_add_obj(g, btn_pid);
        lv_group_add_obj(g, btn_clock);
        lv_group_add_obj(g, btn_pomodoro);
        lv_group_focus_obj(btn_clock);
    }
}

void create_settings_menu(lv_obj_t *parent)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, "Settings Page");
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *slider = lv_slider_create(parent);
    lv_obj_set_width(slider, lv_pct(80));
    lv_obj_center(slider);

    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    lv_group_t *g = lv_group_get_default();
    if (g) {
        lv_group_add_obj(g, slider);
        lv_group_add_obj(g, btn_back);
    }
}

void create_info_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    lv_obj_t *label_title = lv_label_create(parent);
    lv_label_set_text(label_title, "Real-time Monitoring");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 10);

    label_temp = lv_label_create(parent);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, 0, 35);

    label_vdd = lv_label_create(parent);
    lv_obj_align(label_vdd, LV_ALIGN_TOP_MID, 0, 55);

    chart_temp = lv_chart_create(parent);
    lv_obj_set_size(chart_temp, 200, 120);
    lv_obj_align(chart_temp, LV_ALIGN_CENTER, 0, 20);
    lv_chart_set_type(chart_temp, LV_CHART_TYPE_LINE);
    lv_chart_set_range(chart_temp, LV_CHART_AXIS_PRIMARY_Y, core_temp - 5, core_temp + 5);
    lv_chart_set_point_count(chart_temp, 50);

    lv_obj_set_style_line_color(chart_temp, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_chart_set_div_line_count(chart_temp, 6, 5);

    ser_temp = lv_chart_add_series(chart_temp, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);

    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 80, 40);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_t *label_back = lv_label_create(btn_back);
    lv_label_set_text(label_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn_back);

    static lv_timer_t *info_timer = NULL;
    if (info_timer == NULL) {
        info_timer = lv_timer_create(update_temp_task, 200, NULL);
    }
}

void update_clock_from_rtc(lv_obj_t *time_lbl, lv_obj_t *date_lbl)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    if (time_lbl) {
        lv_label_set_text_fmt(time_lbl, "%02d:%02d:%02d",
                              sTime.Hours, sTime.Minutes, sTime.Seconds);
    }
    if (date_lbl) {
        lv_label_set_text_fmt(date_lbl, "20%02d-%02d-%02d",
                              sDate.Year, sDate.Month, sDate.Date);
    }
}

static void update_temp_task(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (label_temp) {
        lv_label_set_text_fmt(label_temp, "Core Temp: %.1f °C", core_temp);
    }

    if (label_vdd) {
        lv_label_set_text_fmt(label_vdd, "Vdd: %.2f V", vdd_v);
    }

    if (chart_temp && ser_temp) {
        lv_chart_set_next_value(chart_temp, ser_temp, (int)core_temp);
    }

    if (led_heater) {
        if (MyPID.Output > 0.5f) {
            uint8_t brightness = (uint8_t)(120.0f + (MyPID.Output * 135.0f / 1000.0f));
            if (brightness > 255U) {
                brightness = 255U;
            }
            lv_led_on(led_heater);
            lv_led_set_brightness(led_heater, brightness);
        } else {
            lv_led_off(led_heater);
            lv_led_set_brightness(led_heater, 0);
        }

        static uint8_t dbg_div = 0;
        if (++dbg_div > 5) {
            printf("UI Update -> Output: %.0f, Brightness: %d\r\n", MyPID.Output, lv_led_get_brightness(led_heater));
            dbg_div = 0;
        }
    }
}

static void update_clock_task(lv_timer_t *timer)
{
    LV_UNUSED(timer);
    update_clock_from_rtc(time_label, date_label);
}

static void pomodoro_update_ui(void)
{
    uint32_t total = (pomodoro_total_sec == 0U) ? 1U : pomodoro_total_sec;
    uint32_t percent = (pomodoro_remain_sec * 100U) / total;

    if (pomodoro_arc) {
        lv_arc_set_value(pomodoro_arc, (int32_t)percent);
    }
    if (pomodoro_time_label) {
        uint32_t min = pomodoro_remain_sec / 60U;
        uint32_t sec = pomodoro_remain_sec % 60U;
        lv_label_set_text_fmt(pomodoro_time_label, "%02lu:%02lu", (unsigned long)min, (unsigned long)sec);
    }
    if (pomodoro_min_label && pomodoro_slider) {
        lv_label_set_text_fmt(pomodoro_min_label, "Duration: %d min", (int)lv_slider_get_value(pomodoro_slider));
    }
    if (pomodoro_btn_start) {
        lv_obj_t *btn_lbl = lv_obj_get_child(pomodoro_btn_start, 0);
        if (btn_lbl) {
            lv_label_set_text(btn_lbl, pomodoro_running ? "Pause" : "Start");
        }
    }
}

static void pomodoro_tick_cb(lv_timer_t *timer)
{
    LV_UNUSED(timer);

    if (!pomodoro_running) {
        return;
    }

    if (pomodoro_remain_sec > 0U) {
        pomodoro_remain_sec--;
        pomodoro_update_ui();
    }

    if (pomodoro_remain_sec == 0U) {
        pomodoro_running = false;
        pomodoro_update_ui();
        Buzzer_Beep(300);
    }
}

static void pomodoro_duration_changed_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    if (!pomodoro_slider || pomodoro_running) {
        return;
    }
    uint32_t minutes = (uint32_t)lv_slider_get_value(pomodoro_slider);
    pomodoro_total_sec = minutes * 60U;
    pomodoro_remain_sec = pomodoro_total_sec;
    pomodoro_update_ui();
}

static void pomodoro_start_pause_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    pomodoro_running = !pomodoro_running;
    pomodoro_update_ui();
}

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

static void value_changed_event_cb(lv_event_t *e)
{
    lv_obj_t *obj = lv_event_get_target(e);
    uint16_t sel = lv_roller_get_selected(obj);

    if (obj == roller_target) {
        MyPID.Target = 20.0f + (float)sel;
        printf("Target Temp Updated: %.1f\n", MyPID.Target);
    } else if (obj == roller_kp) {
        MyPID.Kp = (float)sel * 5.0f;
        printf("Kp Updated: %.1f\n", MyPID.Kp);
    } else if (obj == roller_ki) {
        MyPID.Ki = (float)sel / 10.0f;
        printf("Ki Updated: %.2f\n", MyPID.Ki);
    } else if (obj == roller_kd) {
        MyPID.Kd = (float)sel * 5.0f;
        printf("Kd Updated: %.1f\n", MyPID.Kd);
    }
}

void create_pid_control_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    label_temp = lv_label_create(parent);
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_14, 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_MID, -20, 10);

    led_heater = lv_led_create(parent);
    lv_obj_set_size(led_heater, 18, 18);
    lv_obj_align_to(led_heater, label_temp, LV_ALIGN_OUT_RIGHT_MID, 40, 0);
    lv_led_set_color(led_heater, lv_color_hex(0xFF0000));
    lv_led_on(led_heater);
    lv_led_set_brightness(led_heater, 255);

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 230, 140);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 10);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    static char opts_target[400];
    if (opts_target[0] == '\0') {
        char *p = opts_target;
        for (int i = 20; i <= 100; i++) {
            p += sprintf(p, "%d\n", i);
        }
        if (p > opts_target) {
            *(p - 1) = '\0';
        }
    }

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

    roller_target = lv_roller_create(cont);
    lv_roller_set_options(roller_target, opts_target, LV_ROLLER_MODE_NORMAL);
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

    lv_obj_t *info = lv_label_create(parent);
    lv_label_set_text(info, "Target   Kp   Ki   Kd");
    lv_obj_align_to(info, cont, LV_ALIGN_OUT_TOP_MID, 0, -5);

    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, btn_back);

    static lv_timer_t *pid_timer = NULL;
    if (pid_timer == NULL) {
        pid_timer = lv_timer_create(update_temp_task, 200, NULL);
    }

    lv_group_focus_obj(roller_target);
    printf("PID Page: Objects added to default group and focused\r\n");
}

void create_clock_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    time_label = lv_label_create(parent);
    lv_label_set_text(time_label, "--:--:--");
    lv_obj_set_style_text_font(time_label, &lv_font_montserrat_48, 0);
    lv_obj_set_style_text_color(time_label, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_align(time_label, LV_ALIGN_CENTER, 0, 0);

    date_label = lv_label_create(parent);
    lv_label_set_text_fmt(date_label, "Date: --/--/----");
    lv_obj_align_to(date_label, time_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 60, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_LEFT, 0, -5);
    lv_obj_t *lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_set_time = lv_btn_create(parent);
    lv_obj_set_size(btn_set_time, 120, 30);
    lv_obj_align(btn_set_time, LV_ALIGN_BOTTOM_RIGHT, 0, -5);
    lv_obj_t *lbl_set_time = lv_label_create(btn_set_time);
    lv_label_set_text(lbl_set_time, "Set Time");
    lv_obj_add_event_cb(btn_set_time, event_open_time_setting, LV_EVENT_CLICKED, NULL);

    if (g) {
        lv_group_add_obj(g, btn_back);
        lv_group_add_obj(g, btn_set_time);
    }

    static lv_timer_t *clock_timer = NULL;
    if (clock_timer == NULL) {
        clock_timer = lv_timer_create(update_clock_task, 1000, NULL);
    }
    update_clock_from_rtc(time_label, date_label);
}

void create_pomodoro_page(lv_obj_t *parent)
{
    lv_group_t *g = lv_group_get_default();

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "Pomodoro Timer");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    pomodoro_arc = lv_arc_create(parent);
    lv_obj_set_size(pomodoro_arc, 150, 150);
    lv_obj_align(pomodoro_arc, LV_ALIGN_CENTER, 0, -20);
    lv_arc_set_range(pomodoro_arc, 0, 100);
    lv_obj_remove_flag(pomodoro_arc, LV_OBJ_FLAG_CLICKABLE);

    pomodoro_time_label = lv_label_create(parent);
    lv_obj_set_style_text_font(pomodoro_time_label, &lv_font_montserrat_24, 0);
    lv_obj_align_to(pomodoro_time_label, pomodoro_arc, LV_ALIGN_CENTER, 0, 0);

    pomodoro_min_label = lv_label_create(parent);
    lv_obj_align_to(pomodoro_min_label, pomodoro_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    pomodoro_slider = lv_slider_create(parent);
    lv_obj_set_width(pomodoro_slider, 180);
    lv_obj_align(pomodoro_slider, LV_ALIGN_BOTTOM_MID, 0, -55);
    lv_slider_set_range(pomodoro_slider, 1, 60);
    lv_slider_set_value(pomodoro_slider, (int32_t)(pomodoro_total_sec / 60U), LV_ANIM_OFF);
    lv_obj_add_event_cb(pomodoro_slider, pomodoro_duration_changed_cb, LV_EVENT_VALUE_CHANGED, NULL);

    pomodoro_btn_start = lv_btn_create(parent);
    lv_obj_set_size(pomodoro_btn_start, 70, 30);
    lv_obj_align(pomodoro_btn_start, LV_ALIGN_BOTTOM_LEFT, 15, -12);
    lv_obj_t *start_lbl = lv_label_create(pomodoro_btn_start);
    lv_label_set_text(start_lbl, "Start");
    lv_obj_center(start_lbl);
    lv_obj_add_event_cb(pomodoro_btn_start, pomodoro_start_pause_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_reset = lv_btn_create(parent);
    lv_obj_set_size(btn_reset, 70, 30);
    lv_obj_align(btn_reset, LV_ALIGN_BOTTOM_MID, 0, -12);
    lv_obj_t *reset_lbl = lv_label_create(btn_reset);
    lv_label_set_text(reset_lbl, "Reset");
    lv_obj_center(reset_lbl);
    lv_obj_add_event_cb(btn_reset, pomodoro_reset_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_back = lv_btn_create(parent);
    lv_obj_set_size(btn_back, 70, 30);
    lv_obj_align(btn_back, LV_ALIGN_BOTTOM_RIGHT, -15, -12);
    lv_obj_t *back_lbl = lv_label_create(btn_back);
    lv_label_set_text(back_lbl, "Back");
    lv_obj_center(back_lbl);
    lv_obj_add_event_cb(btn_back, event_to_main_cb, LV_EVENT_CLICKED, NULL);

    if (g) {
        lv_group_add_obj(g, pomodoro_slider);
        lv_group_add_obj(g, pomodoro_btn_start);
        lv_group_add_obj(g, btn_reset);
        lv_group_add_obj(g, btn_back);
        lv_group_focus_obj(pomodoro_slider);
    }

    if (pomodoro_timer == NULL) {
        pomodoro_timer = lv_timer_create(pomodoro_tick_cb, 1000, NULL);
    }
    pomodoro_update_ui();
}

static void event_to_clock_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_clock_page);
    Buzzer_Beep(100);
}

static void event_to_pomodoro_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_pomodoro_page);
    Buzzer_Beep(100);
}

static void event_to_settings_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_settings_menu);
    Buzzer_Beep(100);
}

static void event_to_info_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_info_page);
    Buzzer_Beep(100);
}

static void event_to_main_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_main_menu);
    Buzzer_Beep(100);
}

static void event_to_pid_cb(lv_event_t *e)
{
    LV_UNUSED(e);
    switch_page(create_pid_control_page);
    Buzzer_Beep(100);
}

static void event_open_time_setting(lv_event_t *e)
{
    LV_UNUSED(e);

    time_setting_popup = lv_obj_create(lv_scr_act());
    lv_obj_set_size(time_setting_popup, 300, 200);
    lv_obj_center(time_setting_popup);
    lv_obj_set_style_bg_color(time_setting_popup, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(time_setting_popup, 2, 0);
    lv_obj_set_style_border_color(time_setting_popup, lv_color_hex(0x333333), 0);

    lv_obj_t *title = lv_label_create(time_setting_popup);
    lv_label_set_text(title, "Set Time");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);

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

    lv_obj_t *btn_ok = lv_btn_create(time_setting_popup);
    lv_obj_set_size(btn_ok, 100, 40);
    lv_obj_align(btn_ok, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
    lv_obj_t *label_ok = lv_label_create(btn_ok);
    lv_label_set_text(label_ok, "OK");
    lv_obj_center(label_ok);
    lv_obj_add_event_cb(btn_ok, event_set_time_ok, LV_EVENT_CLICKED, NULL);

    lv_obj_t *btn_cancel = lv_btn_create(time_setting_popup);
    lv_obj_set_size(btn_cancel, 100, 40);
    lv_obj_align(btn_cancel, LV_ALIGN_BOTTOM_LEFT, 0, 0);
    lv_obj_t *label_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(label_cancel, "Cancel");
    lv_obj_center(label_cancel);
    lv_obj_add_event_cb(btn_cancel, event_set_time_cancel, LV_EVENT_CLICKED, NULL);

    RTC_TimeTypeDef sTime;
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    lv_slider_set_value(hour_slider, sTime.Hours, LV_ANIM_ON);
    lv_slider_set_value(min_slider, sTime.Minutes, LV_ANIM_ON);
    lv_slider_set_value(sec_slider, sTime.Seconds, LV_ANIM_ON);
}

static void event_set_time_ok(lv_event_t *e)
{
    LV_UNUSED(e);

    uint8_t hour = (uint8_t)lv_slider_get_value(hour_slider);
    uint8_t min = (uint8_t)lv_slider_get_value(min_slider);
    uint8_t sec = (uint8_t)lv_slider_get_value(sec_slider);

    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = hour;
    sTime.Minutes = min;
    sTime.Seconds = sec;

    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    lv_obj_del(time_setting_popup);
    time_setting_popup = NULL;
}

static void event_set_time_cancel(lv_event_t *e)
{
    LV_UNUSED(e);

    lv_obj_del(time_setting_popup);
    time_setting_popup = NULL;
}

static void time_slider_changed_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);

    if (label) {
        int32_t value = lv_slider_get_value(slider);
        lv_label_set_text_fmt(label, "%02d", value);
    }
}
