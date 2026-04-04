# TCG OPAL to FreeRTOS Port (Phase 2: Native OS-Agnostic Core)

## Purpose
This directory contains the completed **Phase 2** refactoring of the Linux TCG OPAL security driver (`sed-opal.c`) into a native, standalone C library targeted for embedded RTOS environments (specifically FreeRTOS).

Unlike Phase 1, which relied on a Python pre-processor script and a mock compatibility header (`linux_xcompat.h`) to bridge Linux kernel mappings to standard C, this Phase 2 core has been **fully rewritten natively**. The code here requires no Python manipulation, contains no kernel macros, and can be built strictly on a standard C11 compiler alongside the provided RTOS Abstraction Layer (RAL).

## Key Improvements over Phase 1
- **Removed `linux_xcompat.h`**: The fragile `#define` impedance matching layer is entirely gone.
- **Native Datatypes**: Linux specific types (`u8`, `u16`, `__be32`, `size_t`) were systematically converted to standard C99 typenames (`uint8_t`, `uint16_t`, `uint32_t`) across all header and source files.
- **Native Memory & Mutex Handling**: Instead of aliasing `kmalloc` and `struct mutex`, the OPAL logic now natively calls the RTOS Abstraction Layer endpoints (`ral_malloc`, `ral_mutex_t`).
- **Standalone Linked List**: The circular doubly-linked list logic from the kernel was separated gracefully into an isolated, agnostic `util_list.h` file.
- **Removed Kernel Build Subsystem**: The `Makefile` points strictly to standard `.c` objects without executing `strip_linux_final.py`.

## Repository Structure & File Purposes

### Source Files
* **`opal_core.c`**: The definitive, natively refactored OPAL core state machine logic.
* **`opal_core.h`**: The header for allocating generic `opal_dev` wrappers and initiating sessions.
* **`opal_api.h`**, **`opal_proto.h`**: The structures describing the TCG OPAL specification tokens and objects, scrubbed of Linux annotations.
* **`util_list.h`**: Clean, standalone double-linked list macros to safely manage device states (e.g., unlocking lists).

### The RTOS Abstraction Layer (RAL)
* **`ral.h`**: Defines the required platform interfaces for Memory, Time, Logging, Errors, and Mutexes.
* **`ral_freertos.c`**: An implementation mapping `ral.h` onto the FreeRTOS API (`pvPortMalloc`, `xSemaphoreTake`, `vTaskDelay`).
* **`FreeRTOS.h`, `task.h`, `semphr.h`**: Dummy headers placed here locally to allow mock host-machine building and testing.

### Test & Storage Abstraction
* **`storage_if.c`** / **`storage_if.h`**: Mocks the generic transport endpoint. It exposes `storage_sec_send_recv` which simulates the endpoint passing the raw tokens to a physical drive via Trusted/Security Send or Receive protocols.
* **`main.c`**: Local verification script to construct an OPAL device tree and initiate the `Discovery0` protocol into the mocked storage layer.

## Compilation & Verification

To verify the integration without needing an ARM toolchain, run:
```bash
make clean
make
```

This will link a dummy `opal_test` binary using standard `gcc`. The lack of compiler warnings or undeclared identifiers confirms all kernel remnants were properly stripped.
