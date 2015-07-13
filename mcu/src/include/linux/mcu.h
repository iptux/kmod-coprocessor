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

typedef u8 mcu_id;
typedef u16 mcu_len;

struct mcu_device {
	mcu_id device_id;
	char name[MCU_NAME_SIZE];
	struct device dev;
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
	void (*report)(struct mcu_device *device, unsigned char cmd, unsigned char *buffer, mcu_len len);

	struct device_driver driver;
	const struct mcu_device_id *id_table;

	struct list_head devices;
};
#define to_mcu_driver(d) container_of(d, struct mcu_driver, driver)


static inline void *mcu_get_drvdata(struct mcu_device *device)
{
	return dev_get_drvdata(&device->dev);
}

static inline void mcu_set_drvdata(struct mcu_device *device, void *data)
{
	dev_set_drvdata(&device->dev, data);
}

/* send command with device */
extern int mcu_device_command(struct mcu_device *device, unsigned char cmd, unsigned char *buffer, mcu_len len);

/* use ping to check availability of the peer mcu */
extern int mcu_check_ping(struct mcu_device *device);

#endif	// __LINUX_MCU_H_

