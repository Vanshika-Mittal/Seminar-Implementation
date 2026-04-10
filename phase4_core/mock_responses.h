#ifndef MOCK_RESPONSES_H
#define MOCK_RESPONSES_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

/*
 * Builds a minimal but valid TCG OPAL Level 0 Discovery response.
 * 
 * Layout (all multi-byte fields are big-endian per TCG spec):
 *   Bytes 0-3:   d0_header.length = total feature bytes that follow (big-endian)
 *   Bytes 4-5:   d0_header.revision = 1
 *   Bytes 6-47:  reserved (zeroed)
 *   Bytes 48+:   feature descriptors
 *
 * Each feature descriptor:
 *   Bytes 0-1: feature code (big-endian)
 *   Byte  2:   version nibble
 *   Byte  3:   length of feature data that follows (NOT counting these 4 bytes)
 *   Bytes 4+:  feature-specific data
 */
static inline void mock_fill_discovery0(uint8_t *buf, size_t buflen)
{
    memset(buf, 0, buflen);

    /*
     * d0_header.length = total number of bytes that follow the header.
     * We have 3 features x 8 bytes each = 24 bytes of feature data,
     * plus the 4-byte code/version/length prefix per feature = 12 bytes,
     * total = 36 bytes.
     * But the field counts from the FIRST byte AFTER itself, so:
     * length = (48 - 4) + feature_bytes = 44 + 36 = ... 
     *
     * Simpler: the field is the total payload size. We place features
     * starting at byte 48. We have 3 features, each 12 bytes = 36 bytes.
     * hlen = 36 (this is what opal_discovery0_end reads as hlen).
     * epos = resp + hlen, cpos = resp + sizeof(d0_header) = resp + 48.
     * For the while loop (cpos < epos) to execute, hlen must be > 48.
     * So set hlen = 48 + 36 = 84.
     */
    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x54; /* 84 decimal — covers 48-byte header + 36 bytes of features */

    buf[4] = 0x00;
    buf[5] = 0x01; /* revision = 1 */
    /* bytes 6-47 are reserved, already zeroed by memset */

    size_t pos = 48; /* features start here */

    /* ---- Feature 0x0001: TPer Feature ----
     * supported_features bit 2 (0x04) = SyncSupported
     * (opal_proto.h: TPER_SYNC_SUPPORTED = 0x04 or 0x01 depending on version)
     * Use 0x01 to match TPER_SYNC_SUPPORTED as defined in opal_proto.h
     */
    buf[pos++] = 0x00; buf[pos++] = 0x01; /* feature code FC_TPER */
    buf[pos++] = 0x10;                     /* version = 1, reserved */
    buf[pos++] = 0x04;                     /* length of data that follows = 4 */
    buf[pos++] = 0x01;                     /* supported_features: TPER_SYNC_SUPPORTED */
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;

    /* ---- Feature 0x0002: Locking Feature ----
     * bit 0 = LockingSupported, bit 1 = LockingEnabled
     */
    buf[pos++] = 0x00; buf[pos++] = 0x02; /* feature code FC_LOCKING */
    buf[pos++] = 0x10;
    buf[pos++] = 0x04;
    buf[pos++] = 0x03; /* LockingSupported | LockingEnabled */
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;

    /* ---- Feature 0x0200: OPAL SSC v2.00 ----
     * baseComID = 0x0001 (big-endian), numComIDs = 1
     */
    buf[pos++] = 0x02; buf[pos++] = 0x00; /* feature code FC_OPALV200 */
    buf[pos++] = 0x10;
    buf[pos++] = 0x0C;                     /* length = 12 bytes of data follow */
    buf[pos++] = 0x00; buf[pos++] = 0x01; /* baseComID = 1 (big-endian) */
    buf[pos++] = 0x00; buf[pos++] = 0x01; /* numComIDs = 1 (big-endian) */
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    /* 4 more bytes padding to reach length=12 */
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
    buf[pos++] = 0x00;
}

#endif /* MOCK_RESPONSES_H */