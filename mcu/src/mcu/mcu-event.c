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

struct mcu_event *mcu_wait_event(struct mcu_bus_device *bus, enum mcu_event_type type, int timeout)
{
	struct mcu_event *event = NULL;
	unsigned long flags;
	int ret;

	while (1) {
		ret = wait_event_interruptible_timeout(bus->wait_queue, test_bit(type, &bus->event_flags), msecs_to_jiffies(timeout));
		if (ret <= 0) {
			// timeout
			break;
		}
		spin_lock_irqsave(&bus->event_lock, flags);
		// clear bit to avoid infinit loop
		clear_bit(type, &bus->event_flags);
		event = bus->event_data;
		if (event && event->type == type && event->bus == bus) {
			// found
			spin_unlock_irqrestore(&bus->event_lock, flags);
			break;
		}
		mcu_free_event(event);
		event = NULL;
		spin_unlock_irqrestore(&bus->event_lock, flags);
	}

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
		bus->event_data = event;
		set_bit(event->type, &bus->event_flags);
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
	module_put(event->owner);
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

struct mcu_event *__mcu_queue_event(void *object, struct mcu_bus_device *bus, struct module *owner, enum mcu_event_type event_type)
{
	unsigned long flags;
	struct mcu_event *event = NULL;

	spin_lock_irqsave(&mcu_event_lock, flags);

	/*
	 * Scan event list for the other events for the same serio port,
	 * starting with the most recent one. If event is the same we
	 * do not need add new one. If event is of different type we
	 * need to add this event and should not look further because
	 * we need to preseve sequence of distinct events.
	 */
	list_for_each_entry_reverse(event, &mcu_event_list, node) {
		if (event->object == object) {
			if (event->type == event_type)
				goto out;
			break;
		}
	}

	event = kmalloc(sizeof(struct mcu_event), GFP_ATOMIC);
	if (!event) {
		pr_err("Not enough memory to queue event %d\n", event_type);
		goto out;
	}

	if (!try_module_get(owner)) {
		pr_warning("Can't get module reference, dropping event %d\n", event_type);
		kfree(event);
		event = NULL;
		goto out;
	}

	event->type = event_type;
	event->object = object;
	event->bus = bus;
	event->owner = owner;

	list_add_tail(&event->node, &mcu_event_list);
	queue_work(system_long_wq, &mcu_event_work);

out:
	spin_unlock_irqrestore(&mcu_event_lock, flags);
	return event;
}

void mcu_remove_pending_events(void *object)
{
	struct mcu_event *event, *next;
	unsigned long flags;

	spin_lock_irqsave(&mcu_event_lock, flags);

	list_for_each_entry_safe(event, next, &mcu_event_list, node) {
		if (event->object == object) {
			list_del_init(&event->node);
			mcu_free_event(event);
		}
	}

	spin_unlock_irqrestore(&mcu_event_lock, flags);
}

