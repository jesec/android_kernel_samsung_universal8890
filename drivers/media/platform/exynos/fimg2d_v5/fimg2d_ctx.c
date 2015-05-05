/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_ctx.c
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

#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/exynos_ion.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

int fimg2d_check_image(struct fimg2d_image *img)
{
	struct fimg2d_rect *r;
	int w, h;

	if (!img->addr.type)
		return 0;

	w = img->width;
	h = img->height;
	r = &img->rect;

	/* 16383: max width & height */
	/* 8192: COMP max width & height */
	switch (img->fmt) {
	case CF_COMP_RGB8888:
		if (w > 8192 || h > 8192)
			return -1;
		break;
	default:
		if (w > 16383 || h > 16383)
			return -1;
		break;
	}

	/* Is it correct to compare (x1 >= w) and (y1 >= h) ? */
	if (r->x1 < 0 || r->y1 < 0 ||
			r->x1 >= w || r->y1 >= h ||
			r->x1 >= r->x2 || r->y1 >= r->y2) {
		fimg2d_err("r(%d, %d, %d, %d) w,h(%d, %d)\n",
				r->x1, r->y1, r->x2, r->y2, w, h);
		return -2;
	}

	/* DO support UVA & DVA(fd) */
	if (img->addr.type != ADDR_USER &&
			img->addr.type != ADDR_DEVICE)
		return -3;

	return 0;
}

static int fimg2d_check_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_scale *scl;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	enum addr_space addr_type;
	int w, h, i;
	int ret;

	/* dst is mandatory */
	if (WARN_ON(!blt->dst || !blt->dst->addr.type))
		return -1;

	addr_type = blt->dst->addr.type;

	for (i = 0; i < MAX_SRC; i++) {
		img = blt->src[i];
		if (img && (cmd->src_flag & (1 << i)) &&
				(img->addr.type != addr_type)) {
			if (img->op != BLIT_OP_SOLID_FILL)
				return -2;
		}
	}

	/* Check for destination */
	img = &cmd->image_dst;
	ret = fimg2d_check_image(img);
	if (ret) {
		fimg2d_err("check image failed, ret = %d\n", ret);
		return -3;
	}

	/* Check for source */
	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (fimg2d_check_image(img))
			return -4;

		clp = &img->param.clipping;
		if (clp->enable) {
			w = img->width;
			h = img->height;
			r = &img->rect;

			if (clp->x1 < 0 || clp->y1 < 0 ||
				clp->x1 >= w || clp->y1 >= h ||
				clp->x1 >= clp->x2 || clp->y1 >= clp->y2) {
				fimg2d_err("clp(%d, %d, %d, %d) w,h(%d, %d), r(%d, %d, %d, %d)\n",
					clp->x1, clp->y1, clp->x2, clp->y2,
					w, h, r->x1, r->y1, r->x2, r->y2);
				return -5;
			}
			fimg2d_info("Layer:%d, Clipping(%d, %d, %d, %d)\n",
					i, clp->x1, clp->y1, clp->x2, clp->y2);
		}

		scl = &img->param.scaling;
		if (scl->mode) {
			if (!scl->src_w || !scl->src_h ||
					!scl->dst_w || !scl->dst_h)
				return -6;
		}

	}

	fimg2d_register_memops(cmd, addr_type);

	return 0;
}

static void fimg2d_fixup_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_scale *scl;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		switch (img->fmt) {
		case CF_COMP_RGB8888:
			if (!IS_ALIGNED(img->width, FIMG2D_COMP_ALIGN_WIDTH)) {
				fimg2d_info("SRC[%d], COMP format width(%d)",
								i, img->width);
				img->width = ALIGN(img->width,
						FIMG2D_COMP_ALIGN_WIDTH);
				fimg2d_info("becomes %d, (aligned by %d)\n",
					img->width, FIMG2D_COMP_ALIGN_WIDTH);
			}
			if (!IS_ALIGNED(img->height,
						FIMG2D_COMP_ALIGN_HEIGHT)) {
				fimg2d_info("SRC[%d], COMP format height(%d)",
								i, img->height);
				img->height = ALIGN(img->height,
						FIMG2D_COMP_ALIGN_HEIGHT);
				fimg2d_info("becomes %d, (aligned by %d)\n",
					img->height, FIMG2D_COMP_ALIGN_HEIGHT);
			}
			break;
		default:
			/* NOP */
			break;
		}

		scl = &img->param.scaling;

		/* avoid divided-by-zero */
		if (scl->mode &&
			(scl->src_w == scl->dst_w && scl->src_h == scl->dst_h))
			scl->mode = NO_SCALING;
	}
}

