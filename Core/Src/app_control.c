#include "app_control.h"
#include "app_sensor.h"
#include "pid.h"
#include "stdio.h"

extern PID_TypeDef MyPID;

float core_temp = 0.0f;

void Control_Loop(void)
{
    Sensor_Update();
    float real_temp = Sensor_GetRealTemp();

    static uint8_t init_flag = 0;
    if (!init_flag) {
        core_temp = real_temp;
        init_flag = 1;
    }

    float cooling_rate = (core_temp - real_temp) * 0.01f;
    float heating_rate = MyPID.Output * 0.0005f;

    core_temp = core_temp + heating_rate - cooling_rate;

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

    static uint8_t print_div = 0;
    if (++print_div > 50) {
        printf("PID: Target=%.1f, Temp=%.2f, Output=%.0f\r\n", MyPID.Target, core_temp, MyPID.Output);
        print_div = 0;
    }
}
