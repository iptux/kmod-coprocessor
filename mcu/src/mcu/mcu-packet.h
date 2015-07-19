/*
 * mcu-packet.h
 * mcu coprocessor bus protocol, packet defines
 *
 * Author: Alex.wang
 * Create: 2015-07-16 19:40
 */


#ifndef __MCU_PACK_H_
#define __MCU_PACK_H_

#include <linux/init.h>
#include "linux/mcu.h"

struct mcu_packet;

struct mcu_packet_callback {
	/* low level write operation */
	int (*write)(const void *cp, int count);

	/* ping request detected */
	void (*ping)(void);

	/* ping response detected */
	void (*pong)(void);

	/* device control request detected */
	void (*new_request)(mcu_device_id device_id, mcu_control_code ctl_code, const void *buffer, int count);

	/* device control response detected */
	void (*new_response)(mcu_device_id device_id, mcu_control_code ctl_code, const void *buffer, int count);
};

extern int mcu_packet_init(struct mcu_packet_callback *callback) __init;
extern void mcu_packet_deinit(void) __exit;

/* the send packet should not be free before got reply */
extern void mcu_packet_free(struct mcu_packet *);

extern struct mcu_packet *mcu_packet_send_ping(void);
extern struct mcu_packet *mcu_packet_send_pong(void);
extern struct mcu_packet *mcu_packet_send_control_request(mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len);
extern struct mcu_packet *mcu_packet_send_control_response(mcu_device_id device_id, mcu_control_code control_code, const void *cp, int len);

extern int mcu_packet_receive_buffer(const void *cp, int count);

/* try to detect packet in buffer, should be called after mcu_packet_receive_buffer */
extern void mcu_packet_buffer_detect(void);

#endif	//  __MCU_PACK_H_

