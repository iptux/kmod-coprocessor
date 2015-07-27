/*
 * mcu-core.c
 * mcu coprocessor bus protocol
 *
 * Author: Alex.wang
 * Create: 2015-07-06 10:36
 */


#include <linux/slab.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/sched.h>
#include <linux/delay.h>
#include <linux/mcu.h>
#include "mcu-internal.h"
#include "mcu-packet.h"
#include "mcu-event.h"


DEFINE_MUTEX(mcu_mutex);
DEFINE_IDR(mcu_bus_idr);

struct bus_type mcu_bus_type;
struct device_type mcu_bus_dev_type;
struct device_type mcu_dev_type;


/* send command with device */
int mcu_device_command(struct mcu_device *device, mcu_control_code cmd, unsigned char *buffer, int len)
{
	struct mcu_packet *packet, *reply;
	struct mcu_event *event;
	int ret = 0;

	packet = mcu_packet_send_control_request(device->bus, device->device_id, cmd, buffer, len);
	if (unlikely(!packet)) {
		return -EFAULT;
	}

	// wait for reply
	event = mcu_wait_event(device->bus, packet, MCU_CONTROL_RESPONSE_DETECTED, 3000);
	if (unlikely(!event)) {
		ret = -ETIME;
		goto exit_free_packet;
	}
	reply = (struct mcu_packet *)event->object;
	ret = mcu_packet_copy_control_detail(reply, buffer, &len);
	if (ret < 0) {
		// error code
	}
	else if (ret < len) {
		// buffer too small
		ret = -ENOSPC;
	}
	else {
	}
	mcu_free_event(event);

exit_free_packet:
	mcu_packet_free(packet);
	return ret;
}

/* use ping to check availability of the peer mcu */
int mcu_check_ping(struct mcu_device *device)
{
	struct mcu_packet *packet;
	struct mcu_event *event;
	int ret = 0;

	packet = mcu_packet_send_ping(device->bus);
	if (unlikely(!packet)) {
		return -EFAULT;
	}

	// wait for reply
	event = mcu_wait_event(device->bus, packet, MCU_PONG_DETECTED, 3000);
	if (unlikely(!event)) {
		ret = -ETIME;
		goto exit_free_packet;
	}
	mcu_free_event(event);

exit_free_packet:
	mcu_packet_free(packet);
	return ret;
}

void mcu_write_complete(struct mcu_bus_device *bus)
{
	mcu_queue_event(NULL, bus, MCU_WRITE_COMPLETE);
}

int mcu_receive(struct mcu_bus_device *bus, const unsigned char *cp, size_t count)
{
	int ret;
	if (!bus) {
		pr_warn("mcu: receive data don't have a bus: %d bytes dropped\n", count);
		return -EFAULT;
	}
	ret = mcu_packet_receive_buffer(bus, cp, count);
	mcu_queue_event(NULL, bus, MCU_DATA_RECEIVED);
	return ret;
}

static void mcu_dev_release(struct device *dev)
{
	kfree(to_mcu_device(dev));
}

struct device_type mcu_dev_type = {
	.release	= mcu_dev_release,
};

static struct mcu_device *mcu_find_device(struct mcu_bus_device *bus, mcu_device_id id)
{
	struct mcu_device *device;
	list_for_each_entry(device, &bus->children, node) {
		if (device->device_id == id) {
			return device;
		}
	}
	return NULL;
}

static void mcu_handle_request(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	struct mcu_device *device;
	struct mcu_driver *driver;
	mcu_device_id device_id;
	mcu_control_code control_code;
	int detail_len;
	unsigned char *p = (unsigned char *)packet;

	if (mcu_packet_extract_control_info(packet, &device_id, &control_code, &detail_len) < 0) {
		return;
	}

	device = mcu_find_device(bus, device_id);
	if (!device) {
		return;
	}

	driver = to_mcu_driver(device->dev.driver);

	if (driver && driver->report) {
		driver->report(device, control_code, &p[MCU_PACKET_DETAIL_OFFSET], detail_len);
	}
}

