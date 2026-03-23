
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
    
    // 1. 处理旋转
    data->enc_diff = current_counter - last_counter;
    last_counter = current_counter;

    // 2. 处理按键 (C相/SW)
    // 假设按键接在 GPIOA PIN 0, 低电平有效
    // 如果没有物理按键，这里可以始终设为 RELEASED
    static uint8_t debounce_cnt = 0;
    if (HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0) == GPIO_PIN_RESET) {
        if (debounce_cnt < 3) debounce_cnt++;
    } else {
        debounce_cnt = 0;
    }

    if (debounce_cnt >= 2) {
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
    
    /* 调试输出：如果转动了编码器，打印一下差异 */
    if(data->enc_diff != 0) {
        printf("Enc Diff: %d, Total: %d\r\n", (int)data->enc_diff, (int)current_counter);
    }
}
