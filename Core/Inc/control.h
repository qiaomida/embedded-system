#ifndef __CONTROL_H
#define __CONTROL_H
#include "pid.h"
#include "key.h"
#include "main.h"
#include "adc.h"
#include "tim.h"
extern uint16_t adc_values[2]; // [0]:Vref, [1]:Temp
extern float vdd_v; 
extern float core_temp;
void Control_Loop(void);

#endif // __CONTROL_H