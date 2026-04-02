
/**
 * @file lv_port_indev.c
 *
 */

#include "lv_port_indev.h"
#include "tim.h"
#include "key.h"
#include <stdio.h>

/*********************
 *      STATIC PROTOTYPES
 *********************/
static void encoder_init(void);
static void encoder_read(lv_indev_t * indev, lv_indev_data_t * data);

/**********************
 *  STATIC VARIABLES
 **********************/
lv_indev_t * indev_encoder;
static Key_t encoder_key;
static Key_Event_t last_key_event = KEY_EVENT_NONE;

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

/**
 * @brief 按键扫描函数，应以 10ms 周期调用
 */
void lv_port_indev_scan(void) {
    Key_Event_t event = Key_Scan(&encoder_key);
    if (event != KEY_EVENT_NONE) {
        last_key_event = event;
        
        // 打印调试信息
        switch(event) {
            case KEY_EVENT_SINGLE_CLICK: printf("Key Event: SINGLE CLICK\r\n"); break;
            case KEY_EVENT_DOUBLE_CLICK: printf("Key Event: DOUBLE CLICK\r\n"); break;
            case KEY_EVENT_LONG_PRESS:   printf("Key Event: LONG PRESS\r\n"); break;
            default: break;
        }
    }
}

void lv_port_indev_init(void)
{
    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();
    
    /* 初始化按键结构体 (PC13, 低电平有效) */
    Key_Init(&encoder_key, GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

    /*Register a encoder input device*/
    indev_encoder = lv_indev_create();
    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder, encoder_read);

    /* 必须：创建一个全局默认组并绑定到编码器 */
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(indev_encoder, g);
    
    printf("Indev: Encoder & Key (PC13) initialized\r\n");
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Encoder
 * -----------------*/

/*Initialize your encoder*/
static void encoder_init(void)
{
    /*Start TIM4 encoder mode*/
    HAL_TIM_Encoder_Start(&htim4, TIM_CHANNEL_ALL);
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{

    static int16_t last_counter = 0;
    int16_t current_counter = (int16_t)__HAL_TIM_GET_COUNTER(&htim4);
    static int16_t enc_acc = 0;//减速处理
    // 1. 处理旋转
    int16_t diff = current_counter - last_counter;
    last_counter = current_counter;
    
    if(diff != 0) {
        printf("Raw Diff: %d, Counter: %d\r\n", (int)diff, (int)current_counter);
    }
    
    enc_acc += diff;

    // 每累积 2 个脉冲就动一次（2脉冲/格）
    if(enc_acc >= 2) {
        data->enc_diff = 1;
        enc_acc = 0;
    }
    else if(enc_acc <= -2) {
        data->enc_diff = -1;
        enc_acc = 0;
    }
    else {
        data->enc_diff = 0;
    }
    
    // 2. 处理按键 (使用新 Key 模块)
    // 根据状态机的内部状态判定当前物理按键是否被按下
    if (encoder_key.State != KEY_STATE_IDLE && encoder_key.State != KEY_STATE_WAIT_DOUBLE && encoder_key.State != KEY_STATE_WAIT_LONG_RELEASE) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    
    // 如果发生了特定事件，可以进行额外处理
    if (last_key_event != KEY_EVENT_NONE) {
        // 这里可以将事件映射到 LVGL 的特定操作，或者直接让 LVGL 处理基本的 ENTER
        data->key = LV_KEY_ENTER;
        last_key_event = KEY_EVENT_NONE; // 消费掉事件
    } else {
        data->key = LV_KEY_ENTER;
    }
    
    /* 调试输出：如果转动了编码器，打印一下差异 */
    if(data->enc_diff != 0) {
        printf("Enc Diff: %d, Total: %d\r\n", (int)data->enc_diff, (int)current_counter);
    }
}