struct mcu_device *mcu_new_device(struct mcu_bus_device *bus, struct mcu_board_info const *info)
{
	struct mcu_device *device;
	int ret;

	if (mcu_find_device(bus, info->device_id)) {
		dev_err(&bus->dev, "id[%d] on bus [%s] already exists", info->device_id, bus->name);
		return NULL;
	}

	device = kzalloc(sizeof(struct mcu_bus_device), GFP_KERNEL);
	if (!device)
		return NULL;

	device->dev.platform_data = info->platform_data;
	device->device_id = info->device_id;
	device->bus = bus;
	strlcpy(device->name, info->type, sizeof(device->name));
	device->dev.bus = &mcu_bus_type;
	device->dev.type = &mcu_dev_type;
	device->dev.of_node = info->of_node;

	dev_set_name(&device->dev, "mcu%d-%x", bus->nr, info->device_id);
	ret = device_register(&device->dev);
	if (ret)
		goto reg_err;

	list_add_tail(&device->node, &bus->children);
	dev_dbg(&bus->dev, "device [%s] registered with bus id %s\n", device->name, dev_name(&device->dev));
	return device;

reg_err:
	kfree(device);
	return NULL;
}

void mcu_unregister_device(struct mcu_device *device)
{
	device_unregister(&device->dev);
}

int mcu_remove_device(struct mcu_device *device)
{
	mcu_unregister_device(device);
	return 0;
}

static void mcu_bus_dev_release(struct device *dev)
{
	struct mcu_bus_device *bus = to_mcu_bus_device(dev);
	complete(&bus->dev_released);
}

struct device_type mcu_bus_dev_type = {
	.release	= mcu_bus_dev_release,
};

#if IS_ENABLED(CONFIG_OF)
static void of_mcu_register_devices(struct mcu_bus_device *bus)
{
	struct device_node *node;
	void *ret;
	for_each_available_child_of_node(bus->dev.of_node, node) {
		struct mcu_board_info info = {};
		int reg = MCU_DEVICE_ERROR_ID;

		of_property_read_u32(node, "reg", &reg);
		if (MCU_DEVICE_ERROR_ID == reg) {
			dev_err(&bus->dev, "of_mcu: invaild reg on %s\n", node->full_name);
			continue;
		}

		strncpy(info.type, node->full_name, sizeof(info.type));
		info.device_id = reg;
		info.of_node = of_node_get(node);
		request_module(info.type);
		ret = mcu_new_device(bus, &info);
		if (NULL == ret) {
			dev_err(&bus->dev, "of_mcu: failed to register %s\n", node->full_name);
			of_node_put(node);
			continue;
		}
	}
}
#else
static void of_mcu_register_devices(struct mcu_bus_device *dev) {}
#endif

static int __mcu_packet_write(struct mcu_bus_device *bus, const void *cp, int count)
{
	if (!bus || !bus->do_write) {
		return -EINVAL;
	}
	return bus->do_write(bus, cp, count);
}

static void __mcu_packet_ping(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	mcu_queue_event(packet, bus, MCU_PING_DETECTED);
}

static void __mcu_packet_pong(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	mcu_queue_event(packet, bus, MCU_PONG_DETECTED);
}

static void __mcu_packet_new_request(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	mcu_queue_event(packet, bus, MCU_CONTROL_REQUEST_DETECTED);
}

static void __mcu_packet_new_response(struct mcu_bus_device *bus, struct mcu_packet *packet)
{
	mcu_queue_event(packet, bus, MCU_CONTROL_RESPONSE_DETECTED);
}

static struct mcu_packet_callback __packet_callback = {
	.write	= __mcu_packet_write,
	.ping	= __mcu_packet_ping,
	.pong	= __mcu_packet_pong,
	.new_request	= __mcu_packet_new_request,
	.new_response	= __mcu_packet_new_response,
};

