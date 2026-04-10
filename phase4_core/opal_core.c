#include <stddef.h>
#include <string.h>
#include "ral.h"
#include "util_list.h"
typedef intptr_t ssize_t;

static inline int fls64(uint64_t x)
{
	int r = 64;
	if (!x) return 0;
	if (!(x & 0xffffffff00000000ull)) {
		x <<= 32;
		r -= 32;
	}
	if (!(x & 0xffff000000000000ull)) {
		x <<= 16;
		r -= 16;
	}
	if (!(x & 0xff00000000000000ull)) {
		x <<= 8;
		r -= 8;
	}
	if (!(x & 0xf000000000000000ull)) {
		x <<= 4;
		r -= 4;
	}
	if (!(x & 0xc000000000000000ull)) {
		x <<= 2;
		r -= 2;
	}
	if (!(x & 0x8000000000000000ull)) {
		x <<= 1;
		r -= 1;
	}
	return r;
}

#include "opal_api.h"
#include "opal_core.h"
#include "opal_proto.h"
// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright © 2016 Intel Corporation
 *
 * Authors:
 *    Scott  Bauer      <scott.bauer@intel.com>
 *    Rafael Antognolli <rafael.antognolli@intel.com>
 */

#define pr_fmt(fmt) KBUILD_MODNAME ":OPAL: " fmt

// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed
// removed

#include "opal_proto.h"

#define IO_BUFFER_LENGTH 2048
#define MAX_TOKS 64

/* Number of bytes needed by cmd_finalize. */
#define CMD_FINALIZE_BYTES_NEEDED 7



struct opal_step {
	int (*fn)(struct opal_dev *dev, void *data);
	void *data;
};
typedef int (cont_fn)(struct opal_dev *dev);

enum opal_atom_width {
	OPAL_WIDTH_TINY,
	OPAL_WIDTH_SHORT,
	OPAL_WIDTH_MEDIUM,
	OPAL_WIDTH_LONG,
	OPAL_WIDTH_TOKEN
};

/*
 * On the parsed response, we don't store again the toks that are already
 * stored in the response buffer. Instead, for each token, we just store a
 * pointer to the position in the buffer where the token starts, and the size
 * of the token in bytes.
 */
struct opal_resp_tok {
	const uint8_t *pos;
	size_t len;
	enum opal_response_token type;
	enum opal_atom_width width;
	union {
		uint64_t u;
		int64_t s;
	} stored;
};

/*
 * From the response header it's not possible to know how many tokens there are
 * on the payload. So we hardcode that the maximum will be MAX_TOKS, and later
 * if we start dealing with messages that have more than that, we can increase
 * this number. This is done to avoid having to make two passes through the
 * response, the first one counting how many tokens we have and the second one
 * actually storing the positions.
 */
struct parsed_resp {
	int num;
	struct opal_resp_tok toks[MAX_TOKS];
};

struct opal_dev {
	uint32_t flags;

	void *data;
	sec_send_recv *send_recv;

	ral_mutex_t *dev_lock;
	uint16_t comid;
	uint32_t hsn;
	uint32_t tsn;
	uint64_t align; /* alignment granularity */
	uint64_t lowest_lba;
	uint32_t logical_block_size;
	uint8_t  align_required; /* ALIGN: 0 or 1 */

	size_t pos;
	uint8_t *cmd;
	uint8_t *resp;

	struct parsed_resp parsed;
	size_t prev_d_len;
	void *prev_data;

	struct list_head unlk_lst;
};


static const uint8_t opaluid[][OPAL_UID_LENGTH] = {
	/* users */
	[OPAL_SMUID_UID] =
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff },
	[OPAL_THISSP_UID] =
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_ADMINSP_UID] =
		{ 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_LOCKINGSP_UID] =
		{ 0x00, 0x00, 0x02, 0x05, 0x00, 0x00, 0x00, 0x02 },
	[OPAL_ENTERPRISE_LOCKINGSP_UID] =
		{ 0x00, 0x00, 0x02, 0x05, 0x00, 0x01, 0x00, 0x01 },
	[OPAL_ANYBODY_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_SID_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x06 },
	[OPAL_ADMIN1_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0x00, 0x01 },
	[OPAL_USER1_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x03, 0x00, 0x01 },
	[OPAL_USER2_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x03, 0x00, 0x02 },
	[OPAL_PSID_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x01, 0xff, 0x01 },
	[OPAL_ENTERPRISE_BANDMASTER0_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x80, 0x01 },
	[OPAL_ENTERPRISE_ERASEMASTER_UID] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x84, 0x01 },

	/* tables */
	[OPAL_TABLE_TABLE] =
		{ 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_LOCKINGRANGE_GLOBAL] =
		{ 0x00, 0x00, 0x08, 0x02, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_LOCKINGRANGE_ACE_START_TO_KEY] =
		{ 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0xD0, 0x01 },
	[OPAL_LOCKINGRANGE_ACE_RDLOCKED] =
		{ 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0xE0, 0x01 },
	[OPAL_LOCKINGRANGE_ACE_WRLOCKED] =
		{ 0x00, 0x00, 0x00, 0x08, 0x00, 0x03, 0xE8, 0x01 },
	[OPAL_MBRCONTROL] =
		{ 0x00, 0x00, 0x08, 0x03, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_MBR] =
		{ 0x00, 0x00, 0x08, 0x04, 0x00, 0x00, 0x00, 0x00 },
	[OPAL_AUTHORITY_TABLE] =
		{ 0x00, 0x00, 0x00, 0x09, 0x00, 0x00, 0x00, 0x00},
	[OPAL_C_PIN_TABLE] =
		{ 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x00},
	[OPAL_LOCKING_INFO_TABLE] =
		{ 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x01 },
	[OPAL_ENTERPRISE_LOCKING_INFO_TABLE] =
		{ 0x00, 0x00, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00 },
	[OPAL_DATASTORE] =
		{ 0x00, 0x00, 0x10, 0x01, 0x00, 0x00, 0x00, 0x00 },

	/* C_PIN_TABLE object ID's */
	[OPAL_C_PIN_MSID] =
		{ 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x84, 0x02},
	[OPAL_C_PIN_SID] =
		{ 0x00, 0x00, 0x00, 0x0B, 0x00, 0x00, 0x00, 0x01},
	[OPAL_C_PIN_ADMIN1] =
		{ 0x00, 0x00, 0x00, 0x0B, 0x00, 0x01, 0x00, 0x01},

	/* half UID's (only first 4 bytes used) */
	[OPAL_HALF_UID_AUTHORITY_OBJ_REF] =
		{ 0x00, 0x00, 0x0C, 0x05, 0xff, 0xff, 0xff, 0xff },
	[OPAL_HALF_UID_BOOLEAN_ACE] =
		{ 0x00, 0x00, 0x04, 0x0E, 0xff, 0xff, 0xff, 0xff },

	/* special value for omitted optional parameter */
	[OPAL_UID_HEXFF] =
		{ 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
};

/*
 * TCG Storage SSC Methods.
 * Derived from: TCG_Storage_Architecture_Core_Spec_v2.01_r1.00
 * Section: 6.3 Assigned UIDs
 */
static const uint8_t opalmethod[][OPAL_METHOD_LENGTH] = {
	[OPAL_PROPERTIES] =
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x01 },
	[OPAL_STARTSESSION] =
		{ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x02 },
	[OPAL_REVERT] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x02, 0x02 },
	[OPAL_ACTIVATE] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x02, 0x03 },
	[OPAL_EGET] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x06 },
	[OPAL_ESET] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x07 },
	[OPAL_NEXT] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x08 },
	[OPAL_EAUTHENTICATE] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0c },
	[OPAL_GETACL] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x0d },
	[OPAL_GENKEY] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x10 },
	[OPAL_REVERTSP] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x11 },
	[OPAL_GET] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x16 },
	[OPAL_SET] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x17 },
	[OPAL_AUTHENTICATE] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x00, 0x1c },
	[OPAL_RANDOM] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x06, 0x01 },
	[OPAL_ERASE] =
		{ 0x00, 0x00, 0x00, 0x06, 0x00, 0x00, 0x08, 0x03 },
};

static int end_opal_session_error(struct opal_dev *dev);
static int opal_discovery0_step(struct opal_dev *dev);

struct opal_suspend_data {
	struct opal_lock_unlock unlk;
	uint8_t lr;
	struct list_head node;
};

/*
 * Derived from:
 * TCG_Storage_Architecture_Core_Spec_v2.01_r1.00
 * Section: 5.1.5 Method Status Codes
 */
static const char * const opal_errors[] = {
	"Success",
	"Not Authorized",
	"Unknown Error",
	"SP Busy",
	"SP Failed",
	"SP Disabled",
	"SP Frozen",
	"No Sessions Available",
	"Uniqueness Conflict",
	"Insufficient Space",
	"Insufficient Rows",
	"Invalid Function",
	"Invalid Parameter",
	"Invalid Reference",
	"Unknown Error",
	"TPER Malfunction",
	"Transaction Failure",
	"Response Overflow",
	"Authority Locked Out",
};

static const char *opal_error_to_human(int error)
{
	if (error == 0x3f)
		return "Failed";

	if (error >= ARRAY_SIZE(opal_errors) || error < 0)
		return "Unknown Error";

	return opal_errors[error];
}

static void print_buffer(const uint8_t *ptr, uint32_t length)
{
#ifdef DEBUG
	print_hex_dump_bytes("OPAL: ", DUMP_PREFIX_OFFSET, ptr, length);
	RAL_DEBUG("\n");
#endif
}

/*
 * Allocate/update a SED Opal key and add it to the SED Opal keyring.
 */
static int opal_get_key(struct opal_dev *dev, struct opal_key *key) { return 0; }
static bool check_tper(const void *data)
{
	const struct d0_tper_features *tper = data;
	uint8_t flags = tper->supported_features;

	if (!(flags & TPER_SYNC_SUPPORTED)) {
		RAL_DEBUG("TPer sync not supported. flags = %d\n",
			 tper->supported_features);
		return false;
	}

	return true;
}

static bool check_lcksuppt(const void *data)
{
	const struct d0_locking_features *lfeat = data;
	uint8_t sup_feat = lfeat->supported_features;

	return !!(sup_feat & LOCKING_SUPPORTED_MASK);
}

static bool check_lckenabled(const void *data)
{
	const struct d0_locking_features *lfeat = data;
	uint8_t sup_feat = lfeat->supported_features;

	return !!(sup_feat & LOCKING_ENABLED_MASK);
}

static bool check_locked(const void *data)
{
	const struct d0_locking_features *lfeat = data;
	uint8_t sup_feat = lfeat->supported_features;

	return !!(sup_feat & LOCKED_MASK);
}

static bool check_mbrenabled(const void *data)
{
	const struct d0_locking_features *lfeat = data;
	uint8_t sup_feat = lfeat->supported_features;

	return !!(sup_feat & MBR_ENABLED_MASK);
}

