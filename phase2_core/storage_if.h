#ifndef STORAGE_IF_H
#define STORAGE_IF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "opal_core.h"

/* Generic storage context */
struct storage_ctx {
    void *hw_handle;
    int protocol; /* e.g., NVMe, SATA, SPI */
};

/* The transport implementation that satisfies OPAL's sec_send_recv callback */
int storage_sec_send_recv(void *data, uint16_t spsp, uint8_t secp, void *buffer, size_t len, bool send);

#endif /* STORAGE_IF_H */
