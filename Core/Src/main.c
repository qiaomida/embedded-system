/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body with UART DMA implementation
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "cmsis_os.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
#include "pid.h"
#include "key.h"
#include "control.h"
#include "params_store.h"
#include "stdlib.h"
#include "lv_port_lcd_stm32.h"
#include "lvgl.h"
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

/* USER CODE BEGIN PV */
uint16_t pwm_value = 0;
int8_t direction = 1;
uint16_t adc_values[2]; // [0]:Vref, [1]:Temp
float vdd_v = 0;
float core_temp = 0;
volatile uint8_t tim4_tick = 0;
uint16_t print_counter = 0; // 用于控制打印频率的计数器
#define PRINT_INTERVAL 300 // 每300次中断打印一次
uint16_t rx;
char cmd_buffer[32];// 存放完整命令的收纳箱
uint8_t cmd_index = 0;
// DMA 相关变量
char dma_msg[64];           // 串口发送缓冲区
extern PID_TypeDef MyPID;
// 总线忙标志（必须加 volatile，否则编译器优化会死机）
volatile uint8_t lcd_bus_busy = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */
void key_scan(void);
void key_process(void);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// LVGL 延时回调适配函数：无返回值，匹配 lv_delay_cb_t 类型
 void lvgl_delay_adapt(uint32_t ms)
{
    osDelay(ms);  // 内部调用 FreeRTOS 的延时函数
}
void Debug_Print(void)// 调试信息打印
{
    if(print_counter >= PRINT_INTERVAL)
    {
        print_counter = 0;

        //printf("VDD=%.3fV Temp=%.2f\r\n", vdd_v, core_temp);
        printf("%.1f, %.1f, %.0f\r\n",
               MyPID.Target, core_temp, MyPID.Output/20.0f); // 输出目标温度、当前温度和 PWM 占空比（0-50）
    }
}

void key_process(void)//长按处理
{
    if(key_up.state)
    {
        if(key_up.press_counter == 1)
            MyPID.Target += 1;

        if(key_up.press_counter > 50 && key_up.press_counter % 5 == 0)
            MyPID.Target += 1;
            Params_Save(); // 每次修改参数后保存到 Flash
    }

    if(key_down.state)
    {
        if(key_down.press_counter == 1)
            MyPID.Target -= 1;

        if(key_down.press_counter > 50 && key_down.press_counter % 5 == 0)
            MyPID.Target -= 1;
            Params_Save(); // 每次修改参数后保存到 Flash
    }
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  PID_Init(&MyPID);
  printf("Params Load\r\n");
  //Params_Load(); // 加载参数到 MyPID
  printf("TIM/ADC Start\r\n");
  /* TIM4 is used for Encoder Interface */
  HAL_StatusTypeDef adc_status = HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 2);
  if (adc_status != HAL_OK) {
    printf("ADC DMA Start Failed: %d\r\n", adc_status);
    Error_Handler();
  } 
  printf("ADC DMA Started\r\n");
  //printf("LV_MEM_SIZE = %d\r\n", LV_MEM_SIZE);
  lv_init();
  lv_delay_set_cb(lvgl_delay_adapt);
  lv_tick_set_cb(HAL_GetTick);
  printf("LVGL initialized\r\n");
  HAL_UART_Receive_IT(&huart2,(uint8_t *)&rx,1);
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_BYPASS;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
int fputc(int ch, FILE *f) {
    /* 请确认这里的 UART 实例是你实际连接到 PC 的串口。
       例如 Nucleo/Discovery 板通常使用 USART2 (PA2/PA3)，
       如果你使用的是 USART1/USART3 等，请把 &huart2 替换为对应的句柄。 */
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;



}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        tim4_tick = 1;
    }
      /* 1. timebase) */
  if (htim->Instance == TIM5)
  {
    HAL_IncTick();
  }
}
// 如果你想在发送完成后做点什么，可以取消下面函数的注�?

// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
// {
//     if(huart->Instance == USART2)
//     {
//         if(rx=='+')
//         {MyPID.Target++;}
//         else if (rx=='-')
//         { MyPID.Target--; }
//         else if (rx=='S')
//         {
//             Params_Save(); // 接收到 'S' 时保存参数到 Flash
//             printf(">> Quick Saved!\r\n");
//         }
//         else if (rx == 'X')
//         {printf(">> Target: %.1f, Kp: %.2f, Ki: %.2f, Kd: %.2f\r\n", MyPID.Target, MyPID.Kp, MyPID.Ki, MyPID.Kd);}
//         // B. 处理复杂指令 (如 P10.5, T50, 需要回车触发)
//         else if (rx == '\n' || rx == '\r') 
//         {
//             if (cmd_index > 0) 
//             {
//                 cmd_buffer[cmd_index] = '\0'; // 闭合字符串
                
//                 if (cmd_buffer[0] == 'P') {
//                     MyPID.Kp = atof(&cmd_buffer[1]);
//                     printf(">> Kp: %.2f\r\n", MyPID.Kp);
//                 }
//                 else if (cmd_buffer[0] == 'I') {
//                     MyPID.Ki = atof(&cmd_buffer[1]);
//                     printf(">> Ki: %.2f\r\n", MyPID.Ki);
//                 }
//                 else if (cmd_buffer[0] == 'T') {
//                     MyPID.Target = atof(&cmd_buffer[1]);
//                     printf(">> Target: %.1f\r\n", MyPID.Target);
//                 }
//                 else if (strcmp(cmd_buffer, "SAVE") == 0) {
//                     Params_Save();
//                     printf(">> Full Params Saved!\r\n");
//                 }
//                else if (cmd_buffer[0] == 'D') {
//                     MyPID.Kd = atof(&cmd_buffer[1]);
//                     printf(">> Kd: %.2f\r\n", MyPID.Kd);
//                 } 
//                 cmd_index = 0; // 重置缓冲区
//             }
//         }
//         // C. 如果不是以上字符，也不是回车，就存入缓冲区待命
//         else 
//         {
//             if (cmd_index < 31) {
//                 cmd_buffer[cmd_index++] = (char)rx;
//             }
//         }


        
//         // DMA 接收完成后的回调，可以在这里翻转一?LED 状?
//         HAL_UART_Receive_IT(&huart2,(uint8_t *)&rx,1); // 继续接收下一个字节
    
// }
//     }

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM5 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