static bool check_mbrdone(const void *data)
{
	const struct d0_locking_features *lfeat = data;
	uint8_t sup_feat = lfeat->supported_features;

	return !!(sup_feat & MBR_DONE_MASK);
}

static bool check_sum(const void *data)
{
	const struct d0_single_user_mode *sum = data;
	uint32_t nlo = ral_be32_to_cpu(sum->num_locking_objects);

	if (nlo == 0) {
		RAL_DEBUG("Need at least one locking object.\n");
		return false;
	}

	RAL_DEBUG("Number of locking objects: %d\n", nlo);

	return true;
}

static uint16_t get_comid_v100(const void *data)
{
	const struct d0_opal_v100 *v100 = data;

	return ral_be16_to_cpu(v100->baseComID);
}

static uint16_t get_comid_v200(const void *data)
{
	const struct d0_opal_v200 *v200 = data;

	return ral_be16_to_cpu(v200->baseComID);
}

static int opal_send_cmd(struct opal_dev *dev)
{
	return dev->send_recv(dev->data, dev->comid, TCG_SECP_01,
			      dev->cmd, IO_BUFFER_LENGTH,
			      true);
}

static int opal_recv_cmd(struct opal_dev *dev)
{
	return dev->send_recv(dev->data, dev->comid, TCG_SECP_01,
			      dev->resp, IO_BUFFER_LENGTH,
			      false);
}

static int opal_recv_check(struct opal_dev *dev)
{
	size_t buflen = IO_BUFFER_LENGTH;
	void *buffer = dev->resp;
	struct opal_header *hdr = buffer;
	int ret;

	do {
		RAL_DEBUG("Sent OPAL command: outstanding=%d, minTransfer=%d\n",
			 hdr->cp.outstandingData,
			 hdr->cp.minTransfer);

		if (hdr->cp.outstandingData == 0 ||
		    hdr->cp.minTransfer != 0)
			return 0;

		memset(buffer, 0, buflen);
		ret = opal_recv_cmd(dev);
	} while (!ret);

	return ret;
}

static int opal_send_recv(struct opal_dev *dev, cont_fn *cont)
{
	int ret;

	ret = opal_send_cmd(dev);
	if (ret)
		return ret;
	ret = opal_recv_cmd(dev);
	if (ret)
		return ret;
	ret = opal_recv_check(dev);
	if (ret)
		return ret;
	return cont(dev);
}

static void check_geometry(struct opal_dev *dev, const void *data)
{
	const struct d0_geometry_features *geo = data;

	dev->align = ral_be64_to_cpu(geo->alignment_granularity);
	dev->lowest_lba = ral_be64_to_cpu(geo->lowest_aligned_lba);
	dev->logical_block_size = ral_be32_to_cpu(geo->logical_block_size);
	dev->align_required = geo->reserved01 & 1;
}

static int execute_step(struct opal_dev *dev,
			const struct opal_step *step, size_t stepIndex)
{
	int error = step->fn(dev, step->data);

	if (error) {
		RAL_DEBUG("Step %zu (%pS) failed with error %d: %s\n",
			 stepIndex, step->fn, error,
			 opal_error_to_human(error));
	}

	return error;
}

static int execute_steps(struct opal_dev *dev,
			 const struct opal_step *steps, size_t n_steps)
{
	size_t state = 0;
	int error;

	/* first do a discovery0 */
	error = opal_discovery0_step(dev);
	if (error)
		return error;

	for (state = 0; state < n_steps; state++) {
		error = execute_step(dev, &steps[state], state);
		if (error)
			goto out_error;
	}

	return 0;

out_error:
	/*
	 * For each OPAL command the first step in steps starts some sort of
	 * session. If an error occurred in the initial discovery0 or if an
	 * error occurred in the first step (and thus stopping the loop with
	 * state == 0) then there was an error before or during the attempt to
	 * start a session. Therefore we shouldn't attempt to terminate a
	 * session, as one has not yet been created.
	 */
	if (state > 0)
		end_opal_session_error(dev);

	return error;
}

static int opal_discovery0_end(struct opal_dev *dev, void *data)
{
	struct opal_discovery *discv_out = data; /* may be NULL */
	uint8_t *buf_out;
	uint64_t len_out;
	bool found_com_id = false, supported = true, single_user = false;
	const struct d0_header *hdr = (struct d0_header *)dev->resp;
	const uint8_t *epos = dev->resp, *cpos = dev->resp;
	uint16_t comid = 0;
	uint32_t hlen = ral_be32_to_cpu(hdr->length);

	print_buffer(dev->resp, hlen);
	dev->flags &= OPAL_FL_SUPPORTED;

	if (hlen > IO_BUFFER_LENGTH - sizeof(*hdr)) {
		RAL_DEBUG("Discovery length overflows buffer (%zu+%u)/%u\n",
			 sizeof(*hdr), hlen, IO_BUFFER_LENGTH);
		return -EFAULT;
	}

	if (discv_out) {
		buf_out = (uint8_t *)(uintptr_t)discv_out->data;
		len_out = min_t(uint64_t, discv_out->size, hlen);
		if (buf_out && memcpy(buf_out, dev->resp, len_out))
			return -EFAULT;

		discv_out->size = hlen; /* actual size of data */
	}

	epos += hlen; /* end of buffer */
	cpos += sizeof(*hdr); /* current position on buffer */

	while (cpos < epos && supported) {
		const struct d0_features *body =
			(const struct d0_features *)cpos;

		switch (ral_be16_to_cpu(body->code)) {
		case FC_TPER:
			supported = check_tper(body->features);
			break;
		case FC_SINGLEUSER:
			single_user = check_sum(body->features);
			if (single_user)
				dev->flags |= OPAL_FL_SUM_SUPPORTED;
			break;
		case FC_GEOMETRY:
			check_geometry(dev, body);
			break;
		case FC_LOCKING:
			if (check_lcksuppt(body->features))
				dev->flags |= OPAL_FL_LOCKING_SUPPORTED;
			if (check_lckenabled(body->features))
				dev->flags |= OPAL_FL_LOCKING_ENABLED;
			if (check_locked(body->features))
				dev->flags |= OPAL_FL_LOCKED;
			if (check_mbrenabled(body->features))
				dev->flags |= OPAL_FL_MBR_ENABLED;
			if (check_mbrdone(body->features))
				dev->flags |= OPAL_FL_MBR_DONE;
			break;
		case FC_ENTERPRISE:
		case FC_DATASTORE:
			/* some ignored properties */
			RAL_DEBUG("Found OPAL feature description: %d\n",
				 ral_be16_to_cpu(body->code));
			break;
		case FC_OPALV100:
			comid = get_comid_v100(body->features);
			found_com_id = true;
			break;
		case FC_OPALV200:
			comid = get_comid_v200(body->features);
			found_com_id = true;
			break;
		case 0xbfff ... 0xffff:
			/* vendor specific, just ignore */
			break;
		default:
			RAL_DEBUG("OPAL Unknown feature: %d\n",
				 ral_be16_to_cpu(body->code));

		}
		cpos += body->length + 4;
	}

	if (!supported) {
		RAL_DEBUG("This device is not Opal enabled. Not Supported!\n");
		return -EOPNOTSUPP;
	}

	if (!single_user)
		RAL_DEBUG("Device doesn't support single user mode\n");


	if (!found_com_id) {
		RAL_DEBUG("Could not find OPAL comid for device. Returning early\n");
		return -EOPNOTSUPP;
	}

	dev->comid = comid;

	return 0;
}

int opal_discovery0(struct opal_dev *dev, void *data)
{
	int ret;

	memset(dev->resp, 0, IO_BUFFER_LENGTH);
	dev->comid = OPAL_DISCOVERY_COMID;
	ret = opal_recv_cmd(dev);
	if (ret)
		return ret;

	return opal_discovery0_end(dev, data);
}

static int opal_discovery0_step(struct opal_dev *dev)
{
	const struct opal_step discovery0_step = {
		opal_discovery0, NULL
	};

	return execute_step(dev, &discovery0_step, 0);
}

static size_t remaining_size(struct opal_dev *cmd)
{
	return IO_BUFFER_LENGTH - cmd->pos;
}

static bool can_add(int *err, struct opal_dev *cmd, size_t len)
{
	if (*err)
		return false;

	if (remaining_size(cmd) < len) {
		RAL_DEBUG("Error adding %zu bytes: end of buffer.\n", len);
		*err = -ERANGE;
		return false;
	}

	return true;
}

static void add_token_u8(int *err, struct opal_dev *cmd, uint8_t tok)
{
	if (!can_add(err, cmd, 1))
		return;

	cmd->cmd[cmd->pos++] = tok;
}

static void add_short_atom_header(struct opal_dev *cmd, bool bytestring,
				  bool has_sign, int len)
{
	uint8_t atom;
	int err = 0;

	atom = SHORT_ATOM_ID;
	atom |= bytestring ? SHORT_ATOM_BYTESTRING : 0;
	atom |= has_sign ? SHORT_ATOM_SIGNED : 0;
	atom |= len & SHORT_ATOM_LEN_MASK;

	add_token_u8(&err, cmd, atom);
}

static void add_medium_atom_header(struct opal_dev *cmd, bool bytestring,
				   bool has_sign, int len)
{
	uint8_t header0;

	header0 = MEDIUM_ATOM_ID;
	header0 |= bytestring ? MEDIUM_ATOM_BYTESTRING : 0;
	header0 |= has_sign ? MEDIUM_ATOM_SIGNED : 0;
	header0 |= (len >> 8) & MEDIUM_ATOM_LEN_MASK;

	cmd->cmd[cmd->pos++] = header0;
	cmd->cmd[cmd->pos++] = len;
}

static void add_token_u64(int *err, struct opal_dev *cmd, uint64_t number)
{
	size_t len;
	int msb;

	if (!(number & ~TINY_ATOM_DATA_MASK)) {
		add_token_u8(err, cmd, number);
		return;
	}

	msb = fls64(number);
	len = DIV_ROUND_UP(msb, 8);

	if (!can_add(err, cmd, len + 1)) {
		RAL_DEBUG("Error adding uint64_t: end of buffer.\n");
		return;
	}
	add_short_atom_header(cmd, false, false, len);
	while (len--)
		add_token_u8(err, cmd, number >> (len * 8));
}

static uint8_t *add_bytestring_header(int *err, struct opal_dev *cmd, size_t len)
{
	size_t header_len = 1;
	bool is_short_atom = true;

	if (len & ~SHORT_ATOM_LEN_MASK) {
		header_len = 2;
		is_short_atom = false;
	}

	if (!can_add(err, cmd, header_len + len)) {
		RAL_DEBUG("Error adding bytestring: end of buffer.\n");
		return NULL;
	}

	if (is_short_atom)
		add_short_atom_header(cmd, true, false, len);
	else
		add_medium_atom_header(cmd, true, false, len);

	return &cmd->cmd[cmd->pos];
}

static void add_token_bytestring(int *err, struct opal_dev *cmd,
				 const uint8_t *bytestring, size_t len)
{
	uint8_t *start;

	start = add_bytestring_header(err, cmd, len);
	if (!start)
		return;
	memcpy(start, bytestring, len);
	cmd->pos += len;
}

