#ifndef DELAY_H
#define DELAY_H
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdint.h>

#define uchar unsigned char
#define uint8 unsigned char
#define uint16 unsigned short

void Delay_ms(uint16 ms);


#endif