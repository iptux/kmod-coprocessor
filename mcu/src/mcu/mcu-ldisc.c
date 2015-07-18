/*
 * mcu-ldisc.c
 * mcu coprocessor bus protocol, low level driver tty
 *
 * Author: Alex.wang
 * Create: 2015-07-06 10:36
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/tty.h>
#include <linux/mcu.h>
#include "mcu-bus.h"
#include "mcu-internal.h"

struct sermcu {
	struct tty_struct *tty;
	struct mcu_bus_device *mcu;
	spinlock_t lock;
};

static ssize_t sermcu_ldisc_write(struct tty_struct *tty, struct file *file, const unsigned char *buf, size_t nr)
{
	if (tty && tty->ops && tty->ops->write)
		return tty->ops->write(tty, buf, nr);
	return -EFAULT;
}

static void sermcu_ldisc_receive(struct tty_struct *tty, const unsigned char *cp, char *fp, int count)
{
	struct sermcu *sermcu = (struct sermcu *)tty->disc_data;
	unsigned long flags;

	spin_lock_irqsave(&sermcu->lock, flags);

	while (count--) {
		if (fp && *fp++) {
			// got an error
			cp++;
			continue;
		}
		mcu_receive(sermcu->mcu, cp++, 1);
	}
	spin_unlock_irqrestore(&sermcu->lock, flags);
}

static void sermcu_ldisc_write_wakeup(struct tty_struct *tty)
{
	struct sermcu *sermcu = (struct sermcu *)tty->disc_data;
	unsigned long flags;

	spin_lock_irqsave(&sermcu->lock, flags);
	mcu_write_complete(sermcu->mcu);
	spin_unlock_irqrestore(&sermcu->lock, flags);
}

static int sermcu_ldisc_open(struct tty_struct *tty)
{
	struct sermcu *sermcu;

	DBG("");
	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	sermcu = kzalloc(sizeof(struct sermcu), GFP_KERNEL);
	if (!sermcu)
		return -ENOMEM;

	sermcu->tty = tty;
	spin_lock_init(&sermcu->lock);
	tty->disc_data = sermcu;
	tty->receive_room = 256;
	set_bit(TTY_DO_WRITE_WAKEUP, &tty->flags);

	return 0;
}

static void sermcu_ldisc_close(struct tty_struct *tty)
{
	struct sermcu *sermcu = (struct sermcu *)tty->disc_data;

	kfree(sermcu);
}

static struct tty_ldisc_ops sermcu_ldisc = {
	.owner	= THIS_MODULE,
	.magic	= TTY_LDISC_MAGIC,
	.name	= "mcu",
	.open	= sermcu_ldisc_open,
	.close	= sermcu_ldisc_close,
	.write	= sermcu_ldisc_write,
	.receive_buf	= sermcu_ldisc_receive,
	.write_wakeup	= sermcu_ldisc_write_wakeup,
};

int __init sermcu_init(void)
{
	int ret;
	DBG("");
	ret = tty_register_ldisc(N_MCU, &sermcu_ldisc);
	if (ret)
		pr_err("sermcu: Error register line discipline, err=%d\n", ret);

	return ret;
}

void __exit sermcu_exit(void)
{
	DBG("");
	tty_unregister_ldisc(N_MCU);
}

