#include <stdio.h>
#include "ral.h"
#include "opal_core.h"
#include "storage_if.h"

/* Simple test to execute Discovery0 on an OPAL device */
int main(void)
{
    struct storage_ctx hw_ctx = {
        .hw_handle = (void*)0x1234, // Mock NVMe pointer
        .protocol = 1               // Example: 1 = NVMe
    };

    printf("Starting FreeRTOS OPAL Core Test...\n");

    /* Initialize the OPAL core. This allocates memory via RAL and tests connection via Discovery0 */
    struct opal_dev *dev = init_opal_dev(&hw_ctx, storage_sec_send_recv);
    if (!dev) {
        printf("Failed to initialize OPAL device. (Expected if storage interface doesn't mock responses)\n");
        return -1;
    }

    printf("OPAL Device initialized successfully!\n");

    /* Example: Setting up a user session to lock a range */
    // opal_lock_unlock(dev, ...);

    free_opal_dev(dev);
    printf("Test complete!\n");
    return 0;
}
