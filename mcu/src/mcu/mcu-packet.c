/*
 * mcu-packet.c
 * mcu coprocessor bus protocol, packet handling
 *
 * Author: Alex.wang
 * Create: 2015-07-16 19:40
 */

#include <linux/spinlock.h>
#include <linux/slab.h>
#include "mcu-packet.h"

/* recommended packet receive buffer size */
#define MCU_PACKET_BUFFER_SIZE	512

#define MCU_PACKET_XOR	0xd8

struct mcu_packet_header {
#define MCU_PACKET_MAGIC0	0x4d
	unsigned char magic0;
#define MCU_PACKET_MAGIC1	0x43
	unsigned char magic1;
#define MCU_PACKET_MAX_LENGTH	250
	unsigned char length;
#define MCU_PACKET_PING	0x70
#define MCU_PACKET_PONG	0x61
#define MCU_PACKET_CONTROL_REQUEST	0x71
#define MCU_PACKET_CONTROL_RESPONSE	0x72
	unsigned char identity;
#define MCU_PACKET_CHECKSUM_NULL	0xff
	unsigned char message_checksum;
	unsigned char header_checksum;
} __attribute__((packed));

struct mcu_packet_device_control {
	mcu_device_id device_id;
	mcu_control_code control_code;
	unsigned char detail[0];
} __attribute__((packed));

struct mcu_packet {
	struct mcu_packet_header header;
	union {
		struct mcu_packet_device_control control;
	} message;
} __attribute__((packed));


struct mcu_packet_private {
	unsigned char buffer[MCU_PACKET_BUFFER_SIZE];
	int buffer_start, buffer_end;
	spinlock_t buffer_lock;

	/* avoid alloc and free */
	struct mcu_packet packet[2];

	struct mcu_packet_callback *callback;
} *mcu_packet_data = NULL;

static int mcu_get_packet_length(struct mcu_packet *packet)
{
	if (unlikely(!packet)) {
		return 0;
	}

	return sizeof(packet->header) + packet->header.length;
}

static unsigned char mcu_packet_get_checksum(void *buffer, int len)
{
	int i;
	int sum = 0;
	unsigned char *cp = (unsigned char *)buffer;

	for (i = 0; i < len; i++) {
		sum += cp[i];
	}

	return sum & 0xff;
}

static void mcu_packet_header_fill(struct mcu_packet *packet)
{
	if (unlikely(!packet)) {
		return;
	}

	packet->header.magic0 = MCU_PACKET_MAGIC0;
	packet->header.magic1 = MCU_PACKET_MAGIC1;
	packet->header.message_checksum = 0 == packet->header.length ? MCU_PACKET_CHECKSUM_NULL : mcu_packet_get_checksum(&packet->message, packet->header.length);
	packet->header.header_checksum = mcu_packet_get_checksum(packet, sizeof(packet->header) - sizeof(packet->header.header_checksum));
}


static int mcu_packet_verify_checksum(struct mcu_packet *packet)
{
	unsigned char checksum;
	if (unlikely(!packet)) {
		return 0;
	}

	if (unlikely(packet->header.length > MCU_PACKET_MAX_LENGTH)) {
		return 0;
	}

	checksum = mcu_packet_get_checksum(packet, sizeof(packet->header) - sizeof(packet->header.header_checksum));
	if (unlikely(checksum != packet->header.header_checksum)) {
		return 0;
	}

	checksum = 0 == packet->header.length ? MCU_PACKET_CHECKSUM_NULL : mcu_packet_get_checksum(&packet->message, packet->header.length);
	if (unlikely(checksum != packet->header.message_checksum)) {
		return 0;
	}

	return 1;
}

static int __mcu_packet_write(void *buffer, int count)
{
	if (!mcu_packet_data || !mcu_packet_data->callback) {
		return -EINVAL;
	}

	return mcu_packet_data->callback->write(buffer, count);
}

static int __mcu_packet_send(struct mcu_packet *packet)
{
	return __mcu_packet_write(packet, mcu_get_packet_length(packet));
}

static void __mcu_packet_do_xor(struct mcu_packet *packet)
{
	int i, len = mcu_get_packet_length(packet);
	unsigned char *cp = (unsigned char *)packet;
	for (i = 0; i < len; i++) {
		cp[i] ^= MCU_PACKET_XOR;
	}
}

static int mcu_packet_send(struct mcu_packet *packet)
{
	mcu_packet_header_fill(packet);

	__mcu_packet_do_xor(packet);
	return __mcu_packet_send(packet);
}

int mcu_packet_send_ping(void)
{
	return __mcu_packet_write(&mcu_packet_data->packet[0], sizeof(struct mcu_packet_header));
}

int mcu_packet_send_pong(void)
{
	return __mcu_packet_write(&mcu_packet_data->packet[1], sizeof(struct mcu_packet_header));
}

