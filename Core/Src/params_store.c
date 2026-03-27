#include "params_store.h"
#include "pid.h"
#include <string.h>

// STM32F411 Sector 7 地址
#define FLASH_USER_ADDR      ((uint32_t)0x08060000)
#define PARAMS_MAGIC         0x5A5A1234

extern PID_TypeDef MyPID; // 温控模块使用的 PID 结构体

SysParams_t g_sys_params;

/**
 * @brief 将 Flash 参数加载到内存
 */
void Params_Load(void) {
    // 1. 从 Flash 地址直接读取数据
    memcpy(&g_sys_params, (void*)FLASH_USER_ADDR, sizeof(SysParams_t));

    // 2. 校验数据
    if (g_sys_params.magic == PARAMS_MAGIC) {
        // 匹配成功，更新 PID 参数
        MyPID.Target = g_sys_params.target_temp;
        MyPID.Kp = g_sys_params.kp;
        MyPID.Ki = g_sys_params.ki;
        MyPID.Kd = g_sys_params.kd;
    } else {
        // 校验失败，初始化并使用默认值
        MyPID.Target = 35.0f;
        MyPID.Kp = 2.0f;
        MyPID.Ki = 0.5f;
        MyPID.Kd = 0.1f;
        // 自动写入一个默认值 (暂时注释掉，防止 Boot 卡死)
        // Params_Save(); 
    }
}

/**
 * @brief 将当前内存的 PID 参数写入 Flash
 */
void Params_Save(void) {
    // 1. 准备数据结构
    g_sys_params.magic = PARAMS_MAGIC;
    g_sys_params.target_temp = MyPID.Target;
    g_sys_params.kp = MyPID.Kp;
    g_sys_params.ki = MyPID.Ki;
    g_sys_params.kd = MyPID.Kd;

    // 2. 解锁 Flash
    HAL_FLASH_Unlock();

    // 3. 擦除目标扇区 (Sector 7)
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error;
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.Sector = FLASH_SECTOR_7;
    erase_init.NbSectors = 1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 3.3V 电压

    if (HAL_FLASHEx_Erase(&erase_init, &sector_error) == HAL_OK) {
        // 4. 循环写数据 (以 Word 为单位)
        uint32_t *p_data = (uint32_t *)&g_sys_params;
        for (uint32_t i = 0; i < (sizeof(SysParams_t) / 4); i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_USER_ADDR + (i * 4), p_data[i]);
        }
    }

    // 5. 加锁
    HAL_FLASH_Lock();
}