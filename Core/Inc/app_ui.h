#ifndef APP_UI_H
#define APP_UI_H

#include "lvgl.h"

typedef void (*menu_page_func_t)(lv_obj_t *parent);

void switch_page(menu_page_func_t page_func);

void create_main_menu(lv_obj_t *parent);
void create_settings_menu(lv_obj_t *parent);
void create_info_page(lv_obj_t *parent);
void create_pid_control_page(lv_obj_t *parent);
void create_pomodoro_page(lv_obj_t *parent);
void create_clock_page(lv_obj_t *parent);

void update_clock_from_rtc(lv_obj_t *time_label, lv_obj_t *date_label);

#endif