static int mcu_packet_send_identity(unsigned char identity, mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len)
{
	int ret;
	unsigned char message_length = len + sizeof(struct mcu_packet_device_control);

	struct mcu_packet *packet = kzalloc(sizeof(struct mcu_packet) + message_length, GFP_KERNEL);
	if (unlikely(!packet)) {
		return -ENOMEM;
	}

	packet->header.identity = identity;
	packet->header.length = message_length;
	packet->message.control.device_id = device_id;
	packet->message.control.control_code = control_code;
	memcpy(&packet->message.control.detail, cp, len);

	ret = mcu_packet_send(packet);
	kfree(packet);
	return ret;
}

int mcu_packet_send_control_request(mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len)
{
	return mcu_packet_send_identity(MCU_PACKET_CONTROL_REQUEST, device_id, control_code, cp, len);
}

int mcu_packet_send_control_response(mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len)
{
	return mcu_packet_send_identity(MCU_PACKET_CONTROL_RESPONSE, device_id, control_code, cp, len);
}


static int __mcu_packet_empty(void)
{
	return mcu_packet_data->buffer_start == mcu_packet_data->buffer_end;
}

static void __mcu_packet_buffer_reset(void)
{
	mcu_packet_data->buffer_start = 0;
	mcu_packet_data->buffer_end = 0;
}

static void __mcu_packet_buffer_consume(int count)
{
	mcu_packet_data->buffer_start += count;
	if (__mcu_packet_empty()) {
		__mcu_packet_buffer_reset();
	}
}

static struct mcu_packet * __mcu_packet_detect(void)
{
	int i;
	struct mcu_packet *packet;
	for (i = mcu_packet_data->buffer_start; i < mcu_packet_data->buffer_end; i++) {
		if (MCU_PACKET_MAGIC0 == mcu_packet_data->buffer[i] && MCU_PACKET_MAGIC1 == mcu_packet_data->buffer[i + 1]) {
			packet = (struct mcu_packet *)&mcu_packet_data->buffer[i];
			if (mcu_packet_verify_checksum(packet)) {
				__mcu_packet_buffer_consume(i + mcu_get_packet_length(packet) - mcu_packet_data->buffer_start);
				return packet;
			}
		}
	}

	// not found, try to reset buffer
	//__mcu_packet_buffer_consume(i - mcu_packet_data->buffer_start);

	return NULL;
}

static void __mcu_packet_report(struct mcu_packet *packet)
{
	switch (packet->header.identity) {
	case MCU_PACKET_PING:
		mcu_packet_data->callback->ping();
		break;
	case MCU_PACKET_PONG:
		mcu_packet_data->callback->pong();
		break;
	case MCU_PACKET_CONTROL_REQUEST:
		mcu_packet_data->callback->new_request(
			packet->message.control.device_id,
			packet->message.control.control_code,
			&packet->message.control.detail,
			packet->header.length - sizeof(struct mcu_packet_device_control));
		break;
	case MCU_PACKET_CONTROL_RESPONSE:
		mcu_packet_data->callback->new_response(
			packet->message.control.device_id,
			packet->message.control.control_code,
			&packet->message.control.detail,
			packet->header.length - sizeof(struct mcu_packet_device_control));
		break;
	default:
		break;
	}
}

void mcu_packet_buffer_detect(void)
{
	if (unlikely(!mcu_packet_data)) {
		return;
	}

	spin_lock(&mcu_packet_data->buffer_lock);
	{
		struct mcu_packet *packet = __mcu_packet_detect();
		if (packet) {
			__mcu_packet_report(packet);
		}
	}
	spin_unlock(&mcu_packet_data->buffer_lock);
}

static int mcu_packet_append(const unsigned char *cp, int count)
{
	int len = 0;
	if (unlikely(!mcu_packet_data)) {
		return -EINVAL;
	}

	spin_lock(&mcu_packet_data->buffer_lock);
	{
		int i;
		len = min(count, MCU_PACKET_BUFFER_SIZE - mcu_packet_data->buffer_end);
		for (i = 0; i < len; i++) {
			mcu_packet_data->buffer[mcu_packet_data->buffer_end + i] = cp[i] ^ MCU_PACKET_XOR;
		}
	}
	spin_unlock(&mcu_packet_data->buffer_lock);

	return len;
}

int mcu_packet_receive_buffer(const void *cp, int count)
{
	return mcu_packet_append((const unsigned char *)cp, count);
}


int __init mcu_packet_init(struct mcu_packet_callback *callback)
{
	// internal api, not param check on callback

	mcu_packet_data = kzalloc(sizeof(*mcu_packet_data), GFP_KERNEL);
	if (unlikely(!mcu_packet_data)) {
		return -ENOMEM;
	}
	mcu_packet_data->callback = callback;

	mcu_packet_data->packet[0].header.identity = MCU_PACKET_PING;
	mcu_packet_header_fill(&mcu_packet_data->packet[0]);
	__mcu_packet_do_xor(&mcu_packet_data->packet[0]);
	mcu_packet_data->packet[1].header.identity = MCU_PACKET_PONG;
	mcu_packet_header_fill(&mcu_packet_data->packet[1]);
	__mcu_packet_do_xor(&mcu_packet_data->packet[1]);

	spin_lock_init(&mcu_packet_data->buffer_lock);

	return 0;
}

void __exit mcu_packet_deinit(void)
{
	kfree(mcu_packet_data);
	mcu_packet_data = NULL;
}

