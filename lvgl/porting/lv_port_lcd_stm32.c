#include "lv_port_lcd_stm32.h"
#include "lvgl.h"
#include "spi.h"
#include "main.h"

#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    320 

static lv_display_t * lcd_disp;
static volatile int lcd_bus_busy = 0;

/* 极简底层发送，确保初始化指令 100% 送达 */
static void raw_write_cmd(uint8_t cmd) {
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, &cmd, 1, 100);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void raw_write_data(uint8_t data) {
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET); // DC High
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET); // CS Low
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
    HAL_SPI_Transmit(&hspi1, &data, 1, 100);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

/* 关键：DMA 完成回调函数 */
/* 必须确保在 CubeMX 中开启了 SPI1 的 DMA TX 中断和 SPI1 全局中断 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
    if (hspi->Instance == SPI1) {
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        lcd_bus_busy = 0;
        /* 告诉 LVGL 这一帧刷完了 */
        if(lcd_disp) {
            lv_display_flush_ready(lcd_disp);
        }
    }
}

static void lcd_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param, size_t param_size)
{
    while(lcd_bus_busy); // 如果 DMA 没结束，会卡死在这里
    
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)cmd, cmd_size, 100);
    
    if(param_size > 0) {
        HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
        HAL_SPI_Transmit(&hspi1, (uint8_t *)param, param_size, 100);
    }
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
}

static void lcd_send_color(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param, size_t param_size)
{
    while(lcd_bus_busy); 

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)cmd, cmd_size, 100);

    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);

    /* --- 测试方案：先注释 DMA，改用阻塞模式 --- */
    /* 如果阻塞模式亮了，说明是 DMA 中断没配置好 */
    
    HAL_SPI_Transmit(&hspi1, param, param_size, 1000);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    lv_display_flush_ready(disp);
    

    /* --- 方案 B：正确配置的 DMA 模式 --- */
    // lcd_bus_busy = 1;
    // if(HAL_SPI_Transmit_DMA(&hspi1, param, (uint16_t)param_size) != HAL_OK) {
    //     lcd_bus_busy = 0;
    //     HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    //     lv_display_flush_ready(disp);
    }

void lv_port_disp_init(void)
{
    // 硬件复位序列
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);
    HAL_Delay(50);
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);
    HAL_Delay(120);

    // ST7789 初始化指令 (根据变蓝的经验修正)
    raw_write_cmd(0x11); // Exit Sleep
    HAL_Delay(120);
    raw_write_cmd(0x36); raw_write_data(0x00); // 方向
    raw_write_cmd(0x3A); raw_write_data(0x05); // 16bit
    raw_write_cmd(0x21); // Inversion On
    raw_write_cmd(0x29); // Display On
    HAL_Delay(20);

    // 创建显示对象
    lcd_disp = lv_st7789_create(MY_DISP_HOR_RES, MY_DISP_VER_RES, 0, lcd_send_cmd, lcd_send_color);
    
    // 强制字节序交换 (解决蓝/红反转)
    lv_draw_sw_rgb565_swap(NULL, 0); 

    // 设置缓冲区 (缓冲区大小要适中)
    static uint8_t buf1[MY_DISP_HOR_RES * 40 * 2];
    lv_display_set_buffers(lcd_disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}
