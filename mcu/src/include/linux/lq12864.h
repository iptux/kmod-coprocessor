/*
 * lq12864.h
 * driver for longqiu 128x64 OLED driver(i2c interface)
 *
 * Author: Alex.wang
 * Create: 2013-08-04 17:03
 */


#ifndef _LINUX_LQ12864_H
#define _LINUX_LQ12864_H

#ifdef __KERNEL__
# include <linux/types.h>
# include <linux/ioctl.h>
#else
# include <linux/types.h>
# include <sys/types.h>

#ifndef u8
typedef __u8 u8;
#endif
#endif /* __KERNEL__ */

#define LQ12864_WIDTH  128
#define LQ12864_HEIGHT 8

struct lq12864_ioctl_data {
	size_t size;
	u8 *data;
	u8 x;
	u8 width;
	u8 width2;
	u8 y:3;
	u8 inverse:1;
	u8 height:4;
} __attribute__((packed));

/* ioctl code */
#define LQ12864_MAGIC 0xa8
#define LQ12864_IOCTL_INIT  _IOW(LQ12864_MAGIC, 1, struct lq12864_ioctl_data *)
#define LQ12864_IOCTL_FILL  _IOW(LQ12864_MAGIC, 2, struct lq12864_ioctl_data *)
#define LQ12864_IOCTL_CLEAR _IOW(LQ12864_MAGIC, 3, struct lq12864_ioctl_data *)
#define LQ12864_IOCTL_DRAW  _IOW(LQ12864_MAGIC, 4, struct lq12864_ioctl_data *)
#define LQ12864_IOCTL_EN    _IOW(LQ12864_MAGIC, 5, struct lq12864_ioctl_data *)
#define LQ12864_IOCTL_EN2   _IOW(LQ12864_MAGIC, 6, struct lq12864_ioctl_data *)


#endif /* _LINUX_LQ12864_H */

