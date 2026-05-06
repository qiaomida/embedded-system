# 06_timer_interrupt

基于 **STM32F411xE** 的嵌入式工程：FreeRTOS 调度、LVGL 图形界面、ADC+DMA 采样、PID 温控闭环，以及硬件 **RTC**（优先 **LSE 32.768 kHz**，失败时自动退回 **LSI**）。

---

## 功能概览

| 模块 | 说明 |
|------|------|
| **RTOS** | FreeRTOS（CMSIS-RTOS v2），`configTICK_RATE_HZ = 1000` |
| **UI** | LVGL，SPI 屏端口见 `lv_port_lcd_stm32` |
| **采样** | ADC1 + DMA，内部参考与芯片温度通道，计算 `Vdd` / `core_temp` |
| **控制** | 位置式 PID（`pid.c`），PWM/加热相关输出与 UI 联动 |
| **定时** | TIM4 等周期任务；HAL 1 ms 节拍使用 **TIM5**（非 SysTick） |
| **RTC** | 日期时间界面（`freertos.c`），时钟源：**LSE** + 预分频 `127/255`；无 LSE 时用 **LSI** + `127/249` |
| **调试** | USART2 重定向 `printf`，启动打印与运行期日志 |
| **其它** | 蜂鸣器、按键扫描、番茄钟等 UI 逻辑 |

---

## 系统时钟（摘要）

- **SYSCLK**：PLL，典型配置为 **100 MHz**（以 `SystemClock_Config()` 为准）。
- **HSE**：工程中为 **`RCC_HSE_BYPASS`**（8 MHz 外部时钟输入，常见于 Nucleo 类板）；若使用无源晶振需改为 `RCC_HSE_ON` 并核对 `HSE_VALUE`。
- **LSE**：为 RTC 提供 **32.768 kHz**；`HAL_RCC_OscConfig` 若因 LSE 起振失败而返回错误，会 **关闭 LSE 后重试**，主系统仍可启动，RTC 侧通过 `LSERDY` 选择 LSE 或 LSI。
- **备份域**：`HAL_RTC_MspInit` 中调用 `HAL_PWR_EnableBkUpAccess()`，便于 RTC/备份寄存器访问。

### 如何确认 RTC 已稳定使用 LSE

1. 上电后 **`LSERDY == 1`**（有外接 32.768 kHz 且硬件正常时）。
2. 读 `RCC->BDCR` 中 **RTCSEL**，应为 **LSE**。
3. 与标准时间比对，长时间（数小时～24 h）误差应在 **秒级**，不应再出现约 **1 分钟/小时** 量级（该量级多见于误用 LSI 作 RTC 时钟）。

---

## 目录结构（核心）

```
Core/Src/          应用与 Cube 生成外设：main, freertos, rtc, adc, tim, usart…
Core/Inc/
Drivers/           STM32F4 HAL / CMSIS
Middlewares/       FreeRTOS
lvgl/              LVGL 源码与配置
GCC/               FreeRTOS 移植（ARM_CM4F）
startup_stm32f411xe.s
CMakeLists.txt     CMake 工程入口（项目名称可能仍为历史命名 05_dma，以文件为准）
```

---

## 构建与烧录

1. **STM32CubeIDE / Keil / IAR**  
   按你日常使用的工具链打开工程并编译烧录即可（若从 CubeMX 重新生成代码，注意保留 `USER CODE` 段修改）。

2. **CMake（若已配置 `cmake/stm32cubemx` 子工程）**  ![Uploading IMG_20260326_171555.jpg…]()

   - 需本机安装 **ARM GCC**、与 STM32 CMake 插件/Cube 导出结构一致。  
   - 示例（具体以你环境为准）：
     ```bash
     cmake -B build -G "Ninja" -DCMAKE_TOOLCHAIN_FILE=<你的 toolchain 文件>
     cmake --build build
     ```

3. **固件与芯片**  
   链接脚本与启动文件针对 **STM32F411xE**；更换型号时需同步修改启动文件、链接脚本与 HAL 器件宏。

---

## 串口

- 默认 **`printf` → USART2**（见 `main.c` 中 `fputc` 与 `MX_USART2_UART_Init`）。
- 上电可看到如 `UART_DIRECT_OK` 等启动信息（具体以当前固件打印为准）。

---

## 已知注意点

- **`MX_RTC_Init`** 中若每次上电都执行 `HAL_RTC_SetTime` / `SetDate`，会覆盖掉电保持的时间；若需保留备份电池维持的时钟，应增加备份寄存器“已初始化”判断后再决定是否写入默认时间。
- **HSE BYPASS** 与板级硬件必须一致；**LSE** 为无源晶振时用 `RCC_LSE_ON`，有源钟输入需 `RCC_LSE_BYPASS`。
- 工程中 **`.ioc`** 可能位于其它路径或历史命名（如 `05_dma.ioc`），以实际 Cube 工程为准。

---

## 许可证说明

- ST HAL、CMSIS、FreeRTOS、LVGL 等遵循各自仓库许可证；应用层代码请以你的发布策略为准。
<img width="4096" height="3072" alt="IMG_20260506_143103" src="https://github.com/user-attachments/assets/354d8b2d-6185-421a-95f2-ddf2417c2a68" />![Uploading IMG_20260326_171526.jpg…]()<img width="4096" height="3072" alt="IMG_20260506_143039" src="https://github.com/user-attachments/assets/3466cb8d-d82c-4233-bb27-38e972468b4d" />
