#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include <stdint.h>

extern uint16_t adc_values[2]; /* [0]: Vref, [1]: Temp */
extern float vdd_v;

void Sensor_Update(void);
float Sensor_GetRealTemp(void);

#endif
