/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot_hw5x.c
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

#include <linux/io.h>
#include <linux/sched.h>

#include "g2d1shot.h"
#include "g2d1shot_regs.h"

void g2d_hw_init(struct g2d1shot_dev *g2d_dev)
{
	/* sfr clear */
}

void g2d_hw_start(struct g2d1shot_dev *g2d_dev)
{
	/* start h/w */
}

u32 g2d_hw_read_version(struct g2d1shot_dev *g2d_dev)
{
	/* read version */

	return 0;
}

void g2d_hw_set_source_color(struct g2d1shot_dev *g2d_dev, int n, u32 color)
{
	/* set constant color */
}

void g2d_hw_set_source_blending(struct g2d1shot_dev *g2d_dev, int n, struct m2m1shot2_extra *ext)
{
	/* enable alpha blending, but skip if layer is 0 */
}

void g2d_hw_set_source_premult(struct g2d1shot_dev *g2d_dev, int n, u32 flags)
{
	/* set source premult */
}

void g2d_hw_set_source_type(struct g2d1shot_dev *g2d_dev, int n, bool is_constant)
{
	/* set source type */
}

void g2d_hw_set_source_format(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_context_format *ctx_fmt)
{

	/* set source rect */

	/* set dest clip */

	/* set pixel format */
}

void g2d_hw_set_source_address(struct g2d1shot_dev *g2d_dev, int n,
		int plane, dma_addr_t addr)
{

	/* set source address */
	/* if COMPRESSED, SFR should be changed */

}

void g2d_hw_set_source_repeat(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext)
{

	/* set repeat, or default NONE */
}


void g2d_hw_set_source_scale(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext)
{

	/* set scaling ratio, default NONE */
}

void g2d_hw_set_source_rotate(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext)
{

	/* set rotate mode */
}

void g2d_hw_set_source_valid(struct g2d1shot_dev *g2d_dev, int n)
{

	/* set valid flag */
	/* set update layer flag */
}

void g2d_hw_set_dest_addr(struct g2d1shot_dev *g2d_dev, int plane, dma_addr_t addr)
{

	/* set dest address */
}

void g2d_hw_set_dest_format(struct g2d1shot_dev *g2d_dev, struct m2m1shot2_context_format *ctx_fmt)
{

	/* set dest rect */

	/* set dest pixelformat */
}

void g2d_hw_set_dest_premult(struct g2d1shot_dev *g2d_dev, u32 flags)
{

	/* set dest premult if flag is set */
}

void g2d_hw_set_dither(struct g2d1shot_dev *g2d_dev)
{

	/* set dithering */
}