static int build_locking_range(uint8_t *buffer, size_t length, uint8_t lr)
{
	if (length > OPAL_UID_LENGTH) {
		RAL_DEBUG("Can't build locking range. Length OOB\n");
		return -ERANGE;
	}

	memcpy(buffer, opaluid[OPAL_LOCKINGRANGE_GLOBAL], OPAL_UID_LENGTH);

	if (lr == 0)
		return 0;

	buffer[5] = LOCKING_RANGE_NON_GLOBAL;
	buffer[7] = lr;

	return 0;
}

static int build_locking_user(uint8_t *buffer, size_t length, uint8_t lr)
{
	if (length > OPAL_UID_LENGTH) {
		RAL_DEBUG("Can't build locking range user. Length OOB\n");
		return -ERANGE;
	}

	memcpy(buffer, opaluid[OPAL_USER1_UID], OPAL_UID_LENGTH);

	buffer[7] = lr + 1;

	return 0;
}

static void set_comid(struct opal_dev *cmd, uint16_t comid)
{
	struct opal_header *hdr = (struct opal_header *)cmd->cmd;

	hdr->cp.extendedComID[0] = comid >> 8;
	hdr->cp.extendedComID[1] = comid;
	hdr->cp.extendedComID[2] = 0;
	hdr->cp.extendedComID[3] = 0;
}

static int cmd_finalize(struct opal_dev *cmd, uint32_t hsn, uint32_t tsn)
{
	struct opal_header *hdr;
	int err = 0;

	/*
	 * Close the parameter list opened from cmd_start.
	 * The number of bytes added must be equal to
	 * CMD_FINALIZE_BYTES_NEEDED.
	 */
	add_token_u8(&err, cmd, OPAL_ENDLIST);

	add_token_u8(&err, cmd, OPAL_ENDOFDATA);
	add_token_u8(&err, cmd, OPAL_STARTLIST);
	add_token_u8(&err, cmd, 0);
	add_token_u8(&err, cmd, 0);
	add_token_u8(&err, cmd, 0);
	add_token_u8(&err, cmd, OPAL_ENDLIST);

	if (err) {
		RAL_DEBUG("Error finalizing command.\n");
		return -EFAULT;
	}

	hdr = (struct opal_header *) cmd->cmd;

	hdr->pkt.tsn = ral_cpu_to_be32(tsn);
	hdr->pkt.hsn = ral_cpu_to_be32(hsn);

	hdr->subpkt.length = ral_cpu_to_be32(cmd->pos - sizeof(*hdr));
	while (cmd->pos % 4) {
		if (cmd->pos >= IO_BUFFER_LENGTH) {
			RAL_DEBUG("Error: Buffer overrun\n");
			return -ERANGE;
		}
		cmd->cmd[cmd->pos++] = 0;
	}
	hdr->pkt.length = ral_cpu_to_be32(cmd->pos - sizeof(hdr->cp) -
				      sizeof(hdr->pkt));
	hdr->cp.length = ral_cpu_to_be32(cmd->pos - sizeof(hdr->cp));

	return 0;
}

static const struct opal_resp_tok *response_get_token(
				const struct parsed_resp *resp,
				int n)
{
	const struct opal_resp_tok *tok;

	if (!resp) {
		RAL_DEBUG("Response is NULL\n");
		return ERR_PTR(-EINVAL);
	}

	if (n >= resp->num) {
		RAL_DEBUG("Token number doesn't exist: %d, resp: %d\n",
			 n, resp->num);
		return ERR_PTR(-EINVAL);
	}

	tok = &resp->toks[n];
	if (tok->len == 0) {
		RAL_DEBUG("Token length must be non-zero\n");
		return ERR_PTR(-EINVAL);
	}

	return tok;
}

static ssize_t response_parse_tiny(struct opal_resp_tok *tok,
				   const uint8_t *pos)
{
	tok->pos = pos;
	tok->len = 1;
	tok->width = OPAL_WIDTH_TINY;

	if (pos[0] & TINY_ATOM_SIGNED) {
		tok->type = OPAL_DTA_TOKENID_SINT;
	} else {
		tok->type = OPAL_DTA_TOKENID_UINT;
		tok->stored.u = pos[0] & 0x3f;
	}

	return tok->len;
}

static ssize_t response_parse_short(struct opal_resp_tok *tok,
				    const uint8_t *pos)
{
	tok->pos = pos;
	tok->len = (pos[0] & SHORT_ATOM_LEN_MASK) + 1;
	tok->width = OPAL_WIDTH_SHORT;

	if (pos[0] & SHORT_ATOM_BYTESTRING) {
		tok->type = OPAL_DTA_TOKENID_BYTESTRING;
	} else if (pos[0] & SHORT_ATOM_SIGNED) {
		tok->type = OPAL_DTA_TOKENID_SINT;
	} else {
		uint64_t u_integer = 0;
		ssize_t i, b = 0;

		tok->type = OPAL_DTA_TOKENID_UINT;
		if (tok->len > 9) {
			RAL_DEBUG("uint64 with more than 8 bytes\n");
			return -EINVAL;
		}
		for (i = tok->len - 1; i > 0; i--) {
			u_integer |= ((uint64_t)pos[i] << (8 * b));
			b++;
		}
		tok->stored.u = u_integer;
	}

	return tok->len;
}

static ssize_t response_parse_medium(struct opal_resp_tok *tok,
				     const uint8_t *pos)
{
	tok->pos = pos;
	tok->len = (((pos[0] & MEDIUM_ATOM_LEN_MASK) << 8) | pos[1]) + 2;
	tok->width = OPAL_WIDTH_MEDIUM;

	if (pos[0] & MEDIUM_ATOM_BYTESTRING)
		tok->type = OPAL_DTA_TOKENID_BYTESTRING;
	else if (pos[0] & MEDIUM_ATOM_SIGNED)
		tok->type = OPAL_DTA_TOKENID_SINT;
	else
		tok->type = OPAL_DTA_TOKENID_UINT;

	return tok->len;
}

static ssize_t response_parse_long(struct opal_resp_tok *tok,
				   const uint8_t *pos)
{
	tok->pos = pos;
	tok->len = ((pos[1] << 16) | (pos[2] << 8) | pos[3]) + 4;
	tok->width = OPAL_WIDTH_LONG;

	if (pos[0] & LONG_ATOM_BYTESTRING)
		tok->type = OPAL_DTA_TOKENID_BYTESTRING;
	else if (pos[0] & LONG_ATOM_SIGNED)
		tok->type = OPAL_DTA_TOKENID_SINT;
	else
		tok->type = OPAL_DTA_TOKENID_UINT;

	return tok->len;
}

static ssize_t response_parse_token(struct opal_resp_tok *tok,
				    const uint8_t *pos)
{
	tok->pos = pos;
	tok->len = 1;
	tok->type = OPAL_DTA_TOKENID_TOKEN;
	tok->width = OPAL_WIDTH_TOKEN;

	return tok->len;
}

static int response_parse(const uint8_t *buf, size_t length,
			  struct parsed_resp *resp)
{
	const struct opal_header *hdr;
	struct opal_resp_tok *iter;
	int num_entries = 0;
	int total;
	ssize_t token_length;
	const uint8_t *pos;
	uint32_t clen, plen, slen;

	if (!buf)
		return -EFAULT;

	if (!resp)
		return -EFAULT;

	hdr = (struct opal_header *)buf;
	pos = buf;
	pos += sizeof(*hdr);

	clen = ral_be32_to_cpu(hdr->cp.length);
	plen = ral_be32_to_cpu(hdr->pkt.length);
	slen = ral_be32_to_cpu(hdr->subpkt.length);
	RAL_DEBUG("Response size: cp: %u, pkt: %u, subpkt: %u\n",
		 clen, plen, slen);

	if (clen == 0 || plen == 0 || slen == 0 ||
	    slen > IO_BUFFER_LENGTH - sizeof(*hdr)) {
		RAL_DEBUG("Bad header length. cp: %u, pkt: %u, subpkt: %u\n",
			 clen, plen, slen);
		print_buffer(pos, sizeof(*hdr));
		return -EINVAL;
	}

	if (pos > buf + length)
		return -EFAULT;

	iter = resp->toks;
	total = slen;
	print_buffer(pos, total);
	while (total > 0) {
		if (pos[0] <= TINY_ATOM_BYTE) /* tiny atom */
			token_length = response_parse_tiny(iter, pos);
		else if (pos[0] <= SHORT_ATOM_BYTE) /* short atom */
			token_length = response_parse_short(iter, pos);
		else if (pos[0] <= MEDIUM_ATOM_BYTE) /* medium atom */
			token_length = response_parse_medium(iter, pos);
		else if (pos[0] <= LONG_ATOM_BYTE) /* long atom */
			token_length = response_parse_long(iter, pos);
		else if (pos[0] == EMPTY_ATOM_BYTE) /* empty atom */
			token_length = 1;
		else /* TOKEN */
			token_length = response_parse_token(iter, pos);

		if (token_length < 0)
			return token_length;

		if (pos[0] != EMPTY_ATOM_BYTE)
			num_entries++;

		pos += token_length;
		total -= token_length;
		iter++;
	}

	resp->num = num_entries;

	return 0;
}

static size_t response_get_string(const struct parsed_resp *resp, int n,
				  const char **store)
{
	uint8_t skip;
	const struct opal_resp_tok *tok;

	*store = NULL;
	tok = response_get_token(resp, n);
	if (IS_ERR(tok))
		return 0;

	if (tok->type != OPAL_DTA_TOKENID_BYTESTRING) {
		RAL_DEBUG("Token is not a byte string!\n");
		return 0;
	}

	switch (tok->width) {
	case OPAL_WIDTH_TINY:
	case OPAL_WIDTH_SHORT:
		skip = 1;
		break;
	case OPAL_WIDTH_MEDIUM:
		skip = 2;
		break;
	case OPAL_WIDTH_LONG:
		skip = 4;
		break;
	default:
		RAL_DEBUG("Token has invalid width!\n");
		return 0;
	}

	*store = tok->pos + skip;

	return tok->len - skip;
}

static uint64_t response_get_u64(const struct parsed_resp *resp, int n)
{
	const struct opal_resp_tok *tok;

	tok = response_get_token(resp, n);
	if (IS_ERR(tok))
		return 0;

	if (tok->type != OPAL_DTA_TOKENID_UINT) {
		RAL_DEBUG("Token is not unsigned int: %d\n", tok->type);
		return 0;
	}

	if (tok->width != OPAL_WIDTH_TINY && tok->width != OPAL_WIDTH_SHORT) {
		RAL_DEBUG("Atom is not short or tiny: %d\n", tok->width);
		return 0;
	}

	return tok->stored.u;
}

static bool response_token_matches(const struct opal_resp_tok *token, uint8_t match)
{
	if (IS_ERR(token) ||
	    token->type != OPAL_DTA_TOKENID_TOKEN ||
	    token->pos[0] != match)
		return false;
	return true;
}

