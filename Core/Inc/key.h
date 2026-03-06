#ifndef __KEY_H
#define __KEY_H

#include "main.h"

typedef struct 
{
  uint16_t state;
  uint16_t press_counter;
}Key_TypeDef;

extern Key_TypeDef key_up;
extern Key_TypeDef key_down;

void key_scan(void);

#endif /* __KEY_H */