#include <stddef.h>
#include <stdint.h>
#ifndef LINUX_OPAL_H
#define LINUX_OPAL_H
#include "opal_api.h"
#include <stdbool.h>
#include <stdint.h>
struct opal_dev;
typedef int (sec_send_recv)(void *data, uint16_t spsp, uint8_t secp, void *buffer, size_t len, bool send);
void free_opal_dev(struct opal_dev *dev);
struct opal_dev *init_opal_dev(void *data, sec_send_recv *send_recv);
#endif /* LINUX_OPAL_H */