static uint8_t response_status(const struct parsed_resp *resp)
{
	const struct opal_resp_tok *tok;

	tok = response_get_token(resp, 0);
	if (response_token_matches(tok, OPAL_ENDOFSESSION))
		return 0;

	if (resp->num < 5)
		return DTAERROR_NO_METHOD_STATUS;

	tok = response_get_token(resp, resp->num - 5);
	if (!response_token_matches(tok, OPAL_STARTLIST))
		return DTAERROR_NO_METHOD_STATUS;

	tok = response_get_token(resp, resp->num - 1);
	if (!response_token_matches(tok, OPAL_ENDLIST))
		return DTAERROR_NO_METHOD_STATUS;

	return response_get_u64(resp, resp->num - 4);
}

/* Parses and checks for errors */
static int parse_and_check_status(struct opal_dev *dev)
{
	int error;

	print_buffer(dev->cmd, dev->pos);

	error = response_parse(dev->resp, IO_BUFFER_LENGTH, &dev->parsed);
	if (error) {
		RAL_DEBUG("Couldn't parse response.\n");
		return error;
	}

	return response_status(&dev->parsed);
}

static void clear_opal_cmd(struct opal_dev *dev)
{
	dev->pos = sizeof(struct opal_header);
	memset(dev->cmd, 0, IO_BUFFER_LENGTH);
}

static int cmd_start(struct opal_dev *dev, const uint8_t *uid, const uint8_t *method)
{
	int err = 0;

	clear_opal_cmd(dev);
	set_comid(dev, dev->comid);

	add_token_u8(&err, dev, OPAL_CALL);
	add_token_bytestring(&err, dev, uid, OPAL_UID_LENGTH);
	add_token_bytestring(&err, dev, method, OPAL_METHOD_LENGTH);

	/*
	 * Every method call is followed by its parameters enclosed within
	 * OPAL_STARTLIST and OPAL_ENDLIST tokens. We automatically open the
	 * parameter list here and close it later in cmd_finalize.
	 */
	add_token_u8(&err, dev, OPAL_STARTLIST);

	return err;
}

static int start_opal_session_cont(struct opal_dev *dev)
{
	uint32_t hsn, tsn;
	int error;

	error = parse_and_check_status(dev);
	if (error)
		return error;

	hsn = response_get_u64(&dev->parsed, 4);
	tsn = response_get_u64(&dev->parsed, 5);

	if (hsn != GENERIC_HOST_SESSION_NUM || tsn < FIRST_TPER_SESSION_NUM) {
		RAL_DEBUG("Couldn't authenticate session\n");
		return -EPERM;
	}

	dev->hsn = hsn;
	dev->tsn = tsn;

	return 0;
}

static void add_suspend_info(struct opal_dev *dev,
			     struct opal_suspend_data *sus)
{
	struct opal_suspend_data *iter;

	list_for_each_entry(iter, &dev->unlk_lst, node) {
		if (iter->lr == sus->lr) {
			list_del(&iter->node);
			ral_free(iter);
			break;
		}
	}
	list_add_tail(&sus->node, &dev->unlk_lst);
}

static int end_session_cont(struct opal_dev *dev)
{
	dev->hsn = 0;
	dev->tsn = 0;

	return parse_and_check_status(dev);
}

static int finalize_and_send(struct opal_dev *dev, cont_fn cont)
{
	int ret;

	ret = cmd_finalize(dev, dev->hsn, dev->tsn);
	if (ret) {
		RAL_DEBUG("Error finalizing command buffer: %d\n", ret);
		return ret;
	}

	print_buffer(dev->cmd, dev->pos);

	return opal_send_recv(dev, cont);
}

static int generic_get_columns(struct opal_dev *dev, const uint8_t *table,
			       uint64_t start_column, uint64_t end_column)
{
	int err;

	err = cmd_start(dev, table, opalmethod[OPAL_GET]);

	add_token_u8(&err, dev, OPAL_STARTLIST);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_STARTCOLUMN);
	add_token_u64(&err, dev, start_column);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_ENDCOLUMN);
	add_token_u64(&err, dev, end_column);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_ENDLIST);

	if (err)
		return err;

	return finalize_and_send(dev, parse_and_check_status);
}

/*
 * request @column from table @table on device @dev. On success, the column
 * data will be available in dev->resp->tok[4]
 */
static int generic_get_column(struct opal_dev *dev, const uint8_t *table,
			      uint64_t column)
{
	return generic_get_columns(dev, table, column, column);
}

/*
 * see TCG SAS 5.3.2.3 for a description of the available columns
 *
 * the result is provided in dev->resp->tok[4]
 */
static int generic_get_table_info(struct opal_dev *dev, const uint8_t *table_uid,
				  uint64_t column)
{
	uint8_t uid[OPAL_UID_LENGTH];
	const unsigned int half = OPAL_UID_LENGTH_HALF;

	/* sed-opal UIDs can be split in two halves:
	 *  first:  actual table index
	 *  second: relative index in the table
	 * so we have to get the first half of the OPAL_TABLE_TABLE and use the
	 * first part of the target table as relative index into that table
	 */
	memcpy(uid, opaluid[OPAL_TABLE_TABLE], half);
	memcpy(uid + half, table_uid, half);

	return generic_get_column(dev, uid, column);
}

static int gen_key(struct opal_dev *dev, void *data)
{
	uint8_t uid[OPAL_UID_LENGTH];
	int err;

	memcpy(uid, dev->prev_data, min(sizeof(uid), dev->prev_d_len));
	ral_free(dev->prev_data);
	dev->prev_data = NULL;

	err = cmd_start(dev, uid, opalmethod[OPAL_GENKEY]);

	if (err) {
		RAL_DEBUG("Error building gen key command\n");
		return err;

	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int get_active_key_cont(struct opal_dev *dev)
{
	const char *activekey;
	size_t keylen;
	int error;

	error = parse_and_check_status(dev);
	if (error)
		return error;

	keylen = response_get_string(&dev->parsed, 4, &activekey);
	if (!activekey) {
		RAL_DEBUG("%s: Couldn't extract the Activekey from the response\n",
			 __func__);
		return OPAL_INVAL_PARAM;
	}

	dev->prev_data = ral_memdup(activekey, keylen);

	if (!dev->prev_data)
		return -ENOMEM;

	dev->prev_d_len = keylen;

	return 0;
}

static int get_active_key(struct opal_dev *dev, void *data)
{
	uint8_t uid[OPAL_UID_LENGTH];
	int err;
	uint8_t *lr = data;

	err = build_locking_range(uid, sizeof(uid), *lr);
	if (err)
		return err;

	err = generic_get_column(dev, uid, OPAL_ACTIVEKEY);
	if (err)
		return err;

	return get_active_key_cont(dev);
}

static int generic_table_write_data(struct opal_dev *dev, const uint64_t data,
				    uint64_t offset, uint64_t size, const uint8_t *uid)
{
	const uint8_t *src = (uint8_t *)(uintptr_t)data;
	uint8_t *dst;
	uint64_t len;
	size_t off = 0;
	int err;

	/* do we fit in the available space? */
	err = generic_get_table_info(dev, uid, OPAL_TABLE_ROWS);
	if (err) {
		RAL_DEBUG("Couldn't get the table size\n");
		return err;
	}

	len = response_get_u64(&dev->parsed, 4);
	if (size > len || offset > len - size) {
		RAL_DEBUG("Does not fit in the table (%llu vs. %llu)\n",
			  offset + size, len);
		return -ENOSPC;
	}

	/* do the actual transmission(s) */
	while (off < size) {
		err = cmd_start(dev, uid, opalmethod[OPAL_SET]);
		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_WHERE);
		add_token_u64(&err, dev, offset + off);
		add_token_u8(&err, dev, OPAL_ENDNAME);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_VALUES);

		/*
		 * The bytestring header is either 1 or 2 bytes, so assume 2.
		 * There also needs to be enough space to accommodate the
		 * trailing OPAL_ENDNAME (1 byte) and tokens added by
		 * cmd_finalize.
		 */
		len = min(remaining_size(dev) - (2+1+CMD_FINALIZE_BYTES_NEEDED),
			  (size_t)(size - off));
		RAL_DEBUG("Write bytes %zu+%llu/%llu\n", off, len, size);

		dst = add_bytestring_header(&err, dev, len);
		if (!dst)
			break;

		if (memcpy(dst, src + off, len)) {
			err = -EFAULT;
			break;
		}

		dev->pos += len;

		add_token_u8(&err, dev, OPAL_ENDNAME);
		if (err)
			break;

		err = finalize_and_send(dev, parse_and_check_status);
		if (err)
			break;

		off += len;
	}

	return err;
}

static int generic_lr_enable_disable(struct opal_dev *dev,
				     uint8_t *uid, bool rle, bool wle,
				     bool rl, bool wl)
{
	int err;

	err = cmd_start(dev, uid, opalmethod[OPAL_SET]);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);
	add_token_u8(&err, dev, OPAL_STARTLIST);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_READLOCKENABLED);
	add_token_u8(&err, dev, rle);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_WRITELOCKENABLED);
	add_token_u8(&err, dev, wle);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_READLOCKED);
	add_token_u8(&err, dev, rl);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_WRITELOCKED);
	add_token_u8(&err, dev, wl);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	return err;
}

static inline int enable_global_lr(struct opal_dev *dev, uint8_t *uid,
				   struct opal_user_lr_setup *setup)
{
	int err;

	err = generic_lr_enable_disable(dev, uid, !!setup->RLE, !!setup->WLE,
					0, 0);
	if (err)
		RAL_DEBUG("Failed to create enable global lr command\n");

	return err;
}