int mcu_register_bus_device(struct mcu_bus_device *bus)
{
	int ret;
	INIT_LIST_HEAD(&bus->children);
	dev_set_name(&bus->dev, "mcu-%d", bus->nr);
	bus->dev.bus = &mcu_bus_type;
	bus->dev.type = &mcu_bus_dev_type;
	ret = device_register(&bus->dev);
	if (ret)
		goto out;

	dev_dbg(&bus->dev, "bus [%d] registered\n", bus->nr);

	spin_lock_init(&bus->event_lock);
	init_waitqueue_head(&bus->wait_queue);
	INIT_LIST_HEAD(&bus->event_list);

	mcu_packet_init(bus, &__packet_callback);
	mcu_queue_event(NULL, bus, MCU_LATE_INIT);
	of_mcu_register_devices(bus);
	return 0;

out:
	mutex_lock(&mcu_mutex);
	idr_remove(&mcu_bus_idr, bus->nr);
	mutex_unlock(&mcu_mutex);
	return ret;
}

int mcu_add_bus_device(struct mcu_bus_device *bus)
{
	int id;
	mutex_lock(&mcu_mutex);
	id = idr_alloc(&mcu_bus_idr, bus, 0, 0, GFP_KERNEL);
	mutex_unlock(&mcu_mutex);
	if (id < 0)
		return id;

	bus->nr = id;
	return mcu_register_bus_device(bus);
}

void mcu_remove_bus_device(struct mcu_bus_device *bus)
{
	struct mcu_device *d, *_n;

	list_for_each_entry_safe(d, _n, &bus->children, node) {
		list_del(&d->node);
		mcu_remove_device(d);
	}

	mcu_packet_deinit(bus);
}

static int mcu_do_add_bus(struct mcu_driver *driver, struct mcu_bus_device *bus)
{
	return 0;
}

int mcu_for_each_dev(void *data, int (*fn)(struct device *, void *))
{
	int ret;

	mutex_lock(&mcu_mutex);
	ret = bus_for_each_dev(&mcu_bus_type, NULL, data, fn);
	mutex_unlock(&mcu_mutex);

	return ret;
}

static int __mcu_process_new_driver(struct device *dev, void *data)
{
	if (dev->type != &mcu_bus_dev_type)
		return 0;
	return mcu_do_add_bus(data, to_mcu_bus_device(dev));
}

int __mcu_register_driver(struct mcu_driver *drv, struct module *owner, const char *mod_name)
{
	int ret;

	drv->driver.bus = &mcu_bus_type;
	drv->driver.owner = owner;
	drv->driver.mod_name = mod_name;

	ret = driver_register(&drv->driver);
	if (ret) {
		pr_err("driver_register() failed for %s, error=%d\n", drv->driver.name, ret);
		return ret;
	}

	INIT_LIST_HEAD(&drv->devices);
	mcu_for_each_dev(drv, __mcu_process_new_driver);

	return ret;
}
#define mcu_register_driver(driver) __mcu_register_driver(driver, THIS_MODULE, KBUILD_MODNAME)

void mcu_unregister_driver(struct mcu_driver *drv)
{
	driver_unregister(&drv->driver);
}

static void mcu_handle_event(struct work_struct *work)
{
	struct mcu_event *event;
	struct mcu_packet *packet;
	int do_free = 1;

#define NOTIFY_EVENT(e)	do {	\
		mcu_notify_event(e);	\
		do_free = 0;	\
	} while (0)

	mutex_lock(&mcu_mutex);

	while ((event = mcu_get_event())) {
		switch (event->type) {
		case MCU_WRITE_COMPLETE:
			//NOTIFY_EVENT(event);
			break;
		case MCU_DATA_RECEIVED:
			mcu_packet_buffer_detect(event->bus);
			break;
		case MCU_PING_DETECTED:
			packet = mcu_packet_send_pong(event->bus);
			if (likely(packet))
				mcu_packet_free(packet);
			break;
		case MCU_PONG_DETECTED:
			NOTIFY_EVENT(event);
			break;
		case MCU_CONTROL_REQUEST_DETECTED:
			mcu_handle_request(event->bus, event->object);
			break;
		case MCU_CONTROL_RESPONSE_DETECTED:
			NOTIFY_EVENT(event);
			break;
		case MCU_LATE_INIT:
			event->bus->late_init(event->bus);
			break;
		}

		//mcu_remove_duplicate_events(event->object, event->type);
		if (do_free) {
			mcu_free_event(event);
		}
		do_free = 1;
	}

	mutex_unlock(&mcu_mutex);
}

