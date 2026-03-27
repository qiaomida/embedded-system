#include "pid.h"
PID_TypeDef MyPID;

void PID_Init(PID_TypeDef *pid) {
    pid->Target = 40.0f; // 设定 40 度
    pid->Kp = 30.0f;    // 先给一个较大的 P
    pid->Ki = 0.8f;      // 小一点的 I 消除残余误差
    pid->Kd = 5.0f;     // 适中的 D 抑制震荡
    pid->Integral = 0;
    pid->Last_Error = 0;
}