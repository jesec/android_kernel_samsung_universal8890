/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/clk-provider.h>
#include <linux/pm_runtime.h>
#include <linux/exynos_iovmm.h>
#include <linux/of_address.h>
#include <linux/clk-private.h>

#include <media/v4l2-subdev.h>

#include <mach/regs-clock.h>
#include <plat/cpu.h>

//#include <../pwrcal/pwrcal.h>
#include "decon.h"
#include "dsim.h"
#include "decon_helper.h"
int my_cnt;

static void decon_oneshot_underrun_log(struct decon_device *decon)
{
	DISP_SS_EVENT_LOG(DISP_EVT_UNDERRUN, &decon->sd, ktime_set(0, 0));

	decon->underrun_stat.underrun_cnt++;
	if (decon->fifo_irq_status++ > UNDERRUN_FILTER_IDLE)
		return;

	if (decon->underrun_stat.underrun_cnt > DECON_UNDERRUN_THRESHOLD) {
		decon_warn("underrun (cnt %d), tot_bw %d, int_bw %d, disp_bw %d, mif(%ld), chmap(0x%x), win(0x%lx), aclk(%ld), lh_disp(%ld)\n",
				decon->underrun_stat.underrun_cnt,
				decon->underrun_stat.prev_bw,
				decon->underrun_stat.prev_int_bw,
				decon->underrun_stat.prev_disp_bw,
				(decon->underrun_stat.mif_pll >> 1) / MHZ,
				decon->underrun_stat.chmap,
				decon->underrun_stat.used_windows,
				decon->underrun_stat.aclk / MHZ,
				decon->underrun_stat.lh_disp0 / MHZ);
	}
	decon->underrun_stat.underrun_cnt = 0;

	queue_work(decon->fifo_irq_wq, &decon->fifo_irq_work);
}

static void decon_f_get_enabled_win(struct decon_device *decon)
{
	int i;

	decon->underrun_stat.used_windows = 0;

	for (i = 0; i < MAX_DECON_WIN; ++i)
		if (decon_read(decon->id, (WIN_CONTROL(i)) + SHADOW_OFFSET) & WIN_EN_F)
			set_bit(i * 4, &decon->underrun_stat.used_windows);
}

irqreturn_t decon_f_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	u32 irq_sts_reg;
	ktime_t timestamp;
	u32 fifo_level = 0;

	timestamp = ktime_get();

	spin_lock(&decon->slock);
	if ((decon->state == DECON_STATE_OFF) ||
		(decon->state == DECON_STATE_LPD)) {
		goto irq_end;
	}

	irq_sts_reg = decon_reg_get_interrupt_and_clear(decon->id);
	if (irq_sts_reg & INT_DISPIF_VSTATUS_INT_PEND) {
		/* VSYNC interrupt, accept it */
		if (decon->pdata->psr_mode == DECON_VIDEO_MODE) {
			decon->vsync_info.timestamp = timestamp;
			wake_up_interruptible_all(&decon->vsync_info.wait);
			if(my_cnt++ % 10 == 0)
				pr_err("[> VSYNC <] %s:%d\n", __func__, __LINE__);
		}
	}

	if (irq_sts_reg & INT_FIFO_LEVEL_INT_PEND) {
		decon->underrun_stat.fifo_level = fifo_level;
		decon->underrun_stat.prev_bw = decon->prev_bw;
		decon->underrun_stat.prev_int_bw = decon->prev_int_bw;
		decon->underrun_stat.prev_disp_bw = decon->prev_disp_bw;
		/*decon->underrun_stat.chmap = decon_read(0, WINCHMAP0 + * SHADOW_OFFSET);*/
		decon_f_get_enabled_win(decon);
		decon_oneshot_underrun_log(decon);
	}

	if (irq_sts_reg & INT_FRAME_DONE_INT_PEND) {
		DISP_SS_EVENT_LOG(DISP_EVT_DECON_FRAMEDONE, &decon->sd, ktime_set(0, 0));
		decon->frame_done_cnt_cur++;
		wake_up_interruptible_all(&decon->wait_frmdone);
	}

