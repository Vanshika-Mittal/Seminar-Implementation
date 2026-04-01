#ifndef FREERTOS_H
#define FREERTOS_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define pvPortMalloc malloc
#define vPortFree free
#define configTICK_RATE_HZ 1000
#define portMAX_DELAY 0xFFFFFFFF
#define pdMS_TO_TICKS(x) ((x) * configTICK_RATE_HZ / 1000)
#endif
