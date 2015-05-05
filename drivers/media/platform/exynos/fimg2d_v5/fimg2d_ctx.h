/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_ctx.h
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

static inline int fimg2d_queue_is_empty(struct list_head *q)
{
	return list_empty(q);
}

void fimg2d_add_context(struct fimg2d_control *ctrl,
		struct fimg2d_context *ctx);
void fimg2d_del_context(struct fimg2d_control *ctrl,
		struct fimg2d_context *ctx);