static int setup_locking_range(struct opal_dev *dev, void *data)
{
	uint8_t uid[OPAL_UID_LENGTH];
	struct opal_user_lr_setup *setup = data;
	uint8_t lr;
	int err;

	lr = setup->session.opal_key.lr;
	err = build_locking_range(uid, sizeof(uid), lr);
	if (err)
		return err;

	if (lr == 0)
		err = enable_global_lr(dev, uid, setup);
	else {
		err = cmd_start(dev, uid, opalmethod[OPAL_SET]);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_VALUES);
		add_token_u8(&err, dev, OPAL_STARTLIST);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_RANGESTART);
		add_token_u64(&err, dev, setup->range_start);
		add_token_u8(&err, dev, OPAL_ENDNAME);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_RANGELENGTH);
		add_token_u64(&err, dev, setup->range_length);
		add_token_u8(&err, dev, OPAL_ENDNAME);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_READLOCKENABLED);
		add_token_u64(&err, dev, !!setup->RLE);
		add_token_u8(&err, dev, OPAL_ENDNAME);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_WRITELOCKENABLED);
		add_token_u64(&err, dev, !!setup->WLE);
		add_token_u8(&err, dev, OPAL_ENDNAME);

		add_token_u8(&err, dev, OPAL_ENDLIST);
		add_token_u8(&err, dev, OPAL_ENDNAME);
	}
	if (err) {
		RAL_DEBUG("Error building Setup Locking range command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int response_get_column(const struct parsed_resp *resp,
			       int *iter,
			       uint8_t column,
			       uint64_t *value)
{
	const struct opal_resp_tok *tok;
	int n = *iter;
	uint64_t val;

	tok = response_get_token(resp, n);
	if (IS_ERR(tok))
		return PTR_ERR(tok);

	if (!response_token_matches(tok, OPAL_STARTNAME)) {
		RAL_DEBUG("Unexpected response token type %d.\n", n);
		return OPAL_INVAL_PARAM;
	}
	n++;

	if (response_get_u64(resp, n) != column) {
		RAL_DEBUG("Token %d does not match expected column %u.\n",
			 n, column);
		return OPAL_INVAL_PARAM;
	}
	n++;

	val = response_get_u64(resp, n);
	n++;

	tok = response_get_token(resp, n);
	if (IS_ERR(tok))
		return PTR_ERR(tok);

	if (!response_token_matches(tok, OPAL_ENDNAME)) {
		RAL_DEBUG("Unexpected response token type %d.\n", n);
		return OPAL_INVAL_PARAM;
	}
	n++;

	*value = val;
	*iter = n;

	return 0;
}

static int locking_range_status(struct opal_dev *dev, void *data)
{
	uint8_t lr_buffer[OPAL_UID_LENGTH];
	uint64_t resp;
	bool rlocked, wlocked;
	int err, tok_n = 2;
	struct opal_lr_status *lrst = data;

	err = build_locking_range(lr_buffer, sizeof(lr_buffer),
				  lrst->session.opal_key.lr);
	if (err)
		return err;

	err = generic_get_columns(dev, lr_buffer, OPAL_RANGESTART,
				  OPAL_WRITELOCKED);
	if (err) {
		RAL_DEBUG("Couldn't get lr %u table columns %d to %d.\n",
			 lrst->session.opal_key.lr, OPAL_RANGESTART,
			 OPAL_WRITELOCKED);
		return err;
	}

	/* range start */
	err = response_get_column(&dev->parsed, &tok_n, OPAL_RANGESTART,
				  &lrst->range_start);
	if (err)
		return err;

	/* range length */
	err = response_get_column(&dev->parsed, &tok_n, OPAL_RANGELENGTH,
				  &lrst->range_length);
	if (err)
		return err;

	/* RLE */
	err = response_get_column(&dev->parsed, &tok_n, OPAL_READLOCKENABLED,
				  &resp);
	if (err)
		return err;

	lrst->RLE = !!resp;

	/* WLE */
	err = response_get_column(&dev->parsed, &tok_n, OPAL_WRITELOCKENABLED,
				  &resp);
	if (err)
		return err;

	lrst->WLE = !!resp;

	/* read locked */
	err = response_get_column(&dev->parsed, &tok_n, OPAL_READLOCKED, &resp);
	if (err)
		return err;

	rlocked = !!resp;

	/* write locked */
	err = response_get_column(&dev->parsed, &tok_n, OPAL_WRITELOCKED, &resp);
	if (err)
		return err;

	wlocked = !!resp;

	/* opal_lock_state can not map 'read locked' only state. */
	lrst->l_state = OPAL_RW;
	if (rlocked && wlocked)
		lrst->l_state = OPAL_LK;
	else if (wlocked)
		lrst->l_state = OPAL_RO;
	else if (rlocked) {
		RAL_DEBUG("Can not report read locked only state.\n");
		return -EINVAL;
	}

	return 0;
}

static int start_generic_opal_session(struct opal_dev *dev,
				      enum opal_uid auth,
				      enum opal_uid sp_type,
				      const char *key,
				      uint8_t key_len)
{
	uint32_t hsn;
	int err;

	if (key == NULL && auth != OPAL_ANYBODY_UID)
		return OPAL_INVAL_PARAM;

	hsn = GENERIC_HOST_SESSION_NUM;
	err = cmd_start(dev, opaluid[OPAL_SMUID_UID],
			opalmethod[OPAL_STARTSESSION]);

	add_token_u64(&err, dev, hsn);
	add_token_bytestring(&err, dev, opaluid[sp_type], OPAL_UID_LENGTH);
	add_token_u8(&err, dev, 1);

	switch (auth) {
	case OPAL_ANYBODY_UID:
		break;
	case OPAL_ADMIN1_UID:
	case OPAL_SID_UID:
	case OPAL_PSID_UID:
		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, 0); /* HostChallenge */
		add_token_bytestring(&err, dev, key, key_len);
		add_token_u8(&err, dev, OPAL_ENDNAME);
		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, 3); /* HostSignAuth */
		add_token_bytestring(&err, dev, opaluid[auth],
				     OPAL_UID_LENGTH);
		add_token_u8(&err, dev, OPAL_ENDNAME);
		break;
	default:
		RAL_DEBUG("Cannot start Admin SP session with auth %d\n", auth);
		return OPAL_INVAL_PARAM;
	}

	if (err) {
		RAL_DEBUG("Error building start adminsp session command.\n");
		return err;
	}

	return finalize_and_send(dev, start_opal_session_cont);
}

static int start_anybodyASP_opal_session(struct opal_dev *dev, void *data)
{
	return start_generic_opal_session(dev, OPAL_ANYBODY_UID,
					  OPAL_ADMINSP_UID, NULL, 0);
}

static int start_SIDASP_opal_session(struct opal_dev *dev, void *data)
{
	int ret;
	uint8_t *key = dev->prev_data;

	if (!key) {
		const struct opal_key *okey = data;

		ret = start_generic_opal_session(dev, OPAL_SID_UID,
						 OPAL_ADMINSP_UID,
						 okey->key,
						 okey->key_len);
	} else {
		ret = start_generic_opal_session(dev, OPAL_SID_UID,
						 OPAL_ADMINSP_UID,
						 key, dev->prev_d_len);
		ral_free(key);
		dev->prev_data = NULL;
	}

	return ret;
}

static int start_admin1LSP_opal_session(struct opal_dev *dev, void *data)
{
	struct opal_key *key = data;

	return start_generic_opal_session(dev, OPAL_ADMIN1_UID,
					  OPAL_LOCKINGSP_UID,
					  key->key, key->key_len);
}

static int start_PSID_opal_session(struct opal_dev *dev, void *data)
{
	const struct opal_key *okey = data;

	return start_generic_opal_session(dev, OPAL_PSID_UID,
					  OPAL_ADMINSP_UID,
					  okey->key,
					  okey->key_len);
}

static int start_auth_opal_session(struct opal_dev *dev, void *data)
{
	struct opal_session_info *session = data;
	uint8_t lk_ul_user[OPAL_UID_LENGTH];
	size_t keylen = session->opal_key.key_len;
	int err = 0;

	uint8_t *key = session->opal_key.key;
	uint32_t hsn = GENERIC_HOST_SESSION_NUM;

	if (session->sum)
		err = build_locking_user(lk_ul_user, sizeof(lk_ul_user),
					 session->opal_key.lr);
	else if (session->who != OPAL_ADMIN1 && !session->sum)
		err = build_locking_user(lk_ul_user, sizeof(lk_ul_user),
					 session->who - 1);
	else
		memcpy(lk_ul_user, opaluid[OPAL_ADMIN1_UID], OPAL_UID_LENGTH);

	if (err)
		return err;

	err = cmd_start(dev, opaluid[OPAL_SMUID_UID],
			opalmethod[OPAL_STARTSESSION]);

	add_token_u64(&err, dev, hsn);
	add_token_bytestring(&err, dev, opaluid[OPAL_LOCKINGSP_UID],
			     OPAL_UID_LENGTH);
	add_token_u8(&err, dev, 1);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, 0);
	add_token_bytestring(&err, dev, key, keylen);
	add_token_u8(&err, dev, OPAL_ENDNAME);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, 3);
	add_token_bytestring(&err, dev, lk_ul_user, OPAL_UID_LENGTH);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	if (err) {
		RAL_DEBUG("Error building STARTSESSION command.\n");
		return err;
	}

	return finalize_and_send(dev, start_opal_session_cont);
}

