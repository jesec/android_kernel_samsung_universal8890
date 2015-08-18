/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot_drv.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/exynos_iovmm.h>

#include "g2d1shot.h"

static int m2m1shot2_g2d_init_context(struct m2m1shot2_context *ctx)
{
	struct g2d1shot_dev *dev = dev_get_drvdata(ctx->m21dev->dev);
	struct g2d1shot_ctx *g2d_ctx;

	g2d_ctx = kzalloc(sizeof(*g2d_ctx), GFP_KERNEL);
	if (!g2d_ctx)
		return -ENOMEM;

	ctx->priv = g2d_ctx;

	g2d_ctx->g2d_dev = dev;

	return 0;
}

static int m2m1shot2_g2d_free_context(struct m2m1shot2_context *ctx)
{
	struct g2d1shot_ctx *g2d_ctx = ctx->priv;

	kfree(g2d_ctx);

	return 0;
}

static const struct g2d1shot_fmt g2d_formats[] = {
	{
		.name		= "ABGR8888",
		.pixelformat	= V4L2_PIX_FMT_ABGR32,	/* [31:0] ABGR */
		.bpp		= { 32 },
		.num_planes	= 1,
	}, {
		.name		= "XBGR8888",
		.pixelformat	= V4L2_PIX_FMT_XBGR32,	/* [31:0] XBGR */
		.bpp		= { 32 },
		.num_planes	= 1,
	}, {
		.name		= "ARGB8888",
		.pixelformat	= V4L2_PIX_FMT_ARGB32,	/* [31:0] ARGB */
		.bpp		= { 32 },
		.num_planes	= 1,
	}, {
		.name		= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,	/* [15:0] RGB */
		.bpp		= { 16 },
		.num_planes	= 1,
	},
};

static const struct g2d1shot_fmt *find_format(bool is_source, u32 fmt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g2d_formats); i++) {
		if (fmt == g2d_formats[i].pixelformat) {
			/* TODO: check more.. H/W version, source/dest.. */
			return &g2d_formats[i];
		}
	}

	return NULL;
}

static int m2m1shot2_g2d_prepare_format(
			struct m2m1shot2_context_format *ctx_fmt,
			unsigned int index, enum dma_data_direction dir,
			size_t payload[], unsigned int *num_planes)
{
	const struct g2d1shot_fmt *g2d_fmt;
	struct m2m1shot2_format *fmt = &ctx_fmt->fmt;
	int i;

	g2d_fmt = find_format((dir == DMA_TO_DEVICE), fmt->pixelformat);
	if (!g2d_fmt) {
		pr_err("Not supported format (%u)\n", fmt->pixelformat);
		return -EINVAL;
	}

	if (fmt->width == 0 || fmt->height == 0 ||
			fmt->width > G2D_MAX_NORMAL_SIZE ||
			fmt->height > G2D_MAX_NORMAL_SIZE) {
		pr_err("Invalid width or height (%u, %u)\n",
				fmt->width, fmt->height);
		return -EINVAL;
	}

	if (fmt->crop.left < 0 || fmt->crop.top < 0 ||
			(fmt->crop.left + fmt->crop.width > fmt->width) ||
			(fmt->crop.top + fmt->crop.height > fmt->height)) {
		pr_err("Invalid range of crop ltwh(%d, %d, %d, %d)\n",
				fmt->crop.left, fmt->crop.top,
				fmt->crop.width, fmt->crop.height);
		pr_err("width/height : %u/%u\n", fmt->width, fmt->height);
		return -EINVAL;
	}

	/* TODO: compare between dest rect and clipping rect of each source */

	*num_planes = g2d_fmt->num_planes;
	for (i = 0; i < g2d_fmt->num_planes; i++) {
		payload[i] = fmt->width * fmt->height * g2d_fmt->bpp[i];
		payload[i] /= 8;
	}
	ctx_fmt->priv = (void *)g2d_fmt;

	return 0;
}

static int m2m1shot2_g2d_prepare_source(struct m2m1shot2_context *ctx,
			unsigned int index, struct m2m1shot2_source_image *img)
{
	return 0;
}

static int m2m1shot2_g2d_device_run(struct m2m1shot2_context *ctx)
{
	struct g2d1shot_ctx *g2d_ctx = ctx->priv;
	struct g2d1shot_dev *g2d_dev = g2d_ctx->g2d_dev;

	/* DUMMY for compile error (unused variable) */
	if (!g2d_dev)
		return -EINVAL;

	/* Marking for H/W operation? */

	/* enable power, clock */

	/* H/W initialization */

	/* setting for source */

	/* setting for destination */

	/* setting for common */

	/* run H/W */

	return 0;
}

static const struct m2m1shot2_devops m2m1shot2_g2d_ops = {
	.init_context = m2m1shot2_g2d_init_context,
	.free_context = m2m1shot2_g2d_free_context,
	.prepare_format = m2m1shot2_g2d_prepare_format,
	.prepare_source = m2m1shot2_g2d_prepare_source,
	.device_run = m2m1shot2_g2d_device_run,
};

