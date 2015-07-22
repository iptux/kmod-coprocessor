/*
 * mcu-event.c
 * mcu bus, event define and handling
 *
 * Author: Alex.wang
 * Create: 2015-07-18 20:38
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include "mcu-event.h"
#include "mcu-bus.h"


static DEFINE_SPINLOCK(mcu_event_lock);	/* protects mcu_event_list */
static LIST_HEAD(mcu_event_list);

static struct mcu_event *mcu_event_find_response(struct mcu_bus_device *bus, const struct mcu_packet *req, enum mcu_event_type type, struct mcu_event **eventp)
{
	struct mcu_event *event, *next;
	list_for_each_entry_safe(event, next, &bus->event_list, node) {
		if (event->type == type && event->bus == bus && mcu_packet_response_to(req, event->object)) {
			*eventp = event;
			return event;
		}
	}

	return NULL;
}

struct mcu_event *mcu_wait_event(struct mcu_bus_device *bus, const struct mcu_packet *packet, enum mcu_event_type type, int timeout)
{
	struct mcu_event *event = NULL;
	unsigned long flags;
	int ret;
	int loop = 10;

	while (loop--) {
		ret = wait_event_interruptible_timeout(bus->wait_queue, mcu_event_find_response(bus, packet, type, &event), msecs_to_jiffies(timeout));
		if (ret <= 0) {
			// timeout
			break;
		}
		spin_lock_irqsave(&bus->event_lock, flags);
		if (event) {
			list_del(&event->node);
			goto found;
		}
		spin_unlock_irqrestore(&bus->event_lock, flags);

		// event list not empty, wait more event
		io_schedule();
	}

	return NULL;

found:
	spin_unlock_irqrestore(&bus->event_lock, flags);
	return event;
}

void mcu_notify_event(struct mcu_event *event)
{
	unsigned long flags;
	struct mcu_bus_device *bus = event->bus;
	if (unlikely(!bus)) {
		return;
	}

	spin_lock_irqsave(&bus->event_lock, flags);
	if (waitqueue_active(&bus->wait_queue)) {
		list_add_tail(&event->node, &bus->event_list);
		wake_up(&bus->wait_queue);
	}

	spin_unlock_irqrestore(&bus->event_lock, flags);
}


struct mcu_event *mcu_get_event(void)
{
	struct mcu_event *event = NULL;
	unsigned long flags;

	spin_lock_irqsave(&mcu_event_lock, flags);

	if (!list_empty(&mcu_event_list)) {
		event = list_first_entry(&mcu_event_list, struct mcu_event, node);
		list_del_init(&event->node);
	}

	spin_unlock_irqrestore(&mcu_event_lock, flags);
	return event;
}

void mcu_free_event(struct mcu_event *event)
{
	kfree(event);
}

void mcu_remove_duplicate_events(void *object, enum mcu_event_type type)
{
	struct mcu_event *e, *next;
	unsigned long flags;

	spin_lock_irqsave(&mcu_event_lock, flags);

	list_for_each_entry_safe(e, next, &mcu_event_list, node) {
		if (object == e->object) {
			/*
			 * If this event is of different type we should not
			 * look further - we only suppress duplicate events
			 * that were sent back-to-back.
			 */
			if (type != e->type)
				break;

			list_del_init(&e->node);
			mcu_free_event(e);
		}
	}

	spin_unlock_irqrestore(&mcu_event_lock, flags);
}

struct mcu_event *mcu_queue_event(void *object, struct mcu_bus_device *bus, enum mcu_event_type event_type)
{
	unsigned long flags;
	struct mcu_event *event = NULL;

	spin_lock_irqsave(&mcu_event_lock, flags);

	event = kmalloc(sizeof(struct mcu_event), GFP_ATOMIC);
	if (!event) {
		pr_err("Not enough memory to queue event %d\n", event_type);
		goto out;
	}

	event->type = event_type;
	event->object = object;
	event->bus = bus;

	list_add_tail(&event->node, &mcu_event_list);
	queue_work(system_long_wq, &mcu_event_work);

out:
	spin_unlock_irqrestore(&mcu_event_lock, flags);
	return event;
}

