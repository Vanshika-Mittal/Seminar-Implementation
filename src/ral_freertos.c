#include "ral.h"
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <stdarg.h>
#include <stdio.h>

/* Memory Allocation */
void *ral_malloc(size_t size) {
    return pvPortMalloc(size);
}

void *ral_calloc(size_t nmemb, size_t size) {
    void *ptr = pvPortMalloc(nmemb * size);
    if (ptr) {
        memset(ptr, 0, nmemb * size);
    }
    return ptr;
}

void ral_free(void *ptr) {
    vPortFree(ptr);
}

/* Synchronization */
struct ral_mutex {
    SemaphoreHandle_t sem;
};

ral_mutex_t *ral_mutex_create(void) {
    ral_mutex_t *mutex = (ral_mutex_t *)pvPortMalloc(sizeof(ral_mutex_t));
    if (mutex) {
        mutex->sem = xSemaphoreCreateMutex();
        if (!mutex->sem) {
            vPortFree(mutex);
            return NULL;
        }
    }
    return mutex;
}

void ral_mutex_lock(ral_mutex_t *mutex) {
    if (mutex) {
        xSemaphoreTake(mutex->sem, portMAX_DELAY);
    }
}

void ral_mutex_unlock(ral_mutex_t *mutex) {
    if (mutex) {
        xSemaphoreGive(mutex->sem);
    }
}

void ral_mutex_destroy(ral_mutex_t *mutex) {
    if (mutex) {
        vSemaphoreDelete(mutex->sem);
        vPortFree(mutex);
    }
}

/* Logging */
void ral_log_debug(const char *fmt, ...) {
    /* Use a standard printf, or replace with FreeRTOS logging/UART */
    va_list args;
    va_start(args, fmt);
    printf("OPAL [DEBUG]: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void ral_log_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf("OPAL [ERROR]: ");
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

/* Timing */
void ral_msleep(uint32_t ms) {
    vTaskDelay(pdMS_TO_TICKS(ms));
}