irq_end:
	spin_unlock(&decon->slock);

	return IRQ_HANDLED;
}

int decon_f_get_clocks(struct decon_device *decon)
{
	decon->res.pclk = clk_get(decon->dev, "pclk_decon0");
	if (IS_ERR_OR_NULL(decon->res.pclk)) {
		decon_err("failed to get pclk_decon0\n");
		return -ENODEV;
	}

	decon->res.aclk = clk_get(decon->dev, "aclk_decon0");
	if (IS_ERR_OR_NULL(decon->res.aclk)) {
		decon_err("failed to get aclk_decon0\n");
		return -ENODEV;
	}

	decon->res.eclk = clk_get(decon->dev, "decon0_eclk");
	if (IS_ERR_OR_NULL(decon->res.eclk)) {
		decon_err("failed to get decon0_eclk\n");
		return -ENODEV;
	}

	decon->res.vclk = clk_get(decon->dev, "decon0_vclk");
	if (IS_ERR_OR_NULL(decon->res.vclk)) {
		decon_err("failed to get decon0_vclk\n");
		return -ENODEV;
	}

	return 0;
}

void decon_f_set_clocks(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct decon_clocks clks;
	struct decon_param p;

	decon_to_init_param(decon, &p);
	decon_reg_get_clock_ratio(&clks, &p);

	/* VCLK */
	decon_clk_set_rate(dev, NULL, "disp_pll", clks.decon[CLK_ID_DPLL] * MHZ);
	decon_clk_set_rate(dev, decon->res.vclk, NULL, clks.decon[CLK_ID_VCLK] * MHZ);

	/* ECLK */
	decon_clk_set_rate(dev, decon->res.eclk, NULL, clks.decon[CLK_ID_ECLK] * MHZ);

	/* TODO: ACLK */
	//cal_dfs_set_rate(cal_dvfs_disp, clks.decon[CLK_ID_ACLK] * 100);
	/* TODO: PCLK */

	decon_dbg("%s:pclk %ld aclk %ld vclk %ld eclk %ld\n",
		__func__,
		clk_get_rate(decon->res.pclk),
		clk_get_rate(decon->res.aclk),
		clk_get_rate(decon->res.vclk),
		clk_get_rate(decon->res.eclk));

	return;
}

static void underrun_filter_handler(struct work_struct *work)
{
	struct decon_device *decon =
			container_of(work, struct decon_device, fifo_irq_work);
	msleep(UNDERRUN_FILTER_INTERVAL_MS);
	decon->fifo_irq_status = UNDERRUN_FILTER_IDLE;
}

int decon_f_register_irq(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct resource *res;
	int ret = 0;
	my_cnt = 0;

	/* Get IRQ resource and register IRQ handler. */
	/* 0: FIFO irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, decon_f_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install FIFO irq\n");
		return ret;
	}

	/* 1: VSTATUS INFO irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	ret = devm_request_irq(dev, res->start, decon_f_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err("failed to install VSTATUS irq\n");
		return ret;
	}

	if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) {
		/* 2: I80 FrameDone irq */
		res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
		ret = devm_request_irq(dev, res->start, decon_f_irq_handler,
				0, pdev->name, decon);
		if (ret) {
			decon_err("failed to install FrameDone irq\n");
			return ret;
		}
	}

	/* 3: Extra Interrupts: Resource Conflict irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	ret = devm_request_irq(dev, res->start, decon_f_irq_handler,
			0, pdev->name, decon);
	if (ret) {
		decon_err("failed to install Extra irq\n");
		return ret;
	}

	if (decon->fifo_irq_status++ == UNDERRUN_FILTER_INIT) {
		decon->fifo_irq_wq = create_singlethread_workqueue("decon_fifo_irq_wq");
		if (decon->fifo_irq_wq == NULL) {
			decon_err("%s:failed to create workqueue for fifo_irq_wq\n", __func__);
			return -ENOMEM;
		}

		INIT_WORK(&decon->fifo_irq_work, underrun_filter_handler);
	}

	return ret;
}
