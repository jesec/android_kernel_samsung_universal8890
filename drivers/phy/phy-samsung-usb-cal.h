/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Author: Anton Tikhomirov <av.tikhomirov@samsung.com>
 *
 * Linux kernel specific definitions for Samsung USB PHY CAL
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PHY_SAMSUNG_USB_FW_CAL_H__
#define __PHY_SAMSUNG_USB_FW_CAL_H__

#include <linux/delay.h>
#include <linux/io.h>

/**
 * struct exynos_usbphy_info : USBPHY information to share USBPHY CAL code
 * @version: PHY controller version
 *	     0x0100 - for EXYNOS_USB3 : EXYNOS7420, EXYNOS7890
 *	     0x0101 -			EXYNOS8890
 *	     0x0200 - for EXYNOS_USB2 : EXYNOS7580, EXYNOS3475
 *	     0x0210 -			EXYNOS8890_EVT1
 *	     0xF200 - for EXT	      : EXYNOS7420_HSIC
 * @refclk: reference clock frequency for USBPHY
 * @regs_base: base address of PHY control register
 */
struct exynos_usbphy_info {
	u32	version;
	u32	refclk;
	void __iomem *regs_base;
};

#endif	/* __PHY_SAMSUNG_USB_FW_CAL_H__ */
