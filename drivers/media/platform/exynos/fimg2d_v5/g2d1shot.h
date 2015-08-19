/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot.h
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

#ifndef __EXYNOS_G2D1SHOT_H_
#define __EXYNOS_G2D1SHOT_H_

#include <linux/videodev2.h>
#include <media/m2m1shot2.h>

#include "g2d1shot_regs.h"

#define MODULE_NAME	"exynos-g2d"
#define NODE_NAME	"fimg2d"

#define	G2D_MAX_PLANES	1
#define	G2D_MAX_SOURCES	3

#define G2D_MAX_NORMAL_SIZE	16383
#define G2D_MAX_COMP_SIZE	8192

/* flags for G2D supported format */
#define G2D_FMT_FLAG_SUPPORT_COMP	(1 << 0)
#define G2D_DATAFORMAT_RGB8888		(0 << 16)
#define G2D_DATAFORMAT_RGB565		(1 << 16)

struct g2d1shot_fmt {
	char	*name;
	u32	pixelformat;
	u8	bpp[G2D_MAX_PLANES];
	u8	num_planes;
	u8	flag;
	u32	src_value;
	u32	dst_value;
};

struct g2d1shot_dev {
	struct m2m1shot2_device *oneshot2_dev;
	struct device *dev;
	struct clk *clock;
	void __iomem *reg;

	u32 version;
	unsigned long state;
};

struct g2d1shot_ctx {
	struct g2d1shot_dev *g2d_dev;
	u32 src_fmt_value[G2D_MAX_SOURCES];
	u32 dst_fmt_value;
};

void g2d_hw_init(struct g2d1shot_dev *g2d_dev);
void g2d_hw_start(struct g2d1shot_dev *g2d_dev);
u32 g2d_hw_read_version(struct g2d1shot_dev *g2d_dev);
void g2d_hw_set_source_color(struct g2d1shot_dev *g2d_dev, int n, u32 color);
void g2d_hw_set_source_blending(struct g2d1shot_dev *g2d_dev, int n,
						struct m2m1shot2_extra *ext);
void g2d_hw_set_source_premult(struct g2d1shot_dev *g2d_dev, int n, u32 flags);
void g2d_hw_set_source_type(struct g2d1shot_dev *g2d_dev, int n, bool is_constant);
void g2d_hw_set_source_format(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_context_format *ctx_fmt);
void g2d_hw_set_source_address(struct g2d1shot_dev *g2d_dev, int n,
		int plane, dma_addr_t addr);
void g2d_hw_set_source_repeat(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext);
void g2d_hw_set_source_scale(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext);
void g2d_hw_set_source_rotate(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext);
void g2d_hw_set_source_valid(struct g2d1shot_dev *g2d_dev, int n);
void g2d_hw_set_dest_addr(struct g2d1shot_dev *g2d_dev, int plane, dma_addr_t addr);
void g2d_hw_set_dest_format(struct g2d1shot_dev *g2d_dev,
		struct m2m1shot2_context_format *ctx_fmt);
void g2d_hw_set_dest_premult(struct g2d1shot_dev *g2d_dev, u32 flags);
void g2d_hw_set_dither(struct g2d1shot_dev *g2d_dev);
#endif /* __EXYNOS_G2D1SHOT_H_ */
