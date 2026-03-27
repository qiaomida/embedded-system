#ifndef __PARAMS_STORE_H
#define __PARAMS_STORE_H

#include "main.h"

// 定义需要存储的结构体
typedef struct {
    uint32_t magic;      // 校验幻数
    float target_temp;   // 目标温度
    float kp;            // P 参数
    float ki;            // I 参数
    float kd;            // D 参数
    uint32_t reserved[3]; // 预留空间，凑齐 32 字节对齐
} SysParams_t;

// 函数声明
void Params_Load(void);   // 开机加载
void Params_Save(void);   // 立即保存

#endif