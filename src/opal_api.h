/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright © 2016 Intel Corporation
 *
 * Authors:
 *    Rafael Antognolli <rafael.antognolli@intel.com>
 *    Scott  Bauer      <scott.bauer@intel.com>
 */

#ifndef _UAPI_SED_OPAL_H
#define _UAPI_SED_OPAL_H



#define OPAL_KEY_MAX 256
#define OPAL_MAX_LRS 9

enum opal_mbr {
	OPAL_MBR_ENABLE = 0x0,
	OPAL_MBR_DISABLE = 0x01,
};

enum opal_mbr_done_flag {
	OPAL_MBR_NOT_DONE = 0x0,
	OPAL_MBR_DONE = 0x01
};

enum opal_user {
	OPAL_ADMIN1 = 0x0,
	OPAL_USER1 = 0x01,
	OPAL_USER2 = 0x02,
	OPAL_USER3 = 0x03,
	OPAL_USER4 = 0x04,
	OPAL_USER5 = 0x05,
	OPAL_USER6 = 0x06,
	OPAL_USER7 = 0x07,
	OPAL_USER8 = 0x08,
	OPAL_USER9 = 0x09,
};

enum opal_lock_state {
	OPAL_RO = 0x01, /* 0001 */
	OPAL_RW = 0x02, /* 0010 */
	OPAL_LK = 0x04, /* 0100 */
};

enum opal_lock_flags {
	/* IOC_OPAL_SAVE will also store the provided key for locking */
	OPAL_SAVE_FOR_LOCK = 0x01,
};

enum opal_key_type {
	OPAL_INCLUDED = 0,	/* key[] is the key */
	OPAL_KEYRING,		/* key is in keyring */
};

struct opal_key {
	uint8_t lr;
	uint8_t key_len;
	uint8_t key_type;
	uint8_t __align[5];
	uint8_t key[OPAL_KEY_MAX];
};

enum opal_revert_lsp_opts {
	OPAL_PRESERVE = 0x01,
};

struct opal_lr_act {
	struct opal_key key;
	uint32_t sum;
	uint8_t num_lrs;
	uint8_t lr[OPAL_MAX_LRS];
	uint8_t align[2]; /* Align to 8 byte boundary */
};

struct opal_session_info {
	uint32_t sum;
	uint32_t who;
	struct opal_key opal_key;
};

struct opal_user_lr_setup {
	uint64_t range_start;
	uint64_t range_length;
	uint32_t RLE; /* Read Lock enabled */
	uint32_t WLE; /* Write Lock Enabled */
	struct opal_session_info session;
};

struct opal_lr_status {
	struct opal_session_info session;
	uint64_t range_start;
	uint64_t range_length;
	uint32_t RLE; /* Read Lock enabled */
	uint32_t WLE; /* Write Lock Enabled */
	uint32_t l_state;
	uint8_t  align[4];
};

struct opal_lock_unlock {
	struct opal_session_info session;
	uint32_t l_state;
	uint16_t flags;
	uint8_t __align[2];
};

struct opal_new_pw {
	struct opal_session_info session;

	/* When we're not operating in sum, and we first set
	 * passwords we need to set them via ADMIN authority.
	 * After passwords are changed, we can set them via,
	 * User authorities.
	 * Because of this restriction we need to know about
	 * Two different users. One in 'session' which we will use
	 * to start the session and new_userr_pw as the user we're
	 * chaning the pw for.
	 */
	struct opal_session_info new_user_pw;
};

struct opal_mbr_data {
	struct opal_key key;
	uint8_t enable_disable;
	uint8_t __align[7];
};

struct opal_mbr_done {
	struct opal_key key;
	uint8_t done_flag;
	uint8_t __align[7];
};

struct opal_shadow_mbr {
	struct opal_key key;
	const uint64_t data;
	uint64_t offset;
	uint64_t size;
};

/* Opal table operations */
enum opal_table_ops {
	OPAL_READ_TABLE,
	OPAL_WRITE_TABLE,
};

#define OPAL_UID_LENGTH 8
struct opal_read_write_table {
	struct opal_key key;
	const uint64_t data;
	const uint8_t table_uid[OPAL_UID_LENGTH];
	uint64_t offset;
	uint64_t size;
#define OPAL_TABLE_READ (1 << OPAL_READ_TABLE)
#define OPAL_TABLE_WRITE (1 << OPAL_WRITE_TABLE)
	uint64_t flags;
	uint64_t priv;
};

#define OPAL_FL_SUPPORTED		0x00000001
#define OPAL_FL_LOCKING_SUPPORTED	0x00000002
#define OPAL_FL_LOCKING_ENABLED		0x00000004
#define OPAL_FL_LOCKED			0x00000008
#define OPAL_FL_MBR_ENABLED		0x00000010
#define OPAL_FL_MBR_DONE		0x00000020
#define OPAL_FL_SUM_SUPPORTED		0x00000040

struct opal_status {
	uint32_t flags;
	uint32_t reserved;
};

/*
 * Geometry Reporting per TCG Storage OPAL SSC
 * section 3.1.1.4
 */
struct opal_geometry {
	uint8_t align;
	uint32_t logical_block_size;
	uint64_t alignment_granularity;
	uint64_t lowest_aligned_lba;
	uint8_t  __align[3];
};

struct opal_discovery {
	uint64_t data;
	uint64_t size;
};

struct opal_revert_lsp {
	struct opal_key key;
	uint32_t options;
	uint32_t __pad;
};
























#endif /* _UAPI_SED_OPAL_H */
