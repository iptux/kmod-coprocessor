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
#include "mcu-internal.h"

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

struct mcu_packet_error_response {
	unsigned char error_id;
	unsigned char error_code;
} __attribute__((packed));

struct mcu_packet {
	struct mcu_packet_header header;
	union {
		struct mcu_packet_device_control control;
		struct mcu_packet_error_response error;
	} message;
} __attribute__((packed));


struct mcu_packet_private {
	unsigned char buffer[MCU_PACKET_BUFFER_SIZE];
	int buffer_start, buffer_end;
	spinlock_t buffer_lock;

	struct mcu_packet_callback *callback;
};

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

static int __mcu_packet_write(struct mcu_bus_device *bus, void *buffer, int count)
{
	struct mcu_packet_private *mcu_packet_data = bus->pkt_data;
	if (!mcu_packet_data || !mcu_packet_data->callback) {
		return -EINVAL;
	}

	return mcu_packet_data->callback->write(bus, buffer, count);
}

static void __mcu_packet_do_xor(struct mcu_packet *packet)
{
	int i, len = mcu_get_packet_length(packet);
	unsigned char *cp = (unsigned char *)packet;
	for (i = 0; i < len; i++) {
		cp[i] ^= MCU_PACKET_XOR;
	}
}

static int mcu_packet_send(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	int len = mcu_get_packet_length(packet);
	mcu_packet_header_fill(packet);

	// after xor, package is damaged, mcu_get_packet_length() will get wrong result
	__mcu_packet_do_xor(packet);
	return __mcu_packet_write(bus, packet, len);
}

void mcu_packet_free(struct mcu_packet *packet)
{
	kfree(packet);
}

static struct mcu_packet *__mcu_packet_send_ping(struct mcu_bus_device *bus, unsigned char identity)
{
	int ret;
	struct mcu_packet *packet;

	if (unlikely(!bus)) {
		return NULL;
	}

	packet = kzalloc(sizeof(struct mcu_packet), GFP_KERNEL);
	if (unlikely(!packet)) {
		return NULL;
	}

	packet->header.identity = identity;
	packet->header.length = 0;

	ret = mcu_packet_send(bus, packet);
	if (unlikely(ret < sizeof(struct mcu_packet_header))) {
		kfree(packet);
		packet = NULL;
	}

	return packet;
}

struct mcu_packet *mcu_packet_send_ping(struct mcu_bus_device *bus)
{
	return __mcu_packet_send_ping(bus, MCU_PACKET_PING);
}

struct mcu_packet *mcu_packet_send_pong(struct mcu_bus_device *bus)
{
	return __mcu_packet_send_ping(bus, MCU_PACKET_PONG);
}

static struct mcu_packet *mcu_packet_send_control(struct mcu_bus_device *bus, unsigned char identity, mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len)
{
	int ret;
	unsigned char message_length = len + sizeof(struct mcu_packet_device_control);
	struct mcu_packet *packet;

	if (unlikely(!bus)) {
		return NULL;
	}

	packet = kzalloc(sizeof(struct mcu_packet) + message_length, GFP_KERNEL);
	if (unlikely(!packet)) {
		return NULL;
	}

	packet->header.identity = identity;
	packet->header.length = message_length;
	packet->message.control.device_id = device_id;
	packet->message.control.control_code = control_code;
	if (len) {
		memcpy(&packet->message.control.detail, cp, len);
	}

	ret = mcu_packet_send(bus, packet);
	if (unlikely(ret < sizeof(struct mcu_packet_header) + message_length)) {
		kfree(packet);
		packet = NULL;
	}

	return packet;
}

struct mcu_packet *mcu_packet_send_control_request(struct mcu_bus_device *bus, mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len)
{
	return mcu_packet_send_control(bus, MCU_PACKET_CONTROL_REQUEST, device_id, control_code, cp, len);
}

struct mcu_packet *mcu_packet_send_control_response(struct mcu_bus_device *bus, mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len)
{
	return mcu_packet_send_control(bus, MCU_PACKET_CONTROL_RESPONSE, device_id, control_code, cp, len);
}

int mcu_packet_extract_control_info(struct mcu_packet *packet, mcu_device_id *device_id, mcu_control_code *control_code, int *detail_len)
{
	if (unlikely(!packet))
		return -EINVAL;

	if (device_id) *device_id = packet->message.control.device_id;
	if (control_code) *control_code = packet->message.control.control_code;
	if (detail_len) *detail_len = packet->header.length - sizeof(struct mcu_packet_device_control);

	return 0;
}

int mcu_packet_copy_control_detail(struct mcu_packet *packet, void *buffer, int *size)
{
	int len;
	if (!packet || !buffer || !size) {
		return -EFAULT;
	}
	// if is an error response
	if (MCU_DEVICE_ERROR_ID == packet->message.error.error_id) {
		return -packet->message.error.error_code;
	}
	len = packet->header.length - sizeof(struct mcu_packet_device_control);
	len = min(*size, len);
	*size = packet->header.length - sizeof(struct mcu_packet_device_control);
	if (len)
		memcpy(buffer, packet->message.control.detail, len);
	return len;
}

