/*
 * mcu-event.h
 * mcu bus, event define and handling
 *
 * Author: Alex.wang
 * Create: 2015-07-19 12:11
 */


#ifndef __MCU_EVENT_H_
#define __MCU_EVENT_H_

#include <linux/module.h>
#include "mcu-packet.h"


#define MCU_EVENT_HANDLED	1
#define MCU_EVENT_SENT	2
#define MCU_EVENT_PENDING	3

enum mcu_event_type {
	MCU_DATA_RECEIVED,
	MCU_WRITE_COMPLETE,
	MCU_PING_DETECTED,
	MCU_PONG_DETECTED,
	MCU_CONTROL_REQUEST_DETECTED,
	MCU_CONTROL_RESPONSE_DETECTED,
	MCU_ATTACH_DRIVER,
	MCU_SETUP_TTY,
	MCU_CLOSE_TTY,
};

struct mcu_event {
	enum mcu_event_type type;
	unsigned long flags;
	void *object;
	struct module *owner;
	struct list_head node;
};


extern struct work_struct mcu_event_work;

struct mcu_event *mcu_get_event(void);
void mcu_free_event(struct mcu_event *event);
void mcu_remove_duplicate_events(void *object, enum mcu_event_type type);
int __mcu_queue_event(void *object, struct module *owner, enum mcu_event_type event_type);
void mcu_remove_pending_events(void *object);

#define mcu_queue_event(object, type) __mcu_queue_event((object), THIS_MODULE, (type))


extern void mcu_queue_wait(struct mcu_event *event);
extern struct mcu_event *mcu_get_wait(enum mcu_event_type type);
extern int mcu_wait_event(struct mcu_event *event, int what, int timeout);
extern void mcu_wait_set(struct mcu_event *event, int what);

#endif	// __MCU_EVENT_H_