static irqreturn_t exynos_g2d_irq_handler(int irq, void *priv)
{
	struct g2d1shot_dev *g2d_dev = priv;
	struct m2m1shot2_context *m21ctx;

	m21ctx = m2m1shot2_current_context(g2d_dev->oneshot2_dev);
	if (!m21ctx) {
		dev_err(g2d_dev->dev, "received null in irq handler\n");
		return IRQ_HANDLED;
	}

	/* IRQ handling */

	/* Unmark for H/W operation */

	/* How to define success? */
	m2m1shot2_finish_context(m21ctx, true);

	return IRQ_HANDLED;
}

static int g2d_iommu_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flags, void *token)
{
	struct g2d1shot_dev *g2d_dev = token;

	if (!g2d_dev)
		pr_err("Failed!\n");

	return 0;
}

static int g2d_init_clock(struct device *dev, struct g2d1shot_dev *g2d_dev)
{
	g2d_dev->clock = devm_clk_get(dev, "gate");
	if (IS_ERR(g2d_dev->clock)) {
		dev_err(dev, "Failed to get clock (%ld)\n",
					PTR_ERR(g2d_dev->clock));
		return PTR_ERR(g2d_dev->clock);
	}

	return 0;
}

static int g2d_get_hw_version(struct device *dev, struct g2d1shot_dev *g2d_dev)
{
	int ret;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "Failed to enable power (%d)\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(g2d_dev->clock);
	if (!ret) {
		g2d_dev->version = __raw_readl(g2d_dev->reg + 0x14);
		dev_info(dev, "G2D version : %#010x\n", g2d_dev->version);

		clk_disable_unprepare(g2d_dev->clock);
	}

	pm_runtime_put(dev);

	return 0;
}

static int exynos_g2d_probe(struct platform_device *pdev)
{
	struct g2d1shot_dev *g2d_dev;
	struct resource *res;
	int ret;

	g2d_dev = devm_kzalloc(&pdev->dev, sizeof(*g2d_dev), GFP_KERNEL);
	if (!g2d_dev)
		return -ENOMEM;

	g2d_dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	g2d_dev->reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(g2d_dev->reg))
		return PTR_ERR(g2d_dev->reg);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get IRQ resource");
		return -ENOENT;
	}

	ret = devm_request_irq(&pdev->dev, res->start, exynos_g2d_irq_handler,
				0, pdev->name, g2d_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to install IRQ handler");
		return ret;
	}

	ret = g2d_init_clock(&pdev->dev, g2d_dev);
	if (ret)
		return ret;

	g2d_dev->oneshot2_dev = m2m1shot2_create_device(&pdev->dev,
		&m2m1shot2_g2d_ops, NODE_NAME, -1, M2M1SHOT2_DEVATTR_COHERENT);
	if (IS_ERR(g2d_dev->oneshot2_dev))
		return PTR_ERR(g2d_dev->oneshot2_dev);

	pm_runtime_enable(&pdev->dev);

	ret = g2d_get_hw_version(&pdev->dev, g2d_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get H/W version\n");
		goto err_hwver;
	}

	iovmm_set_fault_handler(&pdev->dev, g2d_iommu_fault_handler, g2d_dev);

	ret = iovmm_activate(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to activate iommu\n");
		goto err_hwver;
	}

	platform_set_drvdata(pdev, g2d_dev);

	dev_info(&pdev->dev, "G2D with m2m1shot2 is probed successfully.\n");

	return 0;
err_hwver:
	m2m1shot2_destroy_device(g2d_dev->oneshot2_dev);

	dev_err(&pdev->dev, "G2D m2m1shot2 probe is failed.\n");

	return ret;
}

static int exynos_g2d_remove(struct platform_device *pdev)
{
	struct g2d1shot_dev *g2d_dev = platform_get_drvdata(pdev);

	m2m1shot2_destroy_device(g2d_dev->oneshot2_dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int g2d_suspend(struct device *dev)
{
	return 0;
}

static int g2d_resume(struct device *dev)
{
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int g2d_runtime_suspend(struct device *dev)
{
	return 0;
}

static int g2d_runtime_resume(struct device *dev)
{
	return 0;
}
#endif

static const struct dev_pm_ops exynos_g2d_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(g2d_suspend, g2d_resume)
	SET_RUNTIME_PM_OPS(g2d_runtime_suspend, g2d_runtime_resume, NULL)
};

static const struct of_device_id exynos_g2d_match[] = {
	{
		.compatible = "samsung,s5p-fimg2d",
	},
	{},
};

static struct platform_driver exynos_g2d_driver = {
	.probe		= exynos_g2d_probe,
	.remove		= exynos_g2d_remove,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &exynos_g2d_pm_ops,
		.of_match_table = of_match_ptr(exynos_g2d_match),
	}
};

module_platform_driver(exynos_g2d_driver);

MODULE_AUTHOR("Janghyuck Kim <janghyuck.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos Graphics 2D driver");
MODULE_LICENSE("GPL");
