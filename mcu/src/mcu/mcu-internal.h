/*
 * mcu-internal.h
 * mcu coprocessor bus protocol, internal driver collection
 *
 * Author: Alex.wang
 * Create: 2015-07-08 13:13
 */


#ifndef __MCU_INTERNAL_H_
#define __MCU_INTERNAL_H_

#include <linux/mcu.h>

// line disciplines number
#define N_MCU 28

/*
 * as all driver embedded in one module,
 * drivers have to be registered in one module_init()
 */

#ifdef CONFIG_MCU_GPIO
extern struct mcu_driver __mcu_gpio;
#endif

#ifdef CONFIG_MCU_OLED
extern struct mcu_driver __mcu_oled;
#endif

#ifdef CONFIG_MCU_BATTERY
extern struct mcu_driver __mcu_battery;
#endif

#ifdef CONFIG_MCU_LDISC
extern int sermcu_init(void) __init;
extern void sermcu_exit(void) __exit;
#endif

#endif	// __MCU_INTERNAL_H_

