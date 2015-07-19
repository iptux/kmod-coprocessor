/*
 * mcu.h
 * mcu coprocessor bus protocol
 *
 * Author: Alex.wang
 * Create: 2015-07-03 14:48
 */

#ifndef __LINUX_MCU_H_
#define __LINUX_MCU_H_

#include <linux/mod_devicetable.h>
#include <linux/device.h>

#define MCU_NAME_SIZE 20

typedef unsigned char mcu_device_id;
typedef unsigned char mcu_control_code;

struct mcu_bus_device;

struct mcu_device {
	mcu_device_id device_id;
	char name[MCU_NAME_SIZE];
	struct mcu_bus_device *bus;
	struct device dev;

	struct list_head node;
};
#define to_mcu_device(d) container_of(d, struct mcu_device, dev)

struct mcu_device_id {
	char name[MCU_NAME_SIZE];
	kernel_ulong_t driver_data;	/* Data private to the driver */
};

struct mcu_driver {
	/* Standard driver model interfaces */
	int (*probe)(struct mcu_device *, const struct mcu_device_id *);
	int (*remove)(struct mcu_device *);

	/* device report callback like command */
	void (*report)(struct mcu_device *device, mcu_control_code cmd, unsigned char *buffer, int len);

	struct device_driver driver;
	const struct mcu_device_id *id_table;

	struct list_head devices;
};
#define to_mcu_driver(d) container_of(d, struct mcu_driver, driver)

struct mcu_board_info {
	char type[MCU_NAME_SIZE];
	mcu_device_id device_id;
	struct device_node *of_node;
	void *platform_data;
};

struct mcu_device *mcu_new_device(struct mcu_bus_device *bus, struct mcu_board_info const *info);
int mcu_remove_device(struct mcu_device *device);

static inline void *mcu_get_drvdata(struct mcu_device *device)
{
	return dev_get_drvdata(&device->dev);
}

static inline void mcu_set_drvdata(struct mcu_device *device, void *data)
{
	dev_set_drvdata(&device->dev, data);
}

/* send command with device */
extern int mcu_device_command(struct mcu_device *device, mcu_control_code cmd, unsigned char *buffer, int len);

/* use ping to check availability of the peer mcu */
extern int mcu_check_ping(struct mcu_device *device);

#endif	// __LINUX_MCU_H_