static int fimg2d_check_dma_sync(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_memops *memops = cmd->memops;
	enum addr_space addr_type;
	int ret = 0;

	addr_type = blt->dst->addr.type;

	if (!memops) {
		fimg2d_err("No memops registered, type = %d\n", addr_type);
		return -EFAULT;
	}

	ret = memops->prepare(ctrl, cmd);
	if (ret) {
		fimg2d_err("Failed to memory prepare, %d\n", ret);
		return ret;
	}

	return 0;
}

struct fimg2d_bltcmd *fimg2d_add_command(struct fimg2d_control *ctrl,
	struct fimg2d_context *ctx, struct fimg2d_blit __user *buf, int *info)
{
	unsigned long flags;
	struct fimg2d_blit *blt;
	struct fimg2d_bltcmd *cmd;
	struct fimg2d_image *img;
	int len = sizeof(struct fimg2d_image);
	int i, ret = 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		*info = -ENOMEM;
		return NULL;
	}

	if (copy_from_user(&cmd->blt, buf, sizeof(cmd->blt))) {
		ret = -EFAULT;
		goto err;
	}

	INIT_LIST_HEAD(&cmd->job);
	cmd->ctx = ctx;

	blt = &cmd->blt;

	for (i = 0; i < MAX_SRC; i++) {
		if (blt->src[i]) {
			img = &cmd->image_src[i];
			if (copy_from_user(img, blt->src[i], len)) {
				ret = -EFAULT;
				goto err;
			}
			blt->src[i] = img;
			cmd->src_flag |= (1 << i);
		}
	}

	if (blt->dst) {
		if (copy_from_user(&cmd->image_dst, blt->dst, len)) {
			ret = -EFAULT;
			goto err;
		}
		blt->dst = &cmd->image_dst;
	}

	fimg2d_dump_command(cmd);

	perf_start(cmd, PERF_TOTAL);

	ret = fimg2d_check_params(cmd);
	if (ret) {
		fimg2d_err("check param fails, ret = %d\n", ret);
		ret = -EINVAL;
		goto err;
	}

	fimg2d_fixup_params(cmd);

	if (fimg2d_check_dma_sync(ctrl, cmd)) {
		ret = -EFAULT;
		goto err;
	}

	/* TODO: PM QoS */

	/* add command node and increase ncmd */
	g2d_spin_lock(&ctrl->bltlock, flags);
	if (atomic_read(&ctrl->suspended)) {
		fimg2d_debug("driver is unavailable, do sw fallback\n");
		g2d_spin_unlock(&ctrl->bltlock, flags);
		ret = -EPERM;
		goto err;
	}
	atomic_inc(&ctx->ncmd);
	fimg2d_enqueue(cmd, ctrl);
	fimg2d_debug("ctx %p pgd %p ncmd(%d) seq_no(%u)\n",
			cmd->ctx, (unsigned long *)cmd->ctx->mm->pgd,
			atomic_read(&ctx->ncmd), cmd->blt.seq_no);
	g2d_spin_unlock(&ctrl->bltlock, flags);

	return cmd;

err:
	kfree(cmd);
	*info = ret;
	return NULL;
}

void fimg2d_del_command(struct fimg2d_control *ctrl, struct fimg2d_bltcmd *cmd)
{
	unsigned long flags;
	struct fimg2d_context *ctx = cmd->ctx;

	perf_end(cmd, PERF_TOTAL);
	perf_print(cmd);
	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_dequeue(cmd, ctrl);
	kfree(cmd);
	atomic_dec(&ctx->ncmd);

	/* wake up context */
	if (!atomic_read(&ctx->ncmd))
		wake_up(&ctx->wait_q);

	g2d_spin_unlock(&ctrl->bltlock, flags);
}

struct fimg2d_bltcmd *fimg2d_get_command(struct fimg2d_control *ctrl,
						int is_wait_q)
{
	unsigned long flags;
	struct fimg2d_bltcmd *cmd;

	g2d_spin_lock(&ctrl->bltlock, flags);
	cmd = fimg2d_get_first_command(ctrl, is_wait_q);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	return cmd;
}

void fimg2d_add_context(struct fimg2d_control *ctrl, struct fimg2d_context *ctx)
{
	atomic_set(&ctx->ncmd, 0);
	init_waitqueue_head(&ctx->wait_q);

	atomic_inc(&ctrl->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&ctrl->nctx));
}

void fimg2d_del_context(struct fimg2d_control *ctrl, struct fimg2d_context *ctx)
{
	atomic_dec(&ctrl->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&ctrl->nctx));
}

int fimg2d_register_memops(struct fimg2d_bltcmd *cmd, enum addr_space addr_type)
{
	/* TODO */

	return 0;
}
