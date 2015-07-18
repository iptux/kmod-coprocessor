/*
 * mcu-gpio.h
 * mcu coprocessor bus protocol, gpio driver
 *
 * Author: Alex.wang
 * Create: 2015-07-08 13:13
 */

#include <linux/slab.h>
#include <linux/of.h>
#include <linux/gpio.h>
#include "mcu-internal.h"

struct mcu_gpio_private {
	struct gpio_chip chip;
	struct mcu_device *device;
};
#define to_mcu_gpio(chip) container_of(chip, struct mcu_gpio_private, chip)

static int mcu_gpio_command(struct gpio_chip *chip, unsigned char cmd, unsigned offset, int *value)
{
	struct mcu_gpio_private *data = to_mcu_gpio(chip);
	struct mcu_device *device = data->device;
	unsigned char buffer[1] = {offset};
	int ret;

	ret = mcu_device_command(data->device, cmd, buffer, sizeof(buffer));
	if (ret) {
		dev_warn(&device->dev, "failed to send commad: cmd=%c, gpio=%d\n", cmd, offset);
	}
	else if (NULL != value) {
		*value = buffer[0];
	}

	return ret;
}

static int mcu_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	int ret, value;

	ret = mcu_gpio_command(chip, 'r', offset, &value);
	return 0 == ret ? value : 0;
}

static void mcu_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	mcu_gpio_command(chip, value ? 'h' : 'l', offset, NULL);
}

static int mcu_gpio_get_direction(struct gpio_chip *chip, unsigned offset)
{
	int ret, value;

	ret = mcu_gpio_command(chip, 'r', offset, &value);
	return 0 == ret ? value : GPIOF_DIR_OUT;
}

static int mcu_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	return mcu_gpio_command(chip, 'i', offset, NULL);
}

static int mcu_gpio_direction_output(struct gpio_chip *chip, unsigned offset, int value)
{
	int ret;

	ret = mcu_gpio_command(chip, 'o', offset, NULL);

	return 0 == ret ? ret : mcu_gpio_command(chip, value ? 'h' : 'l', offset, NULL);
}

static void mcu_gpio_report(struct mcu_device *device, mcu_control_code cmd, unsigned char *buffer, int len)
{
	struct mcu_gpio_private *data = mcu_get_drvdata(device);

}

static int mcu_gpio_probe(struct mcu_device *device, const struct mcu_device_id *id)
{
	struct device_node *np = device->dev.of_node;
	struct mcu_gpio_private *data;
	int ret;

	switch (id->driver_data) {
	case 0:
		break;
	default:
		return -ENODEV;
	}

	data = kzalloc(sizeof(struct mcu_gpio_private), GFP_KERNEL);

	data->chip.dev = &device->dev;
	data->chip.label = dev_name(&device->dev);
	data->chip.of_node = np;
	data->chip.ngpio = ;
	data->chip.get_direction = mcu_gpio_get_direction;
	data->chip.direction_input = mcu_gpio_direction_input;
	data->chip.direction_output = mcu_gpio_direction_output;
	data->chip.get = mcu_gpio_get;
	data->chip.set = mcu_gpio_set;

	return gpiochip_add(&data->chip);
}

static int mcu_gpio_remove(struct mcu_device *device)
{
	struct mcu_gpio_private *data = mcu_get_drvdata(device);

	gpiochip_remove(&data->chip);

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mcu_gpio_match[] = {
	{ .compatible = "lbs,mcu-gpio" },
	{},
};
MODULE_DEVICE_TABLE(of, mcu_gpio_match);
#endif


static struct mcu_device_id mcu_gpio_id[] = {
	{ "mcu-gpio", 0 },
	{ }
};

struct mcu_driver __mcu_gpio = {
	.driver = {
		.name = "mcu-gpio",
		.owner = THIS_MODULE,
#if IS_ENABLED(CONFIG_OF)
		.of_match_table = of_match_ptr(mcu_gpio_match),
#endif
	},
	.probe	= mcu_gpio_probe,
	.remove	= mcu_gpio_remove,
	.id_table	= mcu_gpio_id,
	.report	= mcu_gpio_report,
};

