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
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "key.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>
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
// DMA 相关变量
char dma_msg[64];           // 串口发送缓冲区
typedef struct {
    float Target;        // 目标值
    float Kp, Ki, Kd;    // PID 三参数
    float Error;         // 当前偏差 (Target - Current)
    float Last_Error;    // 上次偏差
    float Integral;      // 偏差累加
    float Output;        // 最终输出 (0-1000)
} PID_TypeDef;
PID_TypeDef MyPID;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void PID_Init(PID_TypeDef *pid) {
    pid->Target = 40.0f; // 设定 40 度
    pid->Kp = 120.0f;    // 先给一个较大的 P
    pid->Ki = 0.8f;      // 小一点的 I 消除残余误差
    pid->Kd = 30.0f;     // 适中的 D 抑制震荡
    pid->Integral = 0;
    pid->Last_Error = 0;
}
void Control_Loop(void)//PID计算
{
    if (adc_values[0] > 0)
        vdd_v = 1.21f * 4095.0f / (float)adc_values[0];

    float vsense = ((float)adc_values[1] / 4095.0f) * vdd_v;
    core_temp = ((vsense - 0.76f) / 0.0025f) + 25.0f;

    MyPID.Error = MyPID.Target - core_temp;
    MyPID.Integral += MyPID.Error;

    if (MyPID.Integral > 150) MyPID.Integral = 150;
    if (MyPID.Integral < -150) MyPID.Integral = -150;

    float D = MyPID.Error - MyPID.Last_Error;

    MyPID.Output =
        MyPID.Kp * MyPID.Error +
        MyPID.Ki * MyPID.Integral +
        MyPID.Kd * D;

    MyPID.Last_Error = MyPID.Error;

    if (MyPID.Output > 1000) MyPID.Output = 1000;
    if (MyPID.Output < 0) MyPID.Output = 0;

    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1,(uint32_t)MyPID.Output);
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
void key_process(void)
{
    if(key_up.state)
    {
        if(key_up.press_counter == 1)
            MyPID.Target += 1;

        if(key_up.press_counter > 50 && key_up.press_counter % 5 == 0)
            MyPID.Target += 1;
    }

    if(key_down.state)
    {
        if(key_down.press_counter == 1)
            MyPID.Target -= 1;

        if(key_down.press_counter > 50 && key_down.press_counter % 5 == 0)
            MyPID.Target -= 1;
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
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  PID_Init(&MyPID);
 if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) {
   Error_Handler();
 }
 if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 2) != HAL_OK) {
   Error_Handler();
 }
 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  printf("boot ok\r\n");

HAL_UART_Receive_IT(&huart2,(uint8_t *)&rx,1);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (tim4_tick)
    {
      tim4_tick = 0;
       HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 2);
        Control_Loop();
                // 2. 串口打印 (每 200ms 运行一次，降低串口负担)
        static uint16_t ui_cnt = 0;
        if (ui_cnt++ >= 20) { 
            Debug_Print();    
            ui_cnt = 0;    
  }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  /* USER CODE END 3 */
}
}
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
int fputc(int ch, FILE *f)
{
  // 使用阻塞模式发送一个字节，超时设为 HAL_MAX_DELAY 确保发送完成
  HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
  return ch;
}
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM4)
    {
        tim4_tick = 1;
        print_counter++;
       key_scan();
       key_process();
    }
}
// 如果你想在发送完成后做点什么，可以取消下面函数的注�?

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if(huart->Instance == USART2)
    {
        if(rx=='+')
        {MyPID.Target++;}
        else if (rx=='-')
        { MyPID.Target--; }
        
        // DMA 接收完成后的回调，可以在这里翻转一?LED 状?
        HAL_UART_Receive_IT(&huart2,(uint8_t *)&rx,1); // 继续接收下一个字节
    }
}

/* USER CODE END 4 */

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
