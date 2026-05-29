#include "app_sensor.h"

uint16_t adc_values[2];
float vdd_v = 0.0f;

void Sensor_Update(void)
{
    if (adc_values[0] > 0) {
        vdd_v = 1.21f * 4095.0f / (float)adc_values[0];
    }
}

float Sensor_GetRealTemp(void)
{
    float vsense = ((float)adc_values[1] / 4095.0f) * vdd_v;
    return ((vsense - 0.76f) / 0.0025f) + 25.0f;
}
