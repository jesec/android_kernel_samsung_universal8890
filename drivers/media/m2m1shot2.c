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

#define M2M1SHOT2_FENCE_MASK (M2M1SHOT2_IMGFLAG_ACQUIRE_FENCE |		\
					M2M1SHOT2_IMGFLAG_RELEASE_FENCE)

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

static void m2m1shot2_put_userptr(struct m2m1shot2_dma_buffer *plane)
{
	/* TODO: implement */
	plane->userptr.vma = NULL;
	plane->userptr.addr = 0;
}

static void m2m1shot2_put_dmabuf(struct m2m1shot2_dma_buffer *plane)
{
	dma_buf_detach(plane->dmabuf.dmabuf, plane->dmabuf.attachment);
	dma_buf_put(plane->dmabuf.dmabuf);
	plane->dmabuf.dmabuf = NULL;
	plane->dmabuf.attachment = NULL;
}

static void m2m1shot2_put_buffer(u32 memory,
				 struct m2m1shot2_dma_buffer plane[],
				 unsigned int num_planes)
{
	unsigned int i;

	if (memory == M2M1SHOT2_BUFTYPE_DMABUF) {
		for (i = 0; i < num_planes; i++)
			m2m1shot2_put_dmabuf(&plane[i]);
	} else if (memory == M2M1SHOT2_BUFTYPE_USERPTR) {
		for (i = 0; i < num_planes; i++)
			m2m1shot2_put_userptr(&plane[i]);
	}
}

static void m2m1shot2_put_image(struct m2m1shot2_context_image *img)
{
	if (img->fence) {
		sync_fence_put(img->fence);
		img->fence = NULL;
	}

	m2m1shot2_put_buffer(img->memory, img->plane, img->num_planes);

	img->memory = M2M1SHOT2_BUFTYPE_NONE;
}

static void m2m1shot2_put_source_images(struct m2m1shot2_context *ctx)
{
	unsigned int i;

	for (i = 0; i < ctx->num_sources; i++)
		m2m1shot2_put_image(&ctx->source[i].img);

	ctx->num_sources = 0;
}

static void m2m1shot2_put_images(struct m2m1shot2_context *ctx)
{
	m2m1shot2_put_source_images(ctx);
	m2m1shot2_put_image(&ctx->target);
}

