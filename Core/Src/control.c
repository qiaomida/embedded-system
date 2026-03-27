#include "control.h"
void Control_Loop(void)//PID计算
{
    // 1. 获取真实温度 (作为环境基础温度)
    if (adc_values[0] > 0)
        vdd_v = 1.21f * 4095.0f / (float)adc_values[0];

    float vsense = ((float)adc_values[1] / 4095.0f) * vdd_v;
    float real_temp = ((vsense - 0.76f) / 0.0025f) + 25.0f;

    // 2. 模拟温度控制模型 (加热与自然散热)
    // 假设 core_temp 是我们正在控制的“虚拟加热块”温度
    // 初始状态下，让它等于真实芯片温度
    static uint8_t init_flag = 0;
    if(!init_flag) {
        core_temp = real_temp;
        init_flag = 1;
    }

    // 散热模型：温度会自然向环境温度(real_temp)靠拢
    float cooling_rate = (core_temp - real_temp) * 0.01f; 
    
    // 加热模型：PID 输出(0-1000)转化为加热功率
    float heating_rate = MyPID.Output * 0.0005f; 

    // 更新模拟温度
    core_temp = core_temp + heating_rate - cooling_rate;

    // 3. PID 计算
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
    
    // 降低打印频率防止刷屏
    static uint8_t print_div = 0;
    if(++print_div > 50) {
        printf("PID: Target=%.1f, Temp=%.2f, Output=%.0f\r\n", MyPID.Target, core_temp, MyPID.Output);
        print_div = 0;
    }
}