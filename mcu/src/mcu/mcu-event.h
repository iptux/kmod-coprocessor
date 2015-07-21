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


enum mcu_event_type {
	MCU_DATA_RECEIVED,
	MCU_WRITE_COMPLETE,
	MCU_PING_DETECTED,
	MCU_PONG_DETECTED,
	MCU_CONTROL_REQUEST_DETECTED,
	MCU_CONTROL_RESPONSE_DETECTED,
	MCU_LATE_INIT,
};

struct mcu_bus_device;

struct mcu_event {
	enum mcu_event_type type;
	void *object;
	struct mcu_bus_device *bus;
	struct list_head node;
};

extern struct work_struct mcu_event_work;

struct mcu_event *mcu_get_event(void);
void mcu_free_event(struct mcu_event *event);
void mcu_remove_duplicate_events(void *object, enum mcu_event_type type);
struct mcu_event *mcu_queue_event(void *object, struct mcu_bus_device *bus, enum mcu_event_type event_type);
struct mcu_event *mcu_wait_event(struct mcu_bus_device *bus, mcu_device_id device_id, enum mcu_event_type type, int timeout);
void mcu_notify_event(struct mcu_event *event);

#endif	// __MCU_EVENT_H_

