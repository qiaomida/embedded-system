
/**
 * @file lv_port_indev.c
 *
 */

#include "lv_port_indev.h"
#include "tim.h"
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

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_indev_init(void)
{
    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    indev_encoder = lv_indev_create();
    lv_indev_set_type(indev_encoder, LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder, encoder_read);

    /*Assign encoder to default group*/
    lv_group_t * g = lv_group_get_default();
    if(g == NULL) {
        g = lv_group_create();
        lv_group_set_default(g);
    }
    lv_indev_set_group(indev_encoder, g);
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
    // 2. 处理按键 (换成 PC13)                                                                                                                                                                                     
    // PC13 通常是板载蓝色按钮或外部接线，低电平有效
    static uint8_t debounce_cnt = 0;
    GPIO_PinState pin_state = HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13);
    
    if (pin_state == GPIO_PIN_RESET) {
        if (debounce_cnt < 3) debounce_cnt++;
    } else {
        debounce_cnt = 0;
    }

    static lv_indev_state_t last_state = LV_INDEV_STATE_RELEASED;
    if (debounce_cnt >= 2) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->key = LV_KEY_ENTER; 
        static uint32_t last_print = 0;
        if(HAL_GetTick() - last_print > 500) { 
            printf("Button SW: PRESSED (PC13 Low)\r\n");
            last_print = HAL_GetTick();
        }
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    
    if(data->state != last_state) {
        if(data->state == LV_INDEV_STATE_RELEASED) {
            printf("Button SW: RELEASED (PC13 High)\r\n");
        }
        last_state = data->state;
    }
    
    /* 调试输出：如果转动了编码器，打印一下差异 */
    if(data->enc_diff != 0) {
        printf("Enc Diff: %d, Total: %d\r\n", (int)data->enc_diff, (int)current_counter);
    }
}
