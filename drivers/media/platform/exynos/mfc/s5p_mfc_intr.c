/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_intr.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * C file for Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains functions used to wait for command completion.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/io.h>

#include "s5p_mfc_intr.h"

#define wait_condition(x, c) (x->int_cond &&		\
		(R2H_BIT(x->int_type) & r2h_bits(c)))
#define is_err_cond(x)	((x->int_cond) && (x->int_type == S5P_FIMV_R2H_CMD_ERR_RET))

int s5p_mfc_wait_for_done_dev(struct s5p_mfc_dev *dev, int command)
{
	int ret;

	ret = wait_event_timeout(dev->queue,
			wait_condition(dev, command),
			msecs_to_jiffies(MFC_INT_TIMEOUT));
	if (ret == 0) {
		mfc_err_dev("Interrupt (dev->int_type:%d, command:%d) timed out.\n",
							dev->int_type, command);
		return 1;
	}
	mfc_debug(1, "Finished waiting (dev->int_type:%d, command: %d).\n",
							dev->int_type, command);
	return 0;
}

int s5p_mfc_wait_for_done_ctx(struct s5p_mfc_ctx *ctx, int command)
{
	int ret;
	unsigned int timeout = MFC_INT_TIMEOUT;

	if (command == S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET)
		timeout = MFC_INT_SHORT_TIMEOUT;

	ret = wait_event_timeout(ctx->queue,
			wait_condition(ctx, command),
			msecs_to_jiffies(timeout));
	if (ret == 0) {
		mfc_err_ctx("Interrupt (ctx->int_type:%d, command:%d) timed out.\n",
							ctx->int_type, command);
		return 1;
	} else if (ret > 0) {
		if (is_err_cond(ctx)) {
			mfc_err_ctx("Finished (ctx->int_type:%d, command: %d).\n",
					ctx->int_type, command);
			mfc_err_ctx("But error (ctx->int_err:%d).\n", ctx->int_err);
			return -1;
		}
	}
	mfc_debug(1, "Finished waiting (ctx->int_type:%d, command: %d).\n",
							ctx->int_type, command);
	return 0;
}

void s5p_mfc_cleanup_timeout(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);
}


int s5p_mfc_get_new_ctx(struct s5p_mfc_dev *dev)
{
	int new_ctx;
	int cnt;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	mfc_debug(2, "Previous context: %d (bits %08lx)\n", dev->curr_ctx,
							dev->ctx_work_bits);

	if (dev->preempt_ctx > MFC_NO_INSTANCE_SET)
		return dev->preempt_ctx;
	else
		new_ctx = (dev->curr_ctx + 1) % MFC_NUM_CONTEXTS;

	cnt = 0;
	while (!test_bit(new_ctx, &dev->ctx_work_bits)) {
		new_ctx = (new_ctx + 1) % MFC_NUM_CONTEXTS;
		cnt++;
		if (cnt > MFC_NUM_CONTEXTS) {
			/* No contexts to run */
			return -EAGAIN;
		}
	}

	return new_ctx;
}
