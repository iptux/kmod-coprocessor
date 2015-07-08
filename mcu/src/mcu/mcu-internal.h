/*
 * mcu-internal.h
 * mcu coprocessor bus protocol, internal driver collection
 *
 * Author: Alex.wang
 * Create: 2015-07-08 13:13
 */


#include <linux/mcu.h>

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

