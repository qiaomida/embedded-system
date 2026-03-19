#include "control.h"
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

    //_HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_1,(uint32_t)MyPID.Output);
}