static void m2m1shot2_cancel_context(struct m2m1shot2_context *ctx)
{
	unsigned long flags;

	/* signal all possible release fences */
	sw_sync_timeline_inc(ctx->timeline, 1);

	spin_lock_irqsave(&ctx->m21dev->lock_ctx, flags);
	WARN_ON(ctx->state != 0);
	list_del_init(&ctx->node);
	spin_unlock_irqrestore(&ctx->m21dev->lock_ctx, flags);

	m2m1shot2_put_images(ctx);
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

static int m2m1shot2_get_userptr(struct device *dev,
				 struct m2m1shot2_dma_buffer *plane,
				 unsigned long addr, u32 length)
{
	/* TODO: implement */
	return -EFAULT;
}

static int m2m1shot2_get_dmabuf(struct device *dev,
				struct m2m1shot2_dma_buffer *plane,
				int fd, u32 offset, size_t payload)
{
	struct dma_buf *dmabuf = dma_buf_get(fd);
	int ret = -EINVAL;

	if (IS_ERR(dmabuf)) {
		dev_err(dev, "%s: failed to get dmabuf from fd %d\n",
			__func__, fd);
		return PTR_ERR(dmabuf);
	}

	if (dmabuf->size < offset) {
		dev_err(dev, "%s: too large offset %u for dmabuf of %zu\n",
				__func__, offset, dmabuf->size);
		goto err;
	}

	if ((dmabuf->size - offset) < payload) {
		dev_err(dev, "%s: too small dmabuf %zu/%u but reqiured %zu\n",
			__func__, dmabuf->size, offset, payload);
		goto err;
	}

	if ((dmabuf == plane->dmabuf.dmabuf) &&
			(dmabuf->size == plane->dmabuf.dmabuf->size) &&
				(dmabuf->file == plane->dmabuf.dmabuf->file)) {
		/* do not attach dmabuf again for the same buffer */
		dma_buf_put(dmabuf);
		return 0;
	}

	/* release the previous buffer */
	if (plane->dmabuf.dmabuf != NULL)
		m2m1shot2_put_dmabuf(plane);

	plane->dmabuf.attachment = dma_buf_attach(dmabuf, dev);
	if (IS_ERR(plane->dmabuf.attachment)) {
		dev_err(dev, "%s: failed to attach to dmabuf\n", __func__);
		ret = PTR_ERR(plane->dmabuf.attachment);
		goto err;
	}

	plane->dmabuf.dmabuf = dmabuf;

	return 0;
err:
	dma_buf_put(dmabuf);
	return ret;
}

static int m2m1shot2_get_buffer(struct m2m1shot2_context *ctx,
				struct m2m1shot2_context_image *img,
				struct m2m1shot2_image *src,
				size_t payload[])
{
	unsigned int num_planes = src->num_planes;
	int ret = 0;
	unsigned int i;

	if (src->memory != img->memory)
		m2m1shot2_put_buffer(img->memory, img->plane, img->num_planes);

	img->memory = M2M1SHOT2_BUFTYPE_NONE;

	if (src->memory == M2M1SHOT2_BUFTYPE_DMABUF) {
		for (i = 0; i < src->num_planes; i++) {
			ret = m2m1shot2_get_dmabuf(ctx->m21dev->dev,
					&img->plane[i], src->plane[i].fd,
					src->plane[i].offset, payload[i]);
			if (ret) {
				while (i-- > 0)
					m2m1shot2_put_dmabuf(&img->plane[i]);
				return ret;
			}

			img->plane[i].payload = payload[i];
		}
	} else if (src->memory == M2M1SHOT2_BUFTYPE_USERPTR) {
		for (i = 0; i < src->num_planes; i++) {
			if (src->plane[i].offset != 0) {
				dev_err(ctx->m21dev->dev,
					"%s: offset should be 0 with userptr\n",
					__func__);
				ret = -EINVAL;
			} else {
				ret = m2m1shot2_get_userptr(ctx->m21dev->dev,
					&img->plane[i], src->plane[i].userptr,
					src->plane[i].length);
			}

			if (ret) {
				while (i-- > 0)
					m2m1shot2_put_userptr(&img->plane[i]);
				return ret;
			}

			img->plane[i].payload = payload[i];
		}
	} else {
		dev_err(ctx->m21dev->dev,
			"%s: invalid memory type %d\n", __func__, src->memory);
		return -EINVAL;
	}

	img->num_planes = num_planes;
	img->memory = src->memory;

	return 0;
}

static int m2m1shot2_get_source(struct m2m1shot2_context *ctx,
				unsigned int index,
				struct m2m1shot2_source_image *img,
				struct m2m1shot2_image *src)
{
	struct device *dev = ctx->m21dev->dev;
	size_t payload[M2M1SHOT2_MAX_PLANES];
	unsigned int i, num_planes;
	int ret;

	if (!M2M1SHOT2_BUFTYPE_VALID(src->memory)) {
		dev_err(dev,
			"%s: invalid memory type %u specified for image %u\n",
			__func__, src->memory, index);
		return -EINVAL;
	}

	img->img.fmt.fmt = src->fmt;

	ret = ctx->m21dev->ops->prepare_format(&img->img.fmt, index,
				DMA_TO_DEVICE, payload, &num_planes);
	if (ret) {
		dev_err(dev, "%s: invalid format specified for image %u\n",
			__func__, index);
		return ret;
	}

	img->img.flags = src->flags;

	if (src->memory == M2M1SHOT2_BUFTYPE_EMPTY) {
		m2m1shot2_put_image(&img->img);
		img->img.memory = src->memory;
		img->img.num_planes = 0;
		/* no buffer, no fence */
		img->img.flags &= ~(M2M1SHOT2_IMGFLAG_ACQUIRE_FENCE |
					M2M1SHOT2_IMGFLAG_RELEASE_FENCE);
		return 0;
	}

	BUG_ON((num_planes < 1) || (num_planes > M2M1SHOT2_MAX_PLANES));

	if (num_planes != src->num_planes) {
		dev_err(dev, "%s: wrong number of planes %u of image %u.\n",
			__func__, src->num_planes, index);
		return -EINVAL;
	}

	for (i = 0; i < num_planes; i++) {
		if (src->plane[i].length < payload[i]) {
			dev_err(dev,
				"%s: too small size %u (plane %u / image %u)\n",
				__func__, src->plane[i].length, i, index);
			return -EINVAL;
		}
	}

	return m2m1shot2_get_buffer(ctx, &img->img, src, payload);
}

static int m2m1shot2_get_sources(struct m2m1shot2_context *ctx,
				 struct m2m1shot2_image __user *usersrc,
				 bool blocking)
{
	struct device *dev = ctx->m21dev->dev;
	struct m2m1shot2_source_image *image = ctx->source;
	unsigned int i;
	int ret;

	for (i = 0; i < ctx->num_sources; i++, usersrc++) {
		struct m2m1shot2_image source;

		if (copy_from_user(&source, usersrc, sizeof(source))) {
			dev_err(dev,
				"%s: Failed to read source[%u] image data\n",
				__func__, i);
			ret = -EFAULT;
			goto err;
		}

		/* blocking mode does not allow fences */
		if (blocking && !!(source.flags & M2M1SHOT2_FENCE_MASK)) {
			dev_err(dev, "%s: fence set for blocking mode\n",
				__func__);
			ret = -EINVAL;
			goto err;
		}

		ret = m2m1shot2_get_source(ctx, i, &image[i], &source);
		if (ret)
			goto err;

		if (!!(image[i].img.flags & M2M1SHOT2_IMGFLAG_ACQUIRE_FENCE)) {
			image[i].img.fence = sync_fence_fdget(source.fence);
			if (image[i].img.fence == NULL) {
				dev_err(dev, "%s: invalid acquire fence %d\n",
					__func__, source.fence);
				ret = -EINVAL;
				m2m1shot2_put_image(&image[i].img);
				goto err;
			}
		}
	}

	return 0;
err:
	while (i-- > 0)
		m2m1shot2_put_image(&ctx->source[i].img);

	ctx->num_sources = 0;

	return ret;

}

static struct sync_fence *m2m1shot2_create_fence(struct m2m1shot2_context *ctx)
{
	struct device *dev = ctx->m21dev->dev;
	struct sync_fence *fence;
	struct sync_pt *pt;

	pt = sw_sync_pt_create(ctx->timeline, ctx->timeline_max + 1);
	if (!pt) {
		dev_err(dev,
			"%s: failed to create sync_pt\n", __func__);
		return NULL;
	}

	fence = sync_fence_create("m2m1shot2", pt);
	if (!fence) {
		dev_err(dev, "%s: failed to create fence\n", __func__);
		sync_pt_free(pt);
	}

	return fence;
}

static int m2m1shot2_get_target(struct m2m1shot2_context *ctx,
				struct m2m1shot2_image *dst)
{
	struct device *dev = ctx->m21dev->dev;
	struct m2m1shot2_context_image *img = &ctx->target;
	size_t payload[M2M1SHOT2_MAX_PLANES];
	unsigned int i, num_planes;
	int ret;

	if (!M2M1SHOT2_BUFTYPE_VALID(dst->memory)) {
		dev_err(dev,
			"%s: invalid memory type %u specified for target\n",
			__func__, dst->memory);
		return -EINVAL;
	}

	if (dst->memory == M2M1SHOT2_BUFTYPE_EMPTY) {
		dev_err(dev,
			"%s: M2M1SHOT2_BUFTYPE_EMPTY is not valid for target\n",
			__func__);
		return -EINVAL;
	}

	img->fmt.fmt = dst->fmt;
	/*
	 * The client driver may configure 0 to payload if it is not able to
	 * determine the payload before image processing especially when the
	 * result is a compressed data
	 */
	ret = ctx->m21dev->ops->prepare_format(&img->fmt, 0,
				DMA_FROM_DEVICE, payload, &num_planes);
	if (ret) {
		dev_err(dev, "%s: invalid format specified for target\n",
			__func__);
		return ret;
	}

	BUG_ON((num_planes < 1) || (num_planes > M2M1SHOT2_MAX_PLANES));

	if (num_planes != dst->num_planes) {
		dev_err(dev, "%s: wrong number of planes %u for target\n",
			__func__, dst->num_planes);
		return -EINVAL;
	}

	for (i = 0; i < num_planes; i++) {
		if ((payload[i] != 0) && (dst->plane[i].length < payload[i])) {
			dev_err(dev,
				"%s: too small size %u (plane %u)\n",
				__func__, dst->plane[i].length, i);
			return -EINVAL;
		}
	}

	ret = m2m1shot2_get_buffer(ctx, img, dst, payload);
	if (ret)
		return ret;

	img->flags = dst->flags;

	return 0;
}

static int m2m1shot2_get_userdata(struct m2m1shot2_context *ctx,
				  struct m2m1shot2 *data)
{
	struct device *dev = ctx->m21dev->dev;
	struct m2m1shot2_image __user *usersrc = data->sources;
	bool blocking;
	int ret;

	if ((data->num_sources < 1) ||
			(data->num_sources > M2M1SHOT2_MAX_IMAGES)) {
		dev_err(dev, "%s: Invalid number of source images %u\n",
			__func__, data->num_sources);
		return -EINVAL;
	}

	blocking = !(data->flags & M2M1SHOT2_FLAG_NONBLOCK);

	ctx->num_sources = data->num_sources;
	ctx->flags = data->flags;

	ret = m2m1shot2_get_sources(ctx, usersrc, blocking);
	if (ret)
		return ret;

	if (blocking && !!(data->target.flags & M2M1SHOT2_FENCE_MASK)) {
		dev_err(dev, "%s: fence set for blocking mode\n", __func__);
		ret = -EINVAL;
		goto err;
	}

	ret = m2m1shot2_get_target(ctx, &data->target);
	if (ret)
		goto err;

	return 0;
err:
	m2m1shot2_put_source_images(ctx);
	return ret;
}

static int m2m1shot2_install_fence(struct m2m1shot2_context *ctx,
				   struct sync_fence *fence,
				   s32 __user *pfd)
{
	int fd = get_unused_fd();

	if (fd < 0) {
		dev_err(ctx->m21dev->dev, "%s: failed to allocated unused fd\n",
			__func__);
		return fd;
	}

	if (put_user(fd, pfd)) {
		dev_err(ctx->m21dev->dev,
			"%s: failed to put release fence to user\n", __func__);
		put_unused_fd(fd);
		return -EFAULT;
	}

	sync_fence_install(fence, fd);

	return fd;
}

static int m2m1shot2_create_release_fence(struct m2m1shot2_context *ctx,
					struct m2m1shot2_image __user *usertgt,
					struct m2m1shot2_image __user usersrc[],
					unsigned int num_sources)
{
	struct sync_fence *fence = NULL;
	struct m2m1shot2_context_image *img;
	unsigned int i, ret = 0, ifd = 0;
	int fds[M2M1SHOT2_MAX_IMAGES + 1];
	unsigned int num_fences = 0;

	if (!!(ctx->target.flags & M2M1SHOT2_IMGFLAG_RELEASE_FENCE))
		num_fences++;

	for (i = 0; i < ctx->num_sources; i++) {
		if (!!(ctx->source[i].img.flags &
					M2M1SHOT2_IMGFLAG_RELEASE_FENCE))
			num_fences++;
	}

	if (num_fences == 0)
		return 0;

	fence = m2m1shot2_create_fence(ctx);
	if (!fence)
		return -ENOMEM;

	for (i = 0; i < ctx->num_sources; i++) {
		img = &ctx->source[i].img;
		if (!!(img->flags & M2M1SHOT2_IMGFLAG_RELEASE_FENCE)) {
			ret = m2m1shot2_install_fence(ctx,
					fence, &usersrc[i].fence);
			if (ret < 0)
				goto err;

			get_file(fence->file);
			fds[ifd++] = ret;
		}
	}

	img = &ctx->target;
	if (!!(ctx->target.flags & M2M1SHOT2_IMGFLAG_RELEASE_FENCE)) {
		ret = m2m1shot2_install_fence(ctx, fence,
						&usertgt->fence);
		if (ret < 0)
			goto err;

		get_file(fence->file);
		fds[ifd++] = ret;
	}

	/* release a reference of the fence that is increased on creation */
	sync_fence_put(fence);

	ctx->timeline_max++;

	return 0;
err:
	while (ifd-- > 0) {
		put_unused_fd(fds[ifd]);
		sync_fence_put(fence);
	}

	sync_fence_put(fence);

	return ret;
}

static long m2m1shot2_ioctl(struct file *filp,
			    unsigned int cmd, unsigned long arg)
{
	struct m2m1shot2_context *ctx = filp->private_data;
	int ret = 0;

	mutex_lock(&ctx->mutex);

	switch (cmd) {
	case M2M1SHOT2_IOC_PROCESS:
	{
		struct m2m1shot2 __user *uptr = (struct m2m1shot2 __user *)arg;
		struct m2m1shot2 data;

		if (!M2M1S2_CTXSTATE_IDLE(ctx)) {
			dev_err(ctx->m21dev->dev,
				"%s: m2m1shot2 does not allow queueing tasks\n",
				__func__);
			ret = -EINVAL;
			break;
		}

		set_bit(M2M1S2_CTXSTATE_PROCESSING, &ctx->state);
		set_bit(M2M1S2_CTXSTATE_PENDING, &ctx->state);

		if (copy_from_user(&data, (void __user *)arg, sizeof(data))) {
			dev_err(ctx->m21dev->dev,
				"%s: Failed to read userdata\n", __func__);
			ret = -EFAULT;
			break;
		}

		ret = m2m1shot2_get_userdata(ctx, &data);
		if (ret < 0)
			break;

		if (!!(data.flags & M2M1SHOT2_FLAG_NONBLOCK)) {
			ret = m2m1shot2_create_release_fence(ctx, &uptr->target,
						data.sources, data.num_sources);
			if (ret < 0) {
				m2m1shot2_put_images(ctx);
				break;
			}
		}

		/* TODO: process user's request */
		/* TODO: if processing fails,
		   then free all file descriptors related o the rlease fence */
		if (!(data.flags & M2M1SHOT2_FLAG_NONBLOCK)) {
			clear_bit(M2M1S2_CTXSTATE_PROCESSING, &ctx->state);
			clear_bit(M2M1S2_CTXSTATE_PENDING, &ctx->state);
			clear_bit(M2M1S2_CTXSTATE_PROCESSED, &ctx->state);
			clear_bit(M2M1S2_CTXSTATE_WAITING, &ctx->state);
			/* TODO: inform user error
			if (test_bit(M2M1S2_CTXSTATE_ERROR, &ctx->state)) {
			}
			*/
			clear_bit(M2M1S2_CTXSTATE_ERROR, &ctx->state);
		}

		break;
	}
	case M2M1SHOT2_IOC_WAIT_PROCESS:
	{
		ret = -ENOTTY;
		break;
	}
	default:
	{
		dev_err(ctx->m21dev->dev,
			"%s: unknown ioctl command %#x\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}
	} /* switch */

	mutex_unlock(&ctx->mutex);

	return ret;
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
