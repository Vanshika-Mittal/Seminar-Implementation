#ifndef SEMPHR_H
#define SEMPHR_H
#include "FreeRTOS.h"
#include <pthread.h>
typedef struct { pthread_mutex_t mtx; } *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateMutex(void) {
    SemaphoreHandle_t sem = malloc(sizeof(*sem));
    pthread_mutex_init(&sem->mtx, NULL);
    return sem;
}
static inline int xSemaphoreTake(SemaphoreHandle_t sem, uint32_t ticks) {
    return pthread_mutex_lock(&sem->mtx) == 0;
}
static inline int xSemaphoreGive(SemaphoreHandle_t sem) {
    return pthread_mutex_unlock(&sem->mtx) == 0;
}
static inline void vSemaphoreDelete(SemaphoreHandle_t sem) {
    pthread_mutex_destroy(&sem->mtx);
    free(sem);
}
#endif
