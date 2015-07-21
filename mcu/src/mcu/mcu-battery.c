/*
 * mcu-battery.h
 * mcu coprocessor bus protocol, battery driver
 *
 * Author: Alex.wang
 * Create: 2015-07-08 13:38
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/mcu.h>
#include "mcu-internal.h"

#define MCU_BATTERY_STATUS_NOT_PRESENT 5

struct mcu_battery_private {
	struct mcu_device *device;
	struct power_supply battery;
	struct mutex mutex;

	int capacity;
	int capacity_level;
	int status;
	int health;
	int present;
};

void mcu_battery_set_status(struct mcu_battery_private *data, unsigned char status)
{
	if (MCU_BATTERY_STATUS_NOT_PRESENT == status) {
		data->present = 0;
		data->status = 0;
	}
	else {
		data->present = 1;
		data->status = status;
	}
}

void mcu_battery_set_capacity(struct mcu_battery_private *data, unsigned char capacity)
{
	data->capacity = capacity;
	data->health = POWER_SUPPLY_HEALTH_GOOD;

	if (capacity >= 99) {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_FULL;
	}
	else if (capacity >= 80) {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_HIGH;
	}
	else if (capacity >= 30) {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_NORMAL;
	}
	else if (capacity >= 10) {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_LOW;
	}
	else {
		data->capacity_level = POWER_SUPPLY_CAPACITY_LEVEL_CRITICAL;
		data->health = POWER_SUPPLY_HEALTH_DEAD;
	}
}

static int mcu_battery_command(struct mcu_battery_private *data, mcu_control_code cmd, unsigned char *value)
{
	struct mcu_device *device = data->device;
	unsigned char buffer[1] = {0};
	int ret;

	ret = mcu_device_command(data->device, cmd, buffer, sizeof(buffer));
	if (ret < 0) {
		dev_warn(&device->dev, "failed to send commad: cmd=%d\n", cmd);
	}
	else if (NULL != value) {
		*value = buffer[0];
	}

	return ret;
}

static void mcu_battery_update_status_on_demand(struct mcu_battery_private *data)
{
	unsigned char value = 0;
	if (data->status) {
		return;
	}
	if (mcu_battery_command(data, 'S', &value) == 1) {
		mcu_battery_set_status(data, value);
	}
}

static void mcu_battery_update_capacity_on_demand(struct mcu_battery_private *data)
{
	unsigned char value = 0;

	// before update capacity, update status first
	mcu_battery_update_status_on_demand(data);

	if (data->capacity) {
		return;
	}
	if (mcu_battery_command(data, 'C', &value) == 1) {
		mcu_battery_set_capacity(data, value);
	}
}

static int mcu_battery_get_property(struct power_supply *psy,
		enum power_supply_property psp,
		union power_supply_propval *val)
{
	struct mcu_battery_private *data = container_of(psy, struct mcu_battery_private, battery);

	switch (psp) {
	case POWER_SUPPLY_PROP_TECHNOLOGY:
		val->intval = POWER_SUPPLY_TECHNOLOGY_LION;
		break;
	case POWER_SUPPLY_PROP_STATUS:
		mcu_battery_update_status_on_demand(data);
		val->intval = data->status;
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		mcu_battery_update_capacity_on_demand(data);
		val->intval = data->health;
		break;
	case POWER_SUPPLY_PROP_CAPACITY:
		mcu_battery_update_capacity_on_demand(data);
		val->intval = data->capacity;
		break;
	case POWER_SUPPLY_PROP_CAPACITY_LEVEL:
		mcu_battery_update_capacity_on_demand(data);
		val->intval = data->capacity_level;
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		mcu_battery_update_status_on_demand(data);
		val->intval = data->present;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static enum power_supply_property mcu_battery_props[] = {
	POWER_SUPPLY_PROP_TECHNOLOGY,
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CAPACITY,
	POWER_SUPPLY_PROP_CAPACITY_LEVEL,
	POWER_SUPPLY_PROP_PRESENT,
};

static int mcu_battery_probe(struct mcu_device *device, const struct mcu_device_id *id)
{
	struct device *dev = &device->dev;
	struct mcu_battery_private *data;
	int ret = -ENODEV;

	switch (id->driver_data) {
	case 0:
		data = kzalloc(sizeof(struct mcu_battery_private), GFP_KERNEL);
		if (NULL == data) {
			dev_err(dev, "failed to malloc\n");
			ret = -ENOMEM;
			goto exit_alloc_data_failed;
		}

		{
			mutex_init(&data->mutex);
			mcu_set_drvdata(device, data);
			data->device = device;

			data->battery.name	= "battery";
			data->battery.type	= POWER_SUPPLY_TYPE_BATTERY;
			data->battery.get_property	= mcu_battery_get_property;
			data->battery.properties	= mcu_battery_props;
			data->battery.num_properties	= ARRAY_SIZE(mcu_battery_props);

			ret = power_supply_register(&device->dev, &data->battery);
			if (ret) {
				pr_err("mcu-battery: power_supply_register register failed\n");
				goto exit_misc_device_reg_failed;
			}

			dev_info(dev, "mcu-battery device created\n");
			ret = 0;
		}
		break;
	default:
		break;
	}

	return ret;

exit_misc_device_reg_failed:
	mutex_destroy(&data->mutex);
	kfree(data);
exit_alloc_data_failed:
	return ret;
}

static int mcu_battery_remove(struct mcu_device *device)
{
	struct mcu_battery_private *data = mcu_get_drvdata(device);

	power_supply_unregister(&data->battery);
	mutex_destroy(&data->mutex);
	kfree(data);
	return 0;
}

static void mcu_battery_report(struct mcu_device *device, mcu_control_code cmd, unsigned char *buffer, int len)
{
	struct mcu_battery_private *data = mcu_get_drvdata(device);

	if (1 != len) {
		dev_err(&device->dev, "invaild length: cmd=%d, len=%d\n", cmd, len);
		return;
	}

	switch (cmd) {
	case 'C':
		mcu_battery_set_capacity(data, buffer[0]);
		break;
	case 'S':
		mcu_battery_set_status(data, buffer[0]);
		break;
	default:
		dev_warn(&device->dev, "unknown command: cmd=%d, data=%#x\n", cmd, buffer[0]);
		break;
	}
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mcu_battery_dt_match[] = {
	{ .compatible = "lbs,mcu-battery" },
	{ },
};
MODULE_DEVICE_TABLE(of, mcu_battery_dt_match);
#endif

static struct mcu_device_id mcu_battery_id[] = {
	{ "mcu-battery", 0 },
	{ }
};

struct mcu_driver __mcu_battery = {
	.driver	= {
		.name	= "mcu-battery",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(mcu_battery_dt_match),
#endif
	},
	.probe	= mcu_battery_probe,
	.remove	= mcu_battery_remove,
	.id_table	= mcu_battery_id,
	.report	= mcu_battery_report,
};