int mcu_packet_response_to(const struct mcu_packet *req, const struct mcu_packet *resp)
{
	unsigned char req_type, resp_type;
	mcu_device_id req_id, resp_id;
	mcu_control_code req_cmd;
	if (!req || !resp) return 0;
	req_type = req->header.identity ^ MCU_PACKET_XOR;
	resp_type = resp->header.identity;
	// for ping and pong, no further check
	if (MCU_PACKET_PING == req_type && MCU_PACKET_PONG == resp_type) return 1;
	if (MCU_PACKET_CONTROL_REQUEST != req_type || MCU_PACKET_CONTROL_RESPONSE != MCU_PACKET_CONTROL_RESPONSE) return 0;
	resp_id = resp->message.control.device_id;
	if (MCU_DEVICE_ERROR_ID == resp_id) return 1;
	// request packet content is xored
	req_id = req->message.control.device_id ^ MCU_PACKET_XOR;
	req_cmd = req->message.control.control_code ^ MCU_PACKET_XOR;
	return (req_id == resp_id) && (req_cmd == resp->message.control.control_code);
}


static int __mcu_packet_empty(struct mcu_packet_private *mcu_packet_data)
{
	return mcu_packet_data->buffer_start == mcu_packet_data->buffer_end;
}

static int __mcu_packet_buffer_size(struct mcu_packet_private *mcu_packet_data)
{
	return mcu_packet_data->buffer_end - mcu_packet_data->buffer_start;
}

static void __mcu_packet_buffer_reset(struct mcu_packet_private *mcu_packet_data)
{
	if (likely(mcu_packet_data->buffer_start < MCU_PACKET_BUFFER_SIZE / 2)) {
		return;
	}
	mcu_packet_data->buffer_start = 0;
	mcu_packet_data->buffer_end = 0;
}

static void __mcu_packet_buffer_consume(struct mcu_packet_private *mcu_packet_data, int count)
{
	mcu_packet_data->buffer_start += count;
	if (__mcu_packet_empty(mcu_packet_data)) {
		__mcu_packet_buffer_reset(mcu_packet_data);
	}
}

static struct mcu_packet * __mcu_packet_detect(struct mcu_packet_private *mcu_packet_data)
{
	int i;
	struct mcu_packet *packet;

	if (__mcu_packet_buffer_size(mcu_packet_data) < sizeof(struct mcu_packet_header)) {
		// smaller than a packet header, ignore
		return NULL;
	}

	for (i = mcu_packet_data->buffer_start; i < mcu_packet_data->buffer_end - 1; i++) {
		if (MCU_PACKET_MAGIC0 == mcu_packet_data->buffer[i] && MCU_PACKET_MAGIC1 == mcu_packet_data->buffer[i + 1]) {
			packet = (struct mcu_packet *)&mcu_packet_data->buffer[i];
			if (mcu_packet_verify_checksum(packet)) {
				// ignore if buffer to small
				if (i + mcu_get_packet_length(packet) > mcu_packet_data->buffer_end) {
					packet = NULL;
					continue;
				}
				__mcu_packet_buffer_consume(mcu_packet_data, i + mcu_get_packet_length(packet) - mcu_packet_data->buffer_start);
				return packet;
			}
		}
	}

	// not found, try to reset buffer
	//__mcu_packet_buffer_consume(i - mcu_packet_data->buffer_start);

	return NULL;
}

static void __mcu_packet_report(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	struct mcu_packet_private *mcu_packet_data = bus->pkt_data;
	switch (packet->header.identity) {
	case MCU_PACKET_PING:
		mcu_packet_data->callback->ping(bus, packet);
		break;
	case MCU_PACKET_PONG:
		mcu_packet_data->callback->pong(bus, packet);
		break;
	case MCU_PACKET_CONTROL_REQUEST:
		mcu_packet_data->callback->new_request(bus, packet);
		break;
	case MCU_PACKET_CONTROL_RESPONSE:
		mcu_packet_data->callback->new_response(bus, packet);
		break;
	default:
		break;
	}
}

void mcu_packet_buffer_detect(struct mcu_bus_device *bus)
{
	struct mcu_packet_private *mcu_packet_data = bus->pkt_data;
	if (unlikely(!mcu_packet_data)) {
		return;
	}

	spin_lock(&mcu_packet_data->buffer_lock);
	while (1) {
		struct mcu_packet *packet = __mcu_packet_detect(mcu_packet_data);
		if (packet) {
			__mcu_packet_report(bus, packet);
		}
		else {
			break;
		}
	}
	spin_unlock(&mcu_packet_data->buffer_lock);
}

static int mcu_packet_append(struct mcu_packet_private *mcu_packet_data, const unsigned char *cp, int count)
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
			mcu_packet_data->buffer[mcu_packet_data->buffer_end++] = cp[i] ^ MCU_PACKET_XOR;
		}
	}
	spin_unlock(&mcu_packet_data->buffer_lock);

	return len;
}

int mcu_packet_receive_buffer(struct mcu_bus_device *bus, const void *cp, int count)
{
	return mcu_packet_append(bus->pkt_data, (const unsigned char *)cp, count);
}


int __init mcu_packet_init(struct mcu_bus_device *bus, struct mcu_packet_callback *callback)
{
	struct mcu_packet_private *mcu_packet_data;

	mcu_packet_data = kzalloc(sizeof(*mcu_packet_data), GFP_KERNEL);
	if (unlikely(!mcu_packet_data)) {
		return -ENOMEM;
	}
	mcu_packet_data->callback = callback;

	spin_lock_init(&mcu_packet_data->buffer_lock);

	bus->pkt_data = mcu_packet_data;
	return 0;
}

void __exit mcu_packet_deinit(struct mcu_bus_device *bus)
{
	kfree(bus->pkt_data);
	bus->pkt_data = NULL;
}

