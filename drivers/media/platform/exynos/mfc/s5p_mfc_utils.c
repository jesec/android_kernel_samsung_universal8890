/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_utils.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "s5p_mfc_utils.h"
#include "s5p_mfc_mem.h"

#define COL_FRAME_RATE		0
#define COL_FRAME_INTERVAL	1

/*
 * A framerate table determines framerate by the interval(us) of each frame.
 * Framerate is not accurate, just rough value to seperate overload section.
 * Base line of each section are selected from 25fps(40000us), 45fps(22222us)
 * and 100fps(10000us).
 *
 * interval(us) | 0           10000         22222         40000           |
 * framerate    |     120fps    |    60fps    |    30fps    |    25fps    |
 */
static unsigned long framerate_table[][2] = {
	{ 25000, 40000 },
	{ 30000, 22222 },
	{ 60000, 10000 },
	{ 120000, 0 },
};

int get_framerate_by_interval(int interval)
{
	unsigned long i;

	/* if the interval is too big (2sec), framerate set to 0 */
	if (interval > MFC_MAX_INTERVAL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(framerate_table); i++) {
		if (interval > framerate_table[i][COL_FRAME_INTERVAL])
			return framerate_table[i][COL_FRAME_RATE];
	}

	return 0;
}

int get_framerate(struct timeval *to, struct timeval *from)
{
	unsigned long interval;

	if (timeval_compare(to, from) <= 0)
		return 0;

	interval = timeval_diff(to, from);

	return get_framerate_by_interval(interval);
}

void s5p_mfc_cleanup_queue(struct list_head *lh)
{
	struct s5p_mfc_buf *b;
	int i;

	while (!list_empty(lh)) {
		b = list_entry(lh->next, struct s5p_mfc_buf, list);
		for (i = 0; i < b->vb.num_planes; i++)
			vb2_set_plane_payload(&b->vb, i, 0);
		vb2_buffer_done(&b->vb, VB2_BUF_STATE_ERROR);
		list_del(&b->list);
	}
}

int check_vb_with_fmt(struct s5p_mfc_fmt *fmt, struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	int i;

	if (!fmt)
		return -EINVAL;

	if (fmt->mem_planes != vb->num_planes) {
		mfc_err("plane number is different (%d != %d)\n",
				fmt->mem_planes, vb->num_planes);
		return -EINVAL;
	}

	for (i = 0; i < vb->num_planes; i++) {
		if (!s5p_mfc_mem_plane_addr(ctx, vb, i)) {
			mfc_err("failed to get plane cookie\n");
			return -ENOMEM;
		}

		mfc_debug(2, "index: %d, plane[%d] cookie: 0x%08llx",
				vb->v4l2_buf.index, i,
				s5p_mfc_mem_plane_addr(ctx, vb, i));
	}

	return 0;
}
