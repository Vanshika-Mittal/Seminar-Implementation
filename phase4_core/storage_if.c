#include "storage_if.h"
#include "mock_responses.h"
#include "ral.h"
#include <stdio.h>
#include <string.h>

/*
 * storage_sec_send_recv()
 *
 * This is the single transport callback passed into init_opal_dev().
 * It is the injection point between the OPAL protocol core and the
 * physical storage hardware.
 *
 * On a real FreeRTOS + NVMe system, the send path would call:
 *     nvme_security_send(ctx->hw_handle, spsp, secp, buffer, len)
 * and the receive path would call:
 *     nvme_security_recv(ctx->hw_handle, spsp, secp, buffer, len)
 *
 * For this off-target demo build, the receive path injects a
 * pre-built mock Discovery0 response so the OPAL core can parse
 * it and complete initialization successfully without real hardware.
 */
int storage_sec_send_recv(void *data, uint16_t spsp, uint8_t secp,
                          void *buffer, size_t len, bool send)
{
    struct storage_ctx *ctx = (struct storage_ctx *)data;

    if (!ctx || !buffer)
        return -1;

    if (send) {
        printf("[TRANSPORT][%s] Security Send  | spsp=0x%04x secp=0x%02x len=%zu\n",
               ctx->protocol == 1 ? "NVMe" : "SATA",
               spsp, secp, len);
        /*
         * REAL HW (NVMe):
         *   nvme_security_send(ctx->hw_handle, spsp, secp, buffer, len);
         * REAL HW (SATA):
         *   ata_trusted_send(ctx->hw_handle, spsp, secp, buffer, len);
         */
    } else {
        printf("[TRANSPORT][%s] Security Recv  | spsp=0x%04x secp=0x%02x len=%zu\n",
               ctx->protocol == 1 ? "NVMe" : "SATA",
               spsp, secp, len);
        /*
         * REAL HW (NVMe):
         *   nvme_security_recv(ctx->hw_handle, spsp, secp, buffer, len);
         * REAL HW (SATA):
         *   ata_trusted_recv(ctx->hw_handle, spsp, secp, buffer, len);
         *
         * MOCK: inject a valid Discovery0 response so the OPAL core
         * can parse feature descriptors and complete init_opal_dev().
         */
        if (secp == 0x01) {
            mock_fill_discovery0((uint8_t *)buffer, len);
        }
        /* secp == 0x00 is a ComID management query — zeroed buffer is fine */
    }

    return 0;
}