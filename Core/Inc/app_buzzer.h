#ifndef APP_BUZZER_H
#define APP_BUZZER_H

#include "main.h"

void Buzzer_On(void);
void Buzzer_Off(void);
void Buzzer_Beep(uint16_t time_ms);
void Buzzer_Init(void);

#endif
