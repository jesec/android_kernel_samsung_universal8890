/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_clk.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/of.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"

#include <../drivers/clk/samsung/clk.h>

void fimg2d_clk_on(struct fimg2d_control *ctrl)
{
	clk_enable(ctrl->clock);

	fimg2d_debug("%s : clock enable\n", __func__);
}

void fimg2d_clk_off(struct fimg2d_control *ctrl)
{
	clk_disable(ctrl->clock);

	fimg2d_debug("%s : clock disable\n", __func__);
}

int fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	int ret = 0;

	/* TODO */

	return ret;
}

void fimg2d_clk_release(struct fimg2d_control *ctrl)
{
	/* TODO */
}
