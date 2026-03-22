/**
 * @file lv_port_lcd_stm32_template.c
 *
 * Example implementation of the LVGL LCD display drivers on the STM32 platform
 */

/*Copy this file as "lv_port_disp.c" and set this value to "1" to enable content*/
#if 1

/*********************
 *      INCLUDES
 *********************/
/* Include STM32Cube files here, e.g.:
#include "stm32f7xx_hal.h"
*/

#include "lv_port_lcd_stm32.h"
#include "./src/drivers/display/st7789/lv_st7789.h"
#include <string.h>
#include <stdio.h>
#include <spi.h>
#include <gpio.h>
#include <usart.h>
#include <cmsis_os.h>
/*********************
 *      DEFINES
 *********************/
#define MY_DISP_HOR_RES    240
#define MY_DISP_VER_RES    320
#define BUS_SPI1_POLL_TIMEOUT 0x1000U

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static int32_t lcd_io_init(void);
static int32_t lcd_spi_set_data_size(uint32_t data_size);
static void lcd_trace(const char * msg);
static void lcd_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param,
                         size_t param_size);
static void lcd_send_color(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param,
                           size_t param_size);

/**********************
 *  STATIC VARIABLES
 **********************/
static lv_display_t * lcd_disp;
extern volatile uint8_t lcd_bus_busy; // 使用 main.c 中定义的全局变量，去掉 static 防止冲突

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    // /* Initialize LCD I/O */
     if(lcd_io_init() != 0)
        return;


    /* Create the LVGL display object and the ST7789 LCD display driver */
    lcd_disp = lv_st7789_create(MY_DISP_HOR_RES, MY_DISP_VER_RES, LV_LCD_FLAG_NONE, lcd_send_cmd, lcd_send_color);
    if(lcd_disp == NULL) {
        lcd_trace("disp:create_null");
        return;
    }

    
    lv_display_set_rotation(lcd_disp, LV_DISPLAY_ROTATION_270);     /* set landscape orientation */

    /* Example: two dynamically allocated buffers for partial rendering */
   static uint8_t buf1[MY_DISP_HOR_RES * MY_DISP_VER_RES/10];
   static uint8_t buf2[MY_DISP_HOR_RES * MY_DISP_VER_RES/10];

   lv_display_set_buffers(lcd_disp, buf1, buf2, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

} 

/**********************
 *   STATIC FUNCTIONS
 **********************/

/* Callback is called when background transfer finished */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef * hspi)
{
    if(hspi == NULL || hspi->Instance != SPI1 || lcd_disp == NULL) {
        return;
    }

    /* CS high */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    lcd_bus_busy = 0;
    lv_display_flush_ready(lcd_disp);
}

/* Initialize LCD I/O bus, reset LCD */
static int32_t lcd_io_init(void)
{
    /* reset LCD */
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_RESET);
    osDelay(100);
    HAL_GPIO_WritePin(LCD_RESET_GPIO_Port, LCD_RESET_Pin, GPIO_PIN_SET);
    osDelay(100);
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
    return HAL_OK;
}

static void lcd_trace(const char * msg)
{
    if(msg == NULL) {
        return;
    }

    HAL_UART_Transmit(&huart2, (uint8_t *)msg, strlen(msg), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)"\r\n", 2, HAL_MAX_DELAY);
}

static int32_t lcd_spi_set_data_size(uint32_t data_size)
{
    if(hspi1.Init.DataSize == data_size) return HAL_OK;
    
    __HAL_SPI_DISABLE(&hspi1); // 显式关闭 SPI 确保切换安全
    hspi1.Init.DataSize = data_size;
    if(HAL_SPI_Init(&hspi1) != HAL_OK) {
        printf("lcd spi init failed: datasize=%lu\r\n", (unsigned long)data_size);
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* Platform-specific implementation of the LCD send command function. In general this should use polling transfer. */
static void lcd_send_cmd(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, const uint8_t * param,
                         size_t param_size)
{
    LV_UNUSED(disp);
    uint32_t timeout = 0xFFFF;
    while(lcd_bus_busy && timeout--);    /* wait until previous transfer is finished with timeout */
    
    if(timeout == 0) {
        lcd_trace("cmd:bus_timeout");
        lcd_bus_busy = 0; // 强制复位
    }

    if(lcd_spi_set_data_size(SPI_DATASIZE_8BIT) != HAL_OK) {
        return;
    }
    /* DCX low (command) */
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
    /* CS low */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    /* send command */
    if(HAL_SPI_Transmit(&hspi1, cmd, cmd_size, BUS_SPI1_POLL_TIMEOUT) == HAL_OK) {
        /* DCX high (data) */
        HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
        /* for short data blocks we use polling transfer */
        if(param_size > 0 && HAL_SPI_Transmit(&hspi1, (uint8_t *)param, param_size, BUS_SPI1_POLL_TIMEOUT) != HAL_OK) {
            printf("lcd param tx failed\r\n");
        }
        /* CS high */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    }
    else {
        printf("lcd cmd tx failed\r\n");
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    }
}

/* Platform-specific implementation of the LCD send color function. For better performance this should use DMA transfer.
 * In case of a DMA transfer a callback must be installed to notify LVGL about the end of the transfer.
 */
static void lcd_send_color(lv_display_t * disp, const uint8_t * cmd, size_t cmd_size, uint8_t * param,
                           size_t param_size)
{
    uint32_t timeout = 0xFFFF;
    while(lcd_bus_busy && timeout--);    /* wait until previous transfer is finished */
    if(timeout == 0) {
        lcd_trace("color:bus_timeout");
        lcd_bus_busy = 0;
    }

    if(lcd_spi_set_data_size(SPI_DATASIZE_8BIT) != HAL_OK) {
        lv_display_flush_ready(disp);
        return;
    }
    /* DCX low (command) */
    HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_RESET);
    /* CS low */
    HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
    /* send command */
    if(HAL_SPI_Transmit(&hspi1, cmd, cmd_size, BUS_SPI1_POLL_TIMEOUT) == HAL_OK) {
        /* DCX high (data) */
        HAL_GPIO_WritePin(LCD_DCX_GPIO_Port, LCD_DCX_Pin, GPIO_PIN_SET);
        /* for color data use DMA transfer */
        /* Set the SPI in 16-bit mode to match endianness */
        if(lcd_spi_set_data_size(SPI_DATASIZE_16BIT) != HAL_OK) {
            HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
            lv_display_flush_ready(disp);
            return;
        }

        if(HAL_SPI_Transmit_DMA(&hspi1, param, (uint16_t)param_size / 2U) != HAL_OK) {
            printf("lcd dma start failed\r\n");
            HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
            lv_display_flush_ready(disp);
            return;
        }

        lcd_bus_busy = 1;
        /* NOTE: CS will be reset in the transfer ready callback */
    }
    else {
        printf("lcd color cmd tx failed\r\n");
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        lv_display_flush_ready(disp);
    }
}

#else /*Enable this file at the top*/

/*This dummy typedef exists purely to silence -Wpedantic.*/
typedef int keep_pedantic_happy;
#endif
