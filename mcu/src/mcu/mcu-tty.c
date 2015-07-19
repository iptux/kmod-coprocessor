/*
 * mcu-tty.c
 * mcu bus, tty backend
 *
 * Author: Alex.wang
 * Create: 2015-07-19 22:02
 */


#include <linux/module.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/serial.h>
#include <linux/of_platform.h>
#include <linux/mcu.h>
#include "mcu-internal.h"


struct mcu_tty_private {
	struct mcu_bus_device bus;
	struct device *dev;
	struct file *filp;
	char tty_name[20];
};

static int mcu_tty_write(struct mcu_bus_device *device, const void *buffer, int count)
{
	struct mcu_tty_private *data = container_of(device, struct mcu_tty_private, bus);
	loff_t offset = 0;
	return __kernel_write(data->filp, buffer, count, &offset);
}

static long mcu_tty_ioctl(struct file *filp, unsigned op, unsigned long param)
{
	long ret = -ENOSYS;
	if (filp->f_op->unlocked_ioctl) {
		ret = filp->f_op->unlocked_ioctl(filp, op, param);
	}

	return ret;
}

static void mcu_tty_setup(struct file *filp)
{
	struct termios termios;
	struct serial_struct serial;
	mm_segment_t oldfs;
	int ldisc = N_MCU;

	oldfs = get_fs();
	set_fs(KERNEL_DS);

	mcu_tty_ioctl(filp, TCGETS, (unsigned long)&termios);
	termios.c_iflag = 0;
	termios.c_oflag = 0;
	termios.c_lflag = 0;
#if 0
	termios.c_cflag = CLOCAL | CS8 | CREAD | B9600;
#else
	termios.c_cflag &= ~CBAUD;
	termios.c_cflag |= B9600;
	termios.c_cflag |= (CLOCAL | CREAD);
	termios.c_cflag |= PARENB;
	termios.c_cflag &= ~(PARODD | CSTOPB | CSIZE);
	termios.c_cflag |= CS8;
#endif
	termios.c_cc[VMIN] = 0;
	termios.c_cc[VTIME] = 0;
	mcu_tty_ioctl(filp, TCSETS, (unsigned long)&termios);

	/* Set low latency */
	mcu_tty_ioctl(filp, TIOCGSERIAL, (unsigned long)&serial);
	serial.flags |= ASYNC_LOW_LATENCY;
	mcu_tty_ioctl(filp, TIOCSSERIAL, (unsigned long)&serial);

	mcu_tty_ioctl(filp, TIOCSETD, (unsigned long)&ldisc);

	set_fs(oldfs);
}

static int mcu_tty_late_init(struct mcu_bus_device *device)
{
	struct mcu_tty_private *data = container_of(device, struct mcu_tty_private, bus);

	if (data && data->tty_name[0]) {
		// must be opened in a kthread
		data->filp = filp_open(data->tty_name, O_RDWR | O_NOCTTY, 0);
		if (IS_ERR(data->filp)) {
			DBG("Failed to open port %s: ret=%d", data->tty_name, (int)PTR_ERR(data->filp));
			return PTR_ERR(data->filp);
		}
		mcu_tty_setup(data->filp);
	}

	return 0;
}

#if IS_ENABLED(CONFIG_OF)
static const struct of_device_id mcu_tty_of_match[] = {
	{ .compatible = "lbs,mcu-tty" },
	{},
};
MODULE_DEVICE_TABLE(of, mcu_tty_of_match);
#else
static const struct of_device_id mcu_tty_of_match;
#endif

static int mcu_tty_probe(struct platform_device *op)
{
	const struct of_device_id *match;
	struct mcu_tty_private *data;
	int ret;
	const char *name;

	match = of_match_device(mcu_tty_of_match, &op->dev);
	if (!match)
		return -EINVAL;

	data = kzalloc(sizeof(struct mcu_tty_private), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->dev = &op->dev;

	ret = of_property_read_string(op->dev.of_node, "lbs,tty-name", &name);
	if (ret) {
		dev_err(data->dev, "no tty name\n");
		goto fail_prop;
	}
	strncpy(data->tty_name, name, sizeof(data->tty_name));

	platform_set_drvdata(op, data);

	snprintf(data->bus.name, sizeof(data->bus.name), "mcu-tty.%p", data);
	data->bus.late_init = mcu_tty_late_init;
	data->bus.do_write = mcu_tty_write;
	data->bus.dev.parent = &op->dev;
	data->bus.dev.of_node = of_node_get(op->dev.of_node);

	ret = mcu_add_bus_device(&data->bus);
	if (ret < 0) {
		dev_err(data->dev, "failed to add mcu bus: ret=%d\n", ret);
		goto fail_add;
	}

	return ret;

fail_add:
fail_prop:
	kfree(data);
	return ret;
}

static int mcu_tty_remove(struct platform_device *op)
{
	struct mcu_tty_private *data = platform_get_drvdata(op);
	mcu_remove_bus_device(&data->bus);
	kfree(data);
	return 0;
}

static struct platform_driver mcu_tty_driver = {
	.probe	= mcu_tty_probe,
	.remove	= mcu_tty_remove,
	.driver	= {
		.owner	= THIS_MODULE,
		.name	= "mcu-tty",
#if IS_ENABLED(CONFIG_OF)
		.of_match_table	= of_match_ptr(mcu_tty_of_match),
#endif
	},
};

int __init mcu_tty_init(void)
{
	return platform_driver_register(&mcu_tty_driver);
}

void __exit mcu_tty_exit(void)
{
	platform_driver_unregister(&mcu_tty_driver);
}

