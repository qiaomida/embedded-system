LD2 随着你手指按压芯片而变暗
你现在拥有了一个：

实时采样（ADC + DMA）

精准调度（TIM 中断）

算法闭环（位置式 PID）

视觉反馈（PWM 驱动 LD2）
的完整作品。
int main(void)
{
  /* MCU Configuration--------------------------------------------------------*/
  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();
  /* Configure the system clock */
  SystemClock_Config();
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM4_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  PID_Init(&MyPID);
  /* USER CODE BEGIN 2 */
 if (HAL_TIM_Base_Start_IT(&htim4) != HAL_OK) {
   Error_Handler();
 }
 if (HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 2) != HAL_OK) {
   Error_Handler();
 }
 HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);
  printf("boot ok\r\n");
  /* USER CODE END 2 */
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    if (tim4_tick)
    {
      tim4_tick = 0;
       HAL_ADC_Start_DMA(&hadc1, (uint32_t*)adc_values, 2);
 if(print_counter>=PRINT_INTERVAL)
      {
          print_counter = 0;
      if (adc_values[0] > 0) {
        vdd_v = 1.21f * 4095.0f / (float)adc_values[0];
      }

      {
        float vsense = ((float)adc_values[1] / 4095.0f) * vdd_v;
        core_temp = ((vsense - 0.76f) / 0.0025f) + 25.0f;
      }
      printf("VDD=%.3fV  Temp=%.2f\r\n", vdd_v, core_temp);
        printf("Target:%.1f | Real:%.1f | Out:%.0f\r\n", 
         MyPID.Target, core_temp, MyPID.Output);
    }
    MyPID.Error = MyPID.Target - core_temp;
        MyPID.Integral += MyPID.Error;  
        // 积分抗饱和 (重要：防止 I 项无限增大)
        if (MyPID.Integral > 150) MyPID.Integral = 150;
        if (MyPID.Integral < -150) MyPID.Integral = -150;        
        float D = MyPID.Error - MyPID.Last_Error;
        MyPID.Output = (MyPID.Kp * MyPID.Error) + 
                       (MyPID.Ki * MyPID.Integral) + 
                       (MyPID.Kd * D);
        MyPID.Last_Error = MyPID.Error;
        // 3. 输出限幅与硬件驱动
        if (MyPID.Output > 1000) MyPID.Output = 1000;
        if (MyPID.Output < 0) MyPID.Output = 0;
        __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1, (uint32_t)MyPID.Output);  
    }
  }
}
