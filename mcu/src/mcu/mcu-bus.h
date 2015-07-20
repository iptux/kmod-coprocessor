/*
 * mcu-bus.h
 * mcu coprocessor bus protocol internal header
 *
 * Author: Alex.wang
 * Create: 2015-07-06 15:42
 */


#ifndef __MCU_BUS_H_
#define __MCU_BUS_H_

#include <linux/device.h>

struct mcu_bus_device {
	char name[MCU_NAME_SIZE];
	struct device dev;

	int (*late_init)(struct mcu_bus_device *);
	int (*do_write)(struct mcu_bus_device *, const void *ptr, int len);
	int nr;
	// used by mcu-packet
	void *pkt_data;
	// used by mcu-event
	spinlock_t event_lock;
	wait_queue_head_t wait_queue;
	unsigned long event_flags;
	void *event_data;

	struct completion dev_released;
	struct list_head children;
};
#define to_mcu_bus_device(d) container_of(d, struct mcu_bus_device, dev)

extern void mcu_write_complete(struct mcu_bus_device *);
extern int mcu_receive(struct mcu_bus_device *, const unsigned char *, size_t);

extern int mcu_add_bus_device(struct mcu_bus_device *);
extern void mcu_remove_bus_device(struct mcu_bus_device *);

#endif	// __MCU_BUS_H_

