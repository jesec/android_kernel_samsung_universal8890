/*
 * drivers/media/m2m1shot2.c
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Cho KyongHo <pullip.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>

#include <media/m2m1shot2.h>

static int m2m1shot2_open(struct inode *inode, struct file *filp)
{
	struct m2m1shot2_device *m21dev = container_of(filp->private_data,
						struct m2m1shot2_device, misc);
	struct m2m1shot2_context *ctx;
	unsigned long flags;
	int ret;

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;

	ctx->timeline = sw_sync_timeline_create(dev_name(m21dev->dev));
	if (!ctx->timeline) {
		dev_err(m21dev->dev, "Failed to create timeline\n");
		ret = -ENOMEM;
		goto err_timeline;
	}

	INIT_LIST_HEAD(&ctx->node);
	kref_init(&ctx->starter);

	spin_lock_irqsave(&m21dev->lock_ctx, flags);
	list_add_tail(&ctx->node, &m21dev->contexts);
	spin_unlock_irqrestore(&m21dev->lock_ctx, flags);

	ctx->m21dev = m21dev;
	mutex_init(&ctx->mutex);
	init_completion(&ctx->complete);
	complete_all(&ctx->complete); /* prevent to wait for completion */

	filp->private_data = ctx;

	for (ret = 0; ret < M2M1SHOT2_MAX_IMAGES; ret++)
		ctx->source[ret].img.index = ret;

	ctx->target.index = M2M1SHOT2_MAX_IMAGES;

	ret = m21dev->ops->init_context(ctx);
	if (ret) /* kref_put() is not called not to call .free_context() */
		goto err_init;

	return 0;
err_init:
	sync_timeline_destroy(&ctx->timeline->obj);
err_timeline:
	kfree(ctx);
	return ret;

}

/*
 * m2m1shot2_current_context - get the current m2m1shot2_context
 *
 * return the current context pointer.
 * This function should not be called other places than the function that
 * finishes the current context.
 */
struct m2m1shot2_context *m2m1shot2_current_context(
				const struct m2m1shot2_device *m21dev)
{
	return m21dev->current_ctx;
}


void m2m1shot2_finish_context(struct m2m1shot2_context *ctx, bool success)
{
}

static void m2m1shot2_cancel_context(struct m2m1shot2_context *ctx)
{
	unsigned long flags;

	spin_lock_irqsave(&ctx->m21dev->lock_ctx, flags);
	WARN_ON(ctx->state != 0);
	list_del_init(&ctx->node);
	spin_unlock_irqrestore(&ctx->m21dev->lock_ctx, flags);
}

/*
 * m2m1shot2_destroy_context() may be called during the context to be destroyed
 * is ready for processing or currently waiting for completion of processing due
 * to the non-blocking interface. Therefore it should remove the context from
 * m2m1shot2_device.active_contexts if the context is in the list. If it is
 * currently being processed by H/W, this function should wait until the H/W
 * finishes.
 */
static void m2m1shot2_destroy_context(struct kref *kref)
{
	struct m2m1shot2_context *ctx = container_of(kref,
					struct m2m1shot2_context, starter);

	m2m1shot2_cancel_context(ctx);

	sync_timeline_destroy(&ctx->timeline->obj);

	ctx->m21dev->ops->free_context(ctx);

	kfree(ctx);
}

static int m2m1shot2_release(struct inode *inode, struct file *filp)
{
	struct m2m1shot2_context *ctx = filp->private_data;

	kref_put(&ctx->starter, m2m1shot2_destroy_context);

	return 0;
}

static long m2m1shot2_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	return -ENOTTY;
}

#ifdef CONFIG_COMPAT
static long m2m1shot2_compat_ioctl32(struct file *filp,
				unsigned int cmd, unsigned long arg)
{
	return -ENXIO;
}
#endif

static const struct file_operations m2m1shot2_fops = {
	.owner          = THIS_MODULE,
	.open           = m2m1shot2_open,
	.release        = m2m1shot2_release,
	.unlocked_ioctl	= m2m1shot2_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl	= m2m1shot2_compat_ioctl32,
#endif
};

struct m2m1shot2_device *m2m1shot2_create_device(struct device *dev,
					const struct m2m1shot2_devops *ops,
					const char *nodename, int id)
{
	struct m2m1shot2_device *m21dev;
	char *name;
	size_t name_size;
	int ret = -ENOMEM;

	if (!ops || !ops->init_context || !ops->free_context ||
			!ops->prepare_format || !ops->device_run) {
		dev_err(dev,
			"%s: m2m1shot2_devops is insufficient\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	if (!nodename) {
		dev_err(dev, "%s: node name is not specified\n", __func__);
		return ERR_PTR(-EINVAL);
	}

	name_size = strlen(nodename) + 1;

	if (id >= 0)
		name_size += 3; /* instance number: maximum 3 digits */

	name = kmalloc(name_size, GFP_KERNEL);
	if (!name)
		return ERR_PTR(-ENOMEM);

	if (id < 0)
		strncpy(name, nodename, name_size);
	else
		scnprintf(name, name_size, "%s%d", nodename, id);

	m21dev = kzalloc(sizeof(*m21dev), GFP_KERNEL);
	if (!m21dev)
		goto err_m21dev;

	m21dev->misc.minor = MISC_DYNAMIC_MINOR;
	m21dev->misc.name = name;
	m21dev->misc.fops = &m2m1shot2_fops;
	ret = misc_register(&m21dev->misc);
	if (ret)
		goto err_misc;

	spin_lock_init(&m21dev->lock_ctx);
	INIT_LIST_HEAD(&m21dev->contexts);
	INIT_LIST_HEAD(&m21dev->active_contexts);

	m21dev->dev = dev;
	m21dev->ops = ops;

	return m21dev;

err_misc:
	kfree(m21dev);
err_m21dev:
	kfree(name);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL(m2m1shot2_create_device);

void m2m1shot2_destroy_device(struct m2m1shot2_device *m21dev)
{
	misc_deregister(&m21dev->misc);
	kfree(m21dev->misc.name);
	kfree(m21dev);
	/* TODO: something forgot to release? */
}
EXPORT_SYMBOL(m2m1shot2_destroy_device);
