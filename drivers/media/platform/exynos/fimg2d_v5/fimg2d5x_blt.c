/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d5x_blt.c
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
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/rmap.h>
#include <linux/fs.h>
#include <linux/clk-private.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d5x.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

#define BLIT_TIMEOUT	msecs_to_jiffies(8000)

static int fimg2d5x_blit_wait(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	int ret;

	ret = wait_event_timeout(ctrl->wait_q, !atomic_read(&ctrl->busy),
			BLIT_TIMEOUT);
	if (!ret) {
		fimg2d_err("blit wait timeout\n");

		fimg2d5x_disable_irq(ctrl);
		if (!fimg2d5x_blit_done_status(ctrl))
			fimg2d_err("blit not finished\n");

		fimg2d_dump_command(cmd);
		fimg2d5x_reset(ctrl);

		return -1;
	}
	return 0;
}

int fimg2d5x_bitblt(struct fimg2d_control *ctrl)
{
	int ret = 0;
	enum addr_space addr_type;
	struct fimg2d_context *ctx;
	struct fimg2d_bltcmd *cmd;
	struct fimg2d_memops *memops;
	unsigned long *pgd;

	fimg2d_debug("%s : enter blitter\n", __func__);

	cmd = fimg2d_get_command(ctrl, 0);
	if (!cmd)
		return 0;

	ctx = cmd->ctx;
	ctx->state = CTX_READY;

	list_del(&cmd->job);
	if (fimg2d5x_get_clk_cnt(ctrl->clock) == false)
		fimg2d_err("2D clock is not set\n");

	addr_type = cmd->image_dst.addr.type;

	atomic_set(&ctrl->busy, 1);

	perf_start(cmd, PERF_SFR);
	ret = ctrl->configure(ctrl, cmd);
	perf_end(cmd, PERF_SFR);
	if (IS_ERR_VALUE(ret)) {
		fimg2d_err("failed to configure\n");
		ctx->state = CTX_ERROR;
		goto fail_n_del;
	}

	/* memops is not NULL because configure is successed. */
	memops = cmd->memops;

	if (addr_type == ADDR_USER) {
		if (!ctx->mm || !ctx->mm->pgd) {
			atomic_set(&ctrl->busy, 0);
			fimg2d_err("ctx->mm:0x%p or ctx->mm->pgd:0x%p\n",
					ctx->mm,
					(ctx->mm) ? ctx->mm->pgd : NULL);
			ret = -EPERM;
			goto fail_n_unmap;
		}
		pgd = (unsigned long *)ctx->mm->pgd;
		fimg2d_debug("%s : sysmmu enable: pgd %p ctx %p seq_no(%u)\n",
				__func__, pgd, ctx, cmd->blt.seq_no);
	}

	if (iovmm_activate(ctrl->dev)) {
		fimg2d_err("failed to iovmm activate\n");
		ret = -EPERM;
		goto fail_n_unmap;
	}

	perf_start(cmd, PERF_BLIT);
	/* start blit */
	fimg2d_debug("%s : start blit\n", __func__);
	ctrl->run(ctrl);
	ret = fimg2d5x_blit_wait(ctrl, cmd);
	perf_end(cmd, PERF_BLIT);

	iovmm_deactivate(ctrl->dev);

fail_n_unmap:
	perf_start(cmd, PERF_UNMAP);
	memops->unmap(ctrl, cmd);
	memops->finish(ctrl, cmd);
	perf_end(cmd, PERF_UNMAP);
fail_n_del:
	fimg2d_del_command(ctrl, cmd);

	fimg2d_debug("%s : exit blitter\n", __func__);

	return ret;
}

int fimg2d_register_ops(struct fimg2d_control *ctrl)
{
	/* TODO */
	ctrl->blit = fimg2d5x_bitblt;

	return 0;
};