static int revert_tper(struct opal_dev *dev, void *data)
{
	int err;

	err = cmd_start(dev, opaluid[OPAL_ADMINSP_UID],
			opalmethod[OPAL_REVERT]);
	if (err) {
		RAL_DEBUG("Error building REVERT TPER command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int internal_activate_user(struct opal_dev *dev, void *data)
{
	struct opal_session_info *session = data;
	uint8_t uid[OPAL_UID_LENGTH];
	int err;

	memcpy(uid, opaluid[OPAL_USER1_UID], OPAL_UID_LENGTH);
	uid[7] = session->who;

	err = cmd_start(dev, uid, opalmethod[OPAL_SET]);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);
	add_token_u8(&err, dev, OPAL_STARTLIST);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, 5); /* Enabled */
	add_token_u8(&err, dev, OPAL_TRUE);
	add_token_u8(&err, dev, OPAL_ENDNAME);
	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	if (err) {
		RAL_DEBUG("Error building Activate UserN command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int revert_lsp(struct opal_dev *dev, void *data)
{
	struct opal_revert_lsp *rev = data;
	int err;

	err = cmd_start(dev, opaluid[OPAL_THISSP_UID],
			opalmethod[OPAL_REVERTSP]);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u64(&err, dev, OPAL_KEEP_GLOBAL_RANGE_KEY);
	add_token_u8(&err, dev, (rev->options & OPAL_PRESERVE) ?
			OPAL_TRUE : OPAL_FALSE);
	add_token_u8(&err, dev, OPAL_ENDNAME);
	if (err) {
		RAL_DEBUG("Error building REVERT SP command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int erase_locking_range(struct opal_dev *dev, void *data)
{
	struct opal_session_info *session = data;
	uint8_t uid[OPAL_UID_LENGTH];
	int err;

	if (build_locking_range(uid, sizeof(uid), session->opal_key.lr) < 0)
		return -ERANGE;

	err = cmd_start(dev, uid, opalmethod[OPAL_ERASE]);

	if (err) {
		RAL_DEBUG("Error building Erase Locking Range Command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int set_mbr_done(struct opal_dev *dev, void *data)
{
	uint8_t *mbr_done_tf = data;
	int err;

	err = cmd_start(dev, opaluid[OPAL_MBRCONTROL],
			opalmethod[OPAL_SET]);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);
	add_token_u8(&err, dev, OPAL_STARTLIST);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_MBRDONE);
	add_token_u8(&err, dev, *mbr_done_tf); /* Done T or F */
	add_token_u8(&err, dev, OPAL_ENDNAME);
	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	if (err) {
		RAL_DEBUG("Error Building set MBR Done command\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int set_mbr_enable_disable(struct opal_dev *dev, void *data)
{
	uint8_t *mbr_en_dis = data;
	int err;

	err = cmd_start(dev, opaluid[OPAL_MBRCONTROL],
			opalmethod[OPAL_SET]);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);
	add_token_u8(&err, dev, OPAL_STARTLIST);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_MBRENABLE);
	add_token_u8(&err, dev, *mbr_en_dis);
	add_token_u8(&err, dev, OPAL_ENDNAME);
	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	if (err) {
		RAL_DEBUG("Error Building set MBR done command\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int write_shadow_mbr(struct opal_dev *dev, void *data)
{
	struct opal_shadow_mbr *shadow = data;

	return generic_table_write_data(dev, shadow->data, shadow->offset,
					shadow->size, opaluid[OPAL_MBR]);
}

static int generic_pw_cmd(uint8_t *key, size_t key_len, uint8_t *cpin_uid,
			  struct opal_dev *dev)
{
	int err;

	err = cmd_start(dev, cpin_uid, opalmethod[OPAL_SET]);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);
	add_token_u8(&err, dev, OPAL_STARTLIST);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_PIN);
	add_token_bytestring(&err, dev, key, key_len);
	add_token_u8(&err, dev, OPAL_ENDNAME);
	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	return err;
}

static int set_new_pw(struct opal_dev *dev, void *data)
{
	uint8_t cpin_uid[OPAL_UID_LENGTH];
	struct opal_session_info *usr = data;

	memcpy(cpin_uid, opaluid[OPAL_C_PIN_ADMIN1], OPAL_UID_LENGTH);

	if (usr->who != OPAL_ADMIN1) {
		cpin_uid[5] = 0x03;
		if (usr->sum)
			cpin_uid[7] = usr->opal_key.lr + 1;
		else
			cpin_uid[7] = usr->who;
	}

	if (generic_pw_cmd(usr->opal_key.key, usr->opal_key.key_len,
			   cpin_uid, dev)) {
		RAL_DEBUG("Error building set password command.\n");
		return -ERANGE;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int set_sid_cpin_pin(struct opal_dev *dev, void *data)
{
	uint8_t cpin_uid[OPAL_UID_LENGTH];
	struct opal_key *key = data;

	memcpy(cpin_uid, opaluid[OPAL_C_PIN_SID], OPAL_UID_LENGTH);

	if (generic_pw_cmd(key->key, key->key_len, cpin_uid, dev)) {
		RAL_DEBUG("Error building Set SID cpin\n");
		return -ERANGE;
	}
	return finalize_and_send(dev, parse_and_check_status);
}

static void add_authority_object_ref(int *err,
				     struct opal_dev *dev,
				     const uint8_t *uid,
				     size_t uid_len)
{
	add_token_u8(err, dev, OPAL_STARTNAME);
	add_token_bytestring(err, dev,
			     opaluid[OPAL_HALF_UID_AUTHORITY_OBJ_REF],
			     OPAL_UID_LENGTH/2);
	add_token_bytestring(err, dev, uid, uid_len);
	add_token_u8(err, dev, OPAL_ENDNAME);
}

static void add_boolean_object_ref(int *err,
				   struct opal_dev *dev,
				   uint8_t boolean_op)
{
	add_token_u8(err, dev, OPAL_STARTNAME);
	add_token_bytestring(err, dev, opaluid[OPAL_HALF_UID_BOOLEAN_ACE],
			     OPAL_UID_LENGTH/2);
	add_token_u8(err, dev, boolean_op);
	add_token_u8(err, dev, OPAL_ENDNAME);
}

static int set_lr_boolean_ace(struct opal_dev *dev,
			      unsigned int opal_uid,
			      uint8_t lr,
			      const uint8_t *users,
			      size_t users_len)
{
	uint8_t lr_buffer[OPAL_UID_LENGTH];
	uint8_t user_uid[OPAL_UID_LENGTH];
	uint8_t u;
	int err;

	memcpy(lr_buffer, opaluid[opal_uid], OPAL_UID_LENGTH);
	lr_buffer[7] = lr;

	err = cmd_start(dev, lr_buffer, opalmethod[OPAL_SET]);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);

	add_token_u8(&err, dev, OPAL_STARTLIST);
	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, 3);

	add_token_u8(&err, dev, OPAL_STARTLIST);

	for (u = 0; u < users_len; u++) {
		if (users[u] == OPAL_ADMIN1)
			memcpy(user_uid, opaluid[OPAL_ADMIN1_UID],
			       OPAL_UID_LENGTH);
		else {
			memcpy(user_uid, opaluid[OPAL_USER1_UID],
			       OPAL_UID_LENGTH);
			user_uid[7] = users[u];
		}

		add_authority_object_ref(&err, dev, user_uid, sizeof(user_uid));

		/*
		 * Add boolean operator in postfix only with
		 * two or more authorities being added in ACE
		 * expresion.
		 * */
		if (u > 0)
			add_boolean_object_ref(&err, dev, OPAL_BOOLEAN_OR);
	}

	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);
	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	return err;
}

static int add_user_to_lr(struct opal_dev *dev, void *data)
{
	int err;
	struct opal_lock_unlock *lkul = data;
	const uint8_t users[] = {
		lkul->session.who
	};

	err = set_lr_boolean_ace(dev,
				 lkul->l_state == OPAL_RW ?
					OPAL_LOCKINGRANGE_ACE_WRLOCKED :
					OPAL_LOCKINGRANGE_ACE_RDLOCKED,
				 lkul->session.opal_key.lr, users,
				 ARRAY_SIZE(users));
	if (err) {
		RAL_DEBUG("Error building add user to locking range command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int add_user_to_lr_ace(struct opal_dev *dev, void *data)
{
	int err;
	struct opal_lock_unlock *lkul = data;
	const uint8_t users[] = {
		OPAL_ADMIN1,
		lkul->session.who
	};

	err = set_lr_boolean_ace(dev, OPAL_LOCKINGRANGE_ACE_START_TO_KEY,
				 lkul->session.opal_key.lr, users,
				 ARRAY_SIZE(users));

	if (err) {
		RAL_DEBUG("Error building add user to locking ranges ACEs.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int lock_unlock_locking_range(struct opal_dev *dev, void *data)
{
	uint8_t lr_buffer[OPAL_UID_LENGTH];
	struct opal_lock_unlock *lkul = data;
	uint8_t read_locked = 1, write_locked = 1;
	int err;

	if (build_locking_range(lr_buffer, sizeof(lr_buffer),
				lkul->session.opal_key.lr) < 0)
		return -ERANGE;

	switch (lkul->l_state) {
	case OPAL_RO:
		read_locked = 0;
		write_locked = 1;
		break;
	case OPAL_RW:
		read_locked = 0;
		write_locked = 0;
		break;
	case OPAL_LK:
		/* vars are initialized to locked */
		break;
	default:
		RAL_DEBUG("Tried to set an invalid locking state... returning to uland\n");
		return OPAL_INVAL_PARAM;
	}

	err = cmd_start(dev, lr_buffer, opalmethod[OPAL_SET]);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_VALUES);
	add_token_u8(&err, dev, OPAL_STARTLIST);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_READLOCKED);
	add_token_u8(&err, dev, read_locked);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_STARTNAME);
	add_token_u8(&err, dev, OPAL_WRITELOCKED);
	add_token_u8(&err, dev, write_locked);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	add_token_u8(&err, dev, OPAL_ENDLIST);
	add_token_u8(&err, dev, OPAL_ENDNAME);

	if (err) {
		RAL_DEBUG("Error building SET command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}


static int lock_unlock_locking_range_sum(struct opal_dev *dev, void *data)
{
	uint8_t lr_buffer[OPAL_UID_LENGTH];
	uint8_t read_locked = 1, write_locked = 1;
	struct opal_lock_unlock *lkul = data;
	int ret;

	clear_opal_cmd(dev);
	set_comid(dev, dev->comid);

	if (build_locking_range(lr_buffer, sizeof(lr_buffer),
				lkul->session.opal_key.lr) < 0)
		return -ERANGE;

	switch (lkul->l_state) {
	case OPAL_RO:
		read_locked = 0;
		write_locked = 1;
		break;
	case OPAL_RW:
		read_locked = 0;
		write_locked = 0;
		break;
	case OPAL_LK:
		/* vars are initialized to locked */
		break;
	default:
		RAL_DEBUG("Tried to set an invalid locking state.\n");
		return OPAL_INVAL_PARAM;
	}
	ret = generic_lr_enable_disable(dev, lr_buffer, 1, 1,
					read_locked, write_locked);

	if (ret < 0) {
		RAL_DEBUG("Error building SET command.\n");
		return ret;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

static int activate_lsp(struct opal_dev *dev, void *data)
{
	struct opal_lr_act *opal_act = data;
	uint8_t user_lr[OPAL_UID_LENGTH];
	int err, i;

	err = cmd_start(dev, opaluid[OPAL_LOCKINGSP_UID],
			opalmethod[OPAL_ACTIVATE]);

	if (opal_act->sum) {
		err = build_locking_range(user_lr, sizeof(user_lr),
					  opal_act->lr[0]);
		if (err)
			return err;

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u64(&err, dev, OPAL_SUM_SET_LIST);

		add_token_u8(&err, dev, OPAL_STARTLIST);
		add_token_bytestring(&err, dev, user_lr, OPAL_UID_LENGTH);
		for (i = 1; i < opal_act->num_lrs; i++) {
			user_lr[7] = opal_act->lr[i];
			add_token_bytestring(&err, dev, user_lr, OPAL_UID_LENGTH);
		}
		add_token_u8(&err, dev, OPAL_ENDLIST);
		add_token_u8(&err, dev, OPAL_ENDNAME);
	}

	if (err) {
		RAL_DEBUG("Error building Activate LockingSP command.\n");
		return err;
	}

	return finalize_and_send(dev, parse_and_check_status);
}

/* Determine if we're in the Manufactured Inactive or Active state */
static int get_lsp_lifecycle(struct opal_dev *dev, void *data)
{
	uint8_t lc_status;
	int err;

	err = generic_get_column(dev, opaluid[OPAL_LOCKINGSP_UID],
				 OPAL_LIFECYCLE);
	if (err)
		return err;

	lc_status = response_get_u64(&dev->parsed, 4);
	/* 0x08 is Manufactured Inactive */
	/* 0x09 is Manufactured */
	if (lc_status != OPAL_MANUFACTURED_INACTIVE) {
		RAL_DEBUG("Couldn't determine the status of the Lifecycle state\n");
		return -ENODEV;
	}

	return 0;
}

static int get_msid_cpin_pin(struct opal_dev *dev, void *data)
{
	const char *msid_pin;
	size_t strlen;
	int err;

	err = generic_get_column(dev, opaluid[OPAL_C_PIN_MSID], OPAL_PIN);
	if (err)
		return err;

	strlen = response_get_string(&dev->parsed, 4, &msid_pin);
	if (!msid_pin) {
		RAL_DEBUG("Couldn't extract MSID_CPIN from response\n");
		return OPAL_INVAL_PARAM;
	}

	dev->prev_data = ral_memdup(msid_pin, strlen);
	if (!dev->prev_data)
		return -ENOMEM;

	dev->prev_d_len = strlen;

	return 0;
}

static int write_table_data(struct opal_dev *dev, void *data)
{
	struct opal_read_write_table *write_tbl = data;

	return generic_table_write_data(dev, write_tbl->data, write_tbl->offset,
					write_tbl->size, write_tbl->table_uid);
}

static int read_table_data_cont(struct opal_dev *dev)
{
	int err;
	const char *data_read;

	err = parse_and_check_status(dev);
	if (err)
		return err;

	dev->prev_d_len = response_get_string(&dev->parsed, 1, &data_read);
	dev->prev_data = (void *)data_read;
	if (!dev->prev_data) {
		RAL_DEBUG("%s: Couldn't read data from the table.\n", __func__);
		return OPAL_INVAL_PARAM;
	}

	return 0;
}

/*
 * IO_BUFFER_LENGTH = 2048
 * sizeof(header) = 56
 * No. of Token Bytes in the Response = 11
 * MAX size of data that can be carried in response buffer
 * at a time is : 2048 - (56 + 11) = 1981 = 0x7BD.
 */
#define OPAL_MAX_READ_TABLE (0x7BD)

static int read_table_data(struct opal_dev *dev, void *data)
{
	struct opal_read_write_table *read_tbl = data;
	int err;
	size_t off = 0, max_read_size = OPAL_MAX_READ_TABLE;
	uint64_t table_len, len;
	uint64_t offset = read_tbl->offset, read_size = read_tbl->size - 1;
	uint8_t *dst;

	err = generic_get_table_info(dev, read_tbl->table_uid, OPAL_TABLE_ROWS);
	if (err) {
		RAL_DEBUG("Couldn't get the table size\n");
		return err;
	}

	table_len = response_get_u64(&dev->parsed, 4);

	/* Check if the user is trying to read from the table limits */
	if (read_size > table_len || offset > table_len - read_size) {
		RAL_DEBUG("Read size exceeds the Table size limits (%llu vs. %llu)\n",
			  offset + read_size, table_len);
		return -EINVAL;
	}

	while (off < read_size) {
		err = cmd_start(dev, read_tbl->table_uid, opalmethod[OPAL_GET]);

		add_token_u8(&err, dev, OPAL_STARTLIST);
		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_STARTROW);
		add_token_u64(&err, dev, offset + off); /* start row value */
		add_token_u8(&err, dev, OPAL_ENDNAME);

		add_token_u8(&err, dev, OPAL_STARTNAME);
		add_token_u8(&err, dev, OPAL_ENDROW);

		len = min(max_read_size, (size_t)(read_size - off));
		add_token_u64(&err, dev, offset + off + len); /* end row value
							       */
		add_token_u8(&err, dev, OPAL_ENDNAME);
		add_token_u8(&err, dev, OPAL_ENDLIST);

		if (err) {
			RAL_DEBUG("Error building read table data command.\n");
			break;
		}

		err = finalize_and_send(dev, read_table_data_cont);
		if (err)
			break;

		/* len+1: This includes the NULL terminator at the end*/
		if (dev->prev_d_len > len + 1) {
			err = -EOVERFLOW;
			break;
		}

		dst = (uint8_t *)(uintptr_t)read_tbl->data;
		if (memcpy(dst + off, dev->prev_data, dev->prev_d_len)) {
			RAL_DEBUG("Error copying data to userspace\n");
			err = -EFAULT;
			break;
		}
		dev->prev_data = NULL;

		off += len;
	}

	return err;
}

static int end_opal_session(struct opal_dev *dev, void *data)
{
	int err = 0;

	clear_opal_cmd(dev);
	set_comid(dev, dev->comid);
	add_token_u8(&err, dev, OPAL_ENDOFSESSION);

	if (err < 0)
		return err;

	return finalize_and_send(dev, end_session_cont);
}

static int end_opal_session_error(struct opal_dev *dev)
{
	const struct opal_step error_end_session = {
		end_opal_session,
	};

	return execute_step(dev, &error_end_session, 0);
}

static inline void setup_opal_dev(struct opal_dev *dev)
{
	dev->tsn = 0;
	dev->hsn = 0;
	dev->prev_data = NULL;
}

static int check_opal_support(struct opal_dev *dev)
{
	int ret;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = opal_discovery0_step(dev);
	if (!ret)
		dev->flags |= OPAL_FL_SUPPORTED;
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

static void clean_opal_dev(struct opal_dev *dev) { }

void free_opal_dev(struct opal_dev *dev)
{
	if (!dev)
		return;

	clean_opal_dev(dev);
	ral_free(dev->resp);
	ral_free(dev->cmd);
	ral_free(dev);
}


struct opal_dev *init_opal_dev(void *data, sec_send_recv *send_recv)
{
	struct opal_dev *dev;

	dev = ral_malloc(sizeof(*dev));
	if (!dev)
		return NULL;

	/*
	 * Presumably DMA-able buffers must be cache-aligned. Kmalloc makes
	 * sure the allocated buffer is DMA-safe in that regard.
	 */
	dev->cmd = ral_malloc(IO_BUFFER_LENGTH);
	if (!dev->cmd)
		goto err_free_dev;

	dev->resp = ral_malloc(IO_BUFFER_LENGTH);
	if (!dev->resp)
		goto err_free_cmd;

	INIT_LIST_HEAD(&dev->unlk_lst);
	dev->dev_lock = ral_mutex_create();
	dev->flags = 0;
	dev->data = data;
	dev->send_recv = send_recv;
	if (check_opal_support(dev) != 0) {
		RAL_DEBUG("Opal is not supported on this device\n");
		goto err_free_resp;
	}

	return dev;

err_free_resp:
	ral_free(dev->resp);

err_free_cmd:
	ral_free(dev->cmd);

err_free_dev:
	ral_free(dev);

	return NULL;
}


int opal_secure_erase_locking_range(struct opal_dev *dev,
					   struct opal_session_info *opal_session)
{
	const struct opal_step erase_steps[] = {
		{ start_auth_opal_session, opal_session },
		{ get_active_key, &opal_session->opal_key.lr },
		{ gen_key, },
		{ end_opal_session, }
	};
	int ret;

	ret = opal_get_key(dev, &opal_session->opal_key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, erase_steps, ARRAY_SIZE(erase_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

static int opal_get_discv(struct opal_dev *dev, struct opal_discovery *discv)
{
	const struct opal_step discovery0_step = {
		opal_discovery0, discv
	};
	int ret;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_step(dev, &discovery0_step, 0);
	ral_mutex_unlock(dev->dev_lock);
	if (ret)
		return ret;
	return discv->size; /* modified to actual length of data */
}

static int opal_revertlsp(struct opal_dev *dev, struct opal_revert_lsp *rev)
{
	/* controller will terminate session */
	const struct opal_step steps[] = {
		{ start_admin1LSP_opal_session, &rev->key },
		{ revert_lsp, rev }
	};
	int ret;

	ret = opal_get_key(dev, &rev->key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, steps, ARRAY_SIZE(steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_erase_locking_range(struct opal_dev *dev,
				    struct opal_session_info *opal_session)
{
	const struct opal_step erase_steps[] = {
		{ start_auth_opal_session, opal_session },
		{ erase_locking_range, opal_session },
		{ end_opal_session, }
	};
	int ret;

	ret = opal_get_key(dev, &opal_session->opal_key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, erase_steps, ARRAY_SIZE(erase_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_enable_disable_shadow_mbr(struct opal_dev *dev,
					  struct opal_mbr_data *opal_mbr)
{
	uint8_t enable_disable = opal_mbr->enable_disable == OPAL_MBR_ENABLE ?
		OPAL_TRUE : OPAL_FALSE;

	const struct opal_step mbr_steps[] = {
		{ start_admin1LSP_opal_session, &opal_mbr->key },
		{ set_mbr_done, &enable_disable },
		{ end_opal_session, },
		{ start_admin1LSP_opal_session, &opal_mbr->key },
		{ set_mbr_enable_disable, &enable_disable },
		{ end_opal_session, }
	};
	int ret;

	if (opal_mbr->enable_disable != OPAL_MBR_ENABLE &&
	    opal_mbr->enable_disable != OPAL_MBR_DISABLE)
		return -EINVAL;

	ret = opal_get_key(dev, &opal_mbr->key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, mbr_steps, ARRAY_SIZE(mbr_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_set_mbr_done(struct opal_dev *dev,
			     struct opal_mbr_done *mbr_done)
{
	uint8_t mbr_done_tf = mbr_done->done_flag == OPAL_MBR_DONE ?
		OPAL_TRUE : OPAL_FALSE;

	const struct opal_step mbr_steps[] = {
		{ start_admin1LSP_opal_session, &mbr_done->key },
		{ set_mbr_done, &mbr_done_tf },
		{ end_opal_session, }
	};
	int ret;

	if (mbr_done->done_flag != OPAL_MBR_DONE &&
	    mbr_done->done_flag != OPAL_MBR_NOT_DONE)
		return -EINVAL;

	ret = opal_get_key(dev, &mbr_done->key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, mbr_steps, ARRAY_SIZE(mbr_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_write_shadow_mbr(struct opal_dev *dev,
				 struct opal_shadow_mbr *info)
{
	const struct opal_step mbr_steps[] = {
		{ start_admin1LSP_opal_session, &info->key },
		{ write_shadow_mbr, info },
		{ end_opal_session, }
	};
	int ret;

	if (info->size == 0)
		return 0;

	ret = opal_get_key(dev, &info->key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, mbr_steps, ARRAY_SIZE(mbr_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_save(struct opal_dev *dev, struct opal_lock_unlock *lk_unlk)
{
	struct opal_suspend_data *suspend;

	suspend = ral_calloc(1, sizeof(*suspend));
	if (!suspend)
		return -ENOMEM;

	suspend->unlk = *lk_unlk;
	suspend->lr = lk_unlk->session.opal_key.lr;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	add_suspend_info(dev, suspend);
	ral_mutex_unlock(dev->dev_lock);

	return 0;
}

int opal_add_user_to_lr(struct opal_dev *dev,
			       struct opal_lock_unlock *lk_unlk)
{
	const struct opal_step steps[] = {
		{ start_admin1LSP_opal_session, &lk_unlk->session.opal_key },
		{ add_user_to_lr, lk_unlk },
		{ add_user_to_lr_ace, lk_unlk },
		{ end_opal_session, }
	};
	int ret;

	if (lk_unlk->l_state != OPAL_RO &&
	    lk_unlk->l_state != OPAL_RW) {
		RAL_DEBUG("Locking state was not RO or RW\n");
		return -EINVAL;
	}

	if (lk_unlk->session.who < OPAL_USER1 ||
	    lk_unlk->session.who > OPAL_USER9) {
		RAL_DEBUG("Authority was not within the range of users: %d\n",
			 lk_unlk->session.who);
		return -EINVAL;
	}

	if (lk_unlk->session.sum) {
		RAL_DEBUG("%s not supported in sum. Use setup locking range\n",
			 __func__);
		return -EINVAL;
	}

	ret = opal_get_key(dev, &lk_unlk->session.opal_key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, steps, ARRAY_SIZE(steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_reverttper(struct opal_dev *dev, struct opal_key *opal, bool psid)
{
	/* controller will terminate session */
	const struct opal_step revert_steps[] = {
		{ start_SIDASP_opal_session, opal },
		{ revert_tper, }
	};
	const struct opal_step psid_revert_steps[] = {
		{ start_PSID_opal_session, opal },
		{ revert_tper, }
	};

	int ret;

	ret = opal_get_key(dev, opal);

	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	if (psid)
		ret = execute_steps(dev, psid_revert_steps,
				    ARRAY_SIZE(psid_revert_steps));
	else
		ret = execute_steps(dev, revert_steps,
				    ARRAY_SIZE(revert_steps));
	ral_mutex_unlock(dev->dev_lock);

	/*
	 * If we successfully reverted lets clean
	 * any saved locking ranges.
	 */
	if (!ret)
		clean_opal_dev(dev);

	return ret;
}

static int __opal_lock_unlock(struct opal_dev *dev,
			      struct opal_lock_unlock *lk_unlk)
{
	const struct opal_step unlock_steps[] = {
		{ start_auth_opal_session, &lk_unlk->session },
		{ lock_unlock_locking_range, lk_unlk },
		{ end_opal_session, }
	};
	const struct opal_step unlock_sum_steps[] = {
		{ start_auth_opal_session, &lk_unlk->session },
		{ lock_unlock_locking_range_sum, lk_unlk },
		{ end_opal_session, }
	};

	if (lk_unlk->session.sum)
		return execute_steps(dev, unlock_sum_steps,
				     ARRAY_SIZE(unlock_sum_steps));
	else
		return execute_steps(dev, unlock_steps,
				     ARRAY_SIZE(unlock_steps));
}

static int __opal_set_mbr_done(struct opal_dev *dev, struct opal_key *key)
{
	uint8_t mbr_done_tf = OPAL_TRUE;
	const struct opal_step mbrdone_step[] = {
		{ start_admin1LSP_opal_session, key },
		{ set_mbr_done, &mbr_done_tf },
		{ end_opal_session, }
	};

	return execute_steps(dev, mbrdone_step, ARRAY_SIZE(mbrdone_step));
}

static void opal_lock_check_for_saved_key(struct opal_dev *dev, struct opal_lock_unlock *lk_unlk) { }

int opal_lock_unlock(struct opal_dev *dev,
			    struct opal_lock_unlock *lk_unlk)
{
	int ret;

	if (lk_unlk->session.who > OPAL_USER9)
		return -EINVAL;

	ral_mutex_lock(dev->dev_lock);
	opal_lock_check_for_saved_key(dev, lk_unlk);
	ret = opal_get_key(dev, &lk_unlk->session.opal_key);
	if (!ret)
		ret = __opal_lock_unlock(dev, lk_unlk);
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_take_ownership(struct opal_dev *dev, struct opal_key *opal)
{
	const struct opal_step owner_steps[] = {
		{ start_anybodyASP_opal_session, },
		{ get_msid_cpin_pin, },
		{ end_opal_session, },
		{ start_SIDASP_opal_session, opal },
		{ set_sid_cpin_pin, opal },
		{ end_opal_session, }
	};
	int ret;

	if (!dev)
		return -ENODEV;

	ret = opal_get_key(dev, opal);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, owner_steps, ARRAY_SIZE(owner_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_activate_lsp(struct opal_dev *dev,
			     struct opal_lr_act *opal_lr_act)
{
	const struct opal_step active_steps[] = {
		{ start_SIDASP_opal_session, &opal_lr_act->key },
		{ get_lsp_lifecycle, },
		{ activate_lsp, opal_lr_act },
		{ end_opal_session, }
	};
	int ret;

	if (opal_lr_act->sum &&
	    (!opal_lr_act->num_lrs || opal_lr_act->num_lrs > OPAL_MAX_LRS))
		return -EINVAL;

	ret = opal_get_key(dev, &opal_lr_act->key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, active_steps, ARRAY_SIZE(active_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_setup_locking_range(struct opal_dev *dev,
				    struct opal_user_lr_setup *opal_lrs)
{
	const struct opal_step lr_steps[] = {
		{ start_auth_opal_session, &opal_lrs->session },
		{ setup_locking_range, opal_lrs },
		{ end_opal_session, }
	};
	int ret;

	ret = opal_get_key(dev, &opal_lrs->session.opal_key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, lr_steps, ARRAY_SIZE(lr_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_locking_range_status(struct opal_dev *dev,
			  struct opal_lr_status *opal_lrst,
			  void *data)
{
	const struct opal_step lr_steps[] = {
		{ start_auth_opal_session, &opal_lrst->session },
		{ locking_range_status, opal_lrst },
		{ end_opal_session, }
	};
	int ret;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, lr_steps, ARRAY_SIZE(lr_steps));
	ral_mutex_unlock(dev->dev_lock);

	/* skip session info when copying back to uspace */
	if (!ret && memcpy(data + offsetof(struct opal_lr_status, range_start),
				(void *)opal_lrst + offsetof(struct opal_lr_status, range_start),
				sizeof(*opal_lrst) - offsetof(struct opal_lr_status, range_start))) {
		RAL_DEBUG("Error copying status to userspace\n");
		return -EFAULT;
	}

	return ret;
}

int opal_set_new_pw(struct opal_dev *dev, struct opal_new_pw *opal_pw)
{
	const struct opal_step pw_steps[] = {
		{ start_auth_opal_session, &opal_pw->session },
		{ set_new_pw, &opal_pw->new_user_pw },
		{ end_opal_session, }
	};
	int ret;

	if (opal_pw->session.who > OPAL_USER9  ||
	    opal_pw->new_user_pw.who > OPAL_USER9)
		return -EINVAL;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, pw_steps, ARRAY_SIZE(pw_steps));
	ral_mutex_unlock(dev->dev_lock);

	if (ret)
		return ret;

	/* update keyring and key store with new password */
	if (ret != -EOPNOTSUPP)


	return ret;
	return ret;
}

int opal_set_new_sid_pw(struct opal_dev *dev, struct opal_new_pw *opal_pw)
{
	int ret;
	struct opal_key *newkey = &opal_pw->new_user_pw.opal_key;
	struct opal_key *oldkey = &opal_pw->session.opal_key;

	const struct opal_step pw_steps[] = {
		{ start_SIDASP_opal_session, oldkey },
		{ set_sid_cpin_pin, newkey },
		{ end_opal_session, }
	};

	if (!dev)
		return -ENODEV;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, pw_steps, ARRAY_SIZE(pw_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_activate_user(struct opal_dev *dev,
			      struct opal_session_info *opal_session)
{
	const struct opal_step act_steps[] = {
		{ start_admin1LSP_opal_session, &opal_session->opal_key },
		{ internal_activate_user, opal_session },
		{ end_opal_session, }
	};
	int ret;

	/* We can't activate Admin1 it's active as manufactured */
	if (opal_session->who < OPAL_USER1 ||
	    opal_session->who > OPAL_USER9) {
		RAL_DEBUG("Who was not a valid user: %d\n", opal_session->who);
		return -EINVAL;
	}

	ret = opal_get_key(dev, &opal_session->opal_key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);
	ret = execute_steps(dev, act_steps, ARRAY_SIZE(act_steps));
	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

bool opal_unlock_from_suspend(struct opal_dev *dev)
{
	struct opal_suspend_data *suspend;
	bool was_failure = false;
	int ret;

	if (!dev)
		return false;

	if (!(dev->flags & OPAL_FL_SUPPORTED))
		return false;

	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);

	list_for_each_entry(suspend, &dev->unlk_lst, node) {
		dev->tsn = 0;
		dev->hsn = 0;

		ret = __opal_lock_unlock(dev, &suspend->unlk);
		if (ret) {
			RAL_DEBUG("Failed to unlock LR %hhu with sum %d\n",
				 suspend->unlk.session.opal_key.lr,
				 suspend->unlk.session.sum);
			was_failure = true;
		}

		if (dev->flags & OPAL_FL_MBR_ENABLED) {
			ret = __opal_set_mbr_done(dev, &suspend->unlk.session.opal_key);
			if (ret)
				RAL_DEBUG("Failed to set MBR Done in S3 resume\n");
		}
	}
	ral_mutex_unlock(dev->dev_lock);

	return was_failure;
}


static int opal_read_table(struct opal_dev *dev,
			   struct opal_read_write_table *rw_tbl)
{
	const struct opal_step read_table_steps[] = {
		{ start_admin1LSP_opal_session, &rw_tbl->key },
		{ read_table_data, rw_tbl },
		{ end_opal_session, }
	};

	if (!rw_tbl->size)
		return 0;

	return execute_steps(dev, read_table_steps,
			     ARRAY_SIZE(read_table_steps));
}

static int opal_write_table(struct opal_dev *dev,
			    struct opal_read_write_table *rw_tbl)
{
	const struct opal_step write_table_steps[] = {
		{ start_admin1LSP_opal_session, &rw_tbl->key },
		{ write_table_data, rw_tbl },
		{ end_opal_session, }
	};

	if (!rw_tbl->size)
		return 0;

	return execute_steps(dev, write_table_steps,
			     ARRAY_SIZE(write_table_steps));
}

int opal_generic_read_write_table(struct opal_dev *dev,
					 struct opal_read_write_table *rw_tbl)
{
	int ret, bit_set;

	ret = opal_get_key(dev, &rw_tbl->key);
	if (ret)
		return ret;
	ral_mutex_lock(dev->dev_lock);
	setup_opal_dev(dev);

	bit_set = fls64(rw_tbl->flags) - 1;
	switch (bit_set) {
	case OPAL_READ_TABLE:
		ret = opal_read_table(dev, rw_tbl);
		break;
	case OPAL_WRITE_TABLE:
		ret = opal_write_table(dev, rw_tbl);
		break;
	default:
		RAL_DEBUG("Invalid bit set in the flag (%016llx).\n",
			 rw_tbl->flags);
		ret = -EINVAL;
		break;
	}

	ral_mutex_unlock(dev->dev_lock);

	return ret;
}

int opal_get_status(struct opal_dev *dev, void *data)
{
	struct opal_status sts = {0};

	/*
	 * check_opal_support() error is not fatal,
	 * !dev->supported is a valid condition
	 */
	if (!check_opal_support(dev))
		sts.flags = dev->flags;
	if (memcpy(data, &sts, sizeof(sts))) {
		RAL_DEBUG("Error copying status to userspace\n");
		return -EFAULT;
	}
	return 0;
}

int opal_get_geometry(struct opal_dev *dev, void *data)
{
	struct opal_geometry geo = {0};

	if (check_opal_support(dev))
		return -EINVAL;

	geo.align = dev->align_required;
	geo.logical_block_size = dev->logical_block_size;
	geo.alignment_granularity =  dev->align;
	geo.lowest_aligned_lba = dev->lowest_lba;

	if (memcpy(data, &geo, sizeof(geo))) {
		RAL_DEBUG("Error copying geometry data to userspace\n");
		return -EFAULT;
	}

	return 0;
}

