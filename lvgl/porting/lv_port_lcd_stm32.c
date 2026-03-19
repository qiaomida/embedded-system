#include "lv_port_lcd_stm32.h"
#include "lvgl.h"
#include "spi.h"
#include "main.h"
#include"stdio.h"
#include "cmsis_os2.h"
#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    320 

static lv_display_t * lcd_disp;
static volatile int lcd_bus_busy = 0;
// 放在文件最上面（函数外）
static uint8_t buf1[MY_DISP_HOR_RES * 20 * 2];
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
#define LCD_BUS_BUSY_TIMEOUT_MS 200U

static void lcd_color_transfer_ready_cb(SPI_HandleTypeDef * hspi)
{
    if(hspi->Instance != SPI1) {
        return;
    }

    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    lcd_bus_busy = 0;

    if(lcd_disp) {
        lv_display_flush_ready(lcd_disp);
    }
}

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef * hspi)
{
    lcd_color_transfer_ready_cb(hspi);
}

static int lcd_wait_bus(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while(lcd_bus_busy) {
        if((HAL_GetTick() - start) > timeout_ms) {
        lcd_bus_busy = 0;
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
            if(lcd_disp) {
                lv_display_flush_ready(lcd_disp);
            }
            return -1;
        }
    }

    return 0;
}

static void lcd_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param, size_t param_size)
{
    LV_UNUSED(disp);
 printf("CMD called\n");
    if(lcd_wait_bus(LCD_BUS_BUSY_TIMEOUT_MS) != 0) return;

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
    if(lcd_wait_bus(LCD_BUS_BUSY_TIMEOUT_MS) != 0) return;
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, (uint8_t *)cmd, cmd_size, 100);

    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);

    /* --- DMA mode --- */
    lcd_bus_busy = 1;
    if(HAL_SPI_Transmit_DMA(&hspi1, param, (uint16_t)param_size) != HAL_OK) {
        lcd_bus_busy = 0;
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        lv_display_flush_ready(disp);
    }
}
void lv_port_disp_init(void)
{
    printf("1\n");
    lcd_bus_busy = 0;
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
#if (USE_HAL_SPI_REGISTER_CALLBACKS == 1U)
    (void)HAL_SPI_RegisterCallback(&hspi1, HAL_SPI_TX_COMPLETE_CB_ID, lcd_color_transfer_ready_cb);
#endif
    // 硬件复位序列
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);
    osDelay(50);
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);
    osDelay(120);
    // ST7789 初始化指令 (根据变蓝的经验修正)
     raw_write_cmd(0x11); // Exit Sleep
    osDelay(120);
     raw_write_cmd(0x36); raw_write_data(0x08); // 方向
     raw_write_cmd(0x3A); raw_write_data(0x05); // 16bit
     raw_write_cmd(0x21); // Inversion On
     raw_write_cmd(0x29); // Display On
    osDelay(20);
    printf("A\n");
    // 创建显示对象
     //lcd_disp = lv_st7789_create(MY_DISP_HOR_RES, MY_DISP_VER_RES, 0, NULL, NULL);
    printf("B\n");
    // 设置缓冲区 (缓冲区大小要适中)
    //lv_display_set_buffers(lcd_disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

}
