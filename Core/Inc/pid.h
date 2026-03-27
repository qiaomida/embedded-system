#ifndef __PID_H
#define __PID_H

typedef struct {
    float Target;        // 目标值
    float Kp, Ki, Kd;    // PID 三参数
    float Error;         // 当前偏差 (Target - Current)
    float Last_Error;    // 上次偏差
    float Integral;      // 偏差累加
    float Output;        // 最终输出 (0-1000)
} PID_TypeDef;

extern PID_TypeDef MyPID;

void PID_Init(PID_TypeDef *pid);

#endif // __PID_H