#ifndef TASK_H
#define TASK_H
#include "FreeRTOS.h"
#include <unistd.h>
static inline void vTaskDelay(uint32_t ticks) { usleep(ticks * 1000); }
#endif