DECLARE_WORK(mcu_event_work, mcu_handle_event);


static const struct mcu_device_id *mcu_match_id(const struct mcu_device_id *id, const struct mcu_device *device)
{
	while (id->name[0]) {
		if (strcmp(device->name, id->name) == 0)
			return id;
		id++;
	}
	return NULL;
}

static int mcu_bus_match(struct device *dev, struct device_driver *drv)
{
	struct mcu_device *device = to_mcu_device(dev);
	struct mcu_driver *driver = to_mcu_driver(drv);

	/* try of style matching */
	if (of_driver_match_device(dev, drv)) {
		return 1;
	}

	if (!device)
		return 0;

	if (!driver || !driver->id_table)
		return 0;

	if (driver->id_table)
		return mcu_match_id(driver->id_table, device) != NULL;

	return 0;
}

static int mcu_driver_probe(struct device *dev)
{
	struct mcu_device *device = to_mcu_device(dev);
	struct mcu_driver *driver;
	int ret;

	if (!device)
		return 0;

	driver = to_mcu_driver(dev->driver);
	if (!driver || !driver->probe)
		return -ENODEV;

	// TODO: should find the index of driver->id_table
	ret = driver->probe(device, driver->id_table);
	return ret;
}

static int mcu_driver_remove(struct device *dev)
{
	struct mcu_device *device = to_mcu_device(dev);
	struct mcu_driver *driver;

	if (!device || !dev->driver)
		return 0;

	driver = to_mcu_driver(dev->driver);
	if (!driver->remove) {
		dev->driver = NULL;
		return 0;
	}

	return driver->remove(device);
}

static void mcu_shutdown(struct device *dev)
{
}

struct bus_type mcu_bus_type = {
	.name	= "mcu",
	.match	= mcu_bus_match,
	.probe	= mcu_driver_probe,
	.remove	= mcu_driver_remove,
	.shutdown	= mcu_shutdown,
};

static struct mcu_driver *mcu_all_drivers[] = {
#ifdef CONFIG_MCU_BATTERY
	&__mcu_battery,
#endif
#ifdef CONFIG_MCU_OLED
	&__mcu_oled,
#endif
#ifdef CONFIG_MCU_GPIO
	&__mcu_gpio,
#endif
};

static int __init mcu_init(void)
{
	int ret;
	int i;
	ret = bus_register(&mcu_bus_type);
	if (ret) {
		pr_err("Failed to register mcu bus, error=%d\n", ret);
		return ret;
	}

#ifdef CONFIG_MCU_LDISC
	sermcu_init();
#endif

	for (i = 0; i < ARRAY_SIZE(mcu_all_drivers); i++) {
		mcu_register_driver(mcu_all_drivers[i]);
	}

	ret = mcu_tty_init();
	if (ret) {
		pr_warn("Failed to register mcu-tty driver, error=%d\n", ret);
	}

	return 0;
}

static void __exit mcu_exit(void)
{
	int i;

	mcu_tty_exit();

	for (i = 0; i < ARRAY_SIZE(mcu_all_drivers); i++) {
		mcu_unregister_driver(mcu_all_drivers[i]);
	}

#ifdef CONFIG_MCU_LDISC
	sermcu_exit();
#endif

	bus_unregister(&mcu_bus_type);

	//cancel_work_sync(&mcu_event_work);
}

module_init(mcu_init);
module_exit(mcu_exit);

MODULE_AUTHOR("Tommy Alex <iptux7@gmail.com>");
MODULE_DESCRIPTION("MCU coprocessor bus core");
MODULE_LICENSE("GPL");

