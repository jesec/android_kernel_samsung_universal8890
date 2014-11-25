/* linux/drivers/iommu/exynos_iovmm.c
 *
 * Copyright (c) 2011-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifdef CONFIG_EXYNOS_IOMMU_DEBUG
#define DEBUG
#endif

#include <linux/kernel.h>
#include <linux/hardirq.h>
#include <linux/slab.h>
#include <linux/scatterlist.h>
#include <linux/err.h>
#include <linux/debugfs.h>

#include <linux/exynos_iovmm.h>
#include <plat/cpu.h>

#include "exynos-iommu.h"

#define SZ_768M (SZ_512M + SZ_256M)

static int find_iovm_id(struct exynos_iovmm *vmm,
		struct exynos_vm_region *region)
{
	int ret = 0;

	if ((region->start < IOVA_START) ||
				(region->start > (IOVA_START + IOVM_SIZE)))
			ret = -EINVAL;
	return 0;
}


/* alloc_iovm_region - Allocate IO virtual memory region
 * vmm: virtual memory allocator
 * size: total size to allocate vm region from @vmm.
 * align: alignment constraints of the allocated virtual address
 * max_align: maximum alignment of allocated virtual address. allocated address
 *            does not need to satisfy larger alignment than max_align.
 * exact_align_mask: constraints of the special case that allocated address
 *            must satisfy when it is multiple of align but of max_align.
 *            If this is not 0, allocated address must satisfy the following
 *            constraint:
 *            ((allocated address) % max_align) / align = exact_align_mask
 * offset: must be smaller than PAGE_SIZE. Just a valut to be added to the
 *         allocated virtual address. This does not effect to the allocaded size
 *         and address.
 *
 * This function returns allocated IO virtual address that satisfies the given
 * constraints. Returns 0 if this function is not able to allocate IO virtual
 * memory
 */
static dma_addr_t alloc_iovm_region(struct exynos_iovmm *vmm, size_t size,
			size_t align, size_t max_align, size_t exact_align_mask,
			off_t offset, int id)
{
	dma_addr_t index = 0;
	dma_addr_t vstart;
	size_t vsize;
	unsigned long end, i;
	struct exynos_vm_region *region;

	BUG_ON(align & (align - 1));
	BUG_ON(offset >= PAGE_SIZE);

	vsize = PAGE_ALIGN(size) >> PAGE_SHIFT;
	align >>= PAGE_SHIFT;
	exact_align_mask >>= PAGE_SHIFT;
	max_align >>= PAGE_SHIFT;

	spin_lock(&vmm->bitmap_lock);
again:
	index = find_next_zero_bit(vmm->vm_map[id],
			IOVM_NUM_PAGES(vmm->iovm_size[id]), index);

	if (align) {
		if (exact_align_mask) {
			if ((index & ~(align - 1) & (max_align - 1)) >
							exact_align_mask)
				index = ALIGN(index, max_align);
			index |= exact_align_mask;
		} else {
			index = ALIGN(index, align);
		}

		if (index >= IOVM_NUM_PAGES(vmm->iovm_size[id])) {
			spin_unlock(&vmm->bitmap_lock);
			return 0;
		}

		if (test_bit(index, vmm->vm_map[id]))
			goto again;
	}

	end = index + vsize;

	if (end >= IOVM_NUM_PAGES(vmm->iovm_size[id])) {
		spin_unlock(&vmm->bitmap_lock);
		return 0;
	}

	i = find_next_bit(vmm->vm_map[id], end, index);
	if (i < end) {
		index = i + 1;
		goto again;
	}

	bitmap_set(vmm->vm_map[id], index, vsize);

	spin_unlock(&vmm->bitmap_lock);

	vstart = (index << PAGE_SHIFT) + vmm->iova_start[id] + offset;

	region = kmalloc(sizeof(*region), GFP_KERNEL);
	if (unlikely(!region)) {
		spin_lock(&vmm->bitmap_lock);
		bitmap_clear(vmm->vm_map[id], index, vsize);
		spin_unlock(&vmm->bitmap_lock);
		return 0;
	}

	INIT_LIST_HEAD(&region->node);
	region->start = vstart;
	region->size = vsize << PAGE_SHIFT;

	spin_lock(&vmm->vmlist_lock);
	list_add_tail(&region->node, &vmm->regions_list);
	vmm->allocated_size[id] += region->size;
	vmm->num_areas[id]++;
	vmm->num_map++;
	spin_unlock(&vmm->vmlist_lock);

	return region->start;
}

struct exynos_vm_region *find_iovm_region(struct exynos_iovmm *vmm,
							dma_addr_t iova)
{
	struct exynos_vm_region *region;

	spin_lock(&vmm->vmlist_lock);

	list_for_each_entry(region, &vmm->regions_list, node) {
		if (region->start <= iova &&
			(region->start + region->size) > iova) {
			spin_unlock(&vmm->vmlist_lock);
			return region;
		}
	}

	spin_unlock(&vmm->vmlist_lock);

	return NULL;
}

static struct exynos_vm_region *remove_iovm_region(struct exynos_iovmm *vmm,
							dma_addr_t iova)
{
	struct exynos_vm_region *region;

	spin_lock(&vmm->vmlist_lock);

	list_for_each_entry(region, &vmm->regions_list, node) {
		if (region->start == iova) {
			int id;

			id = find_iovm_id(vmm, region);
			if (id < 0)
				continue;

			list_del(&region->node);
			vmm->allocated_size[id] -= region->size;
			vmm->num_areas[id]--;
			vmm->num_unmap++;
			spin_unlock(&vmm->vmlist_lock);
			return region;
		}
	}

	spin_unlock(&vmm->vmlist_lock);

	return NULL;
}

static void free_iovm_region(struct exynos_iovmm *vmm,
				struct exynos_vm_region *region)
{
	int id;

	if (!region)
		return;

	id = find_iovm_id(vmm, region);
	if (id < 0) {
		kfree(region);
		return;
	}

	spin_lock(&vmm->bitmap_lock);
	bitmap_clear(vmm->vm_map[id],
			(region->start - vmm->iova_start[id]) >> PAGE_SHIFT,
			region->size >> PAGE_SHIFT);
	spin_unlock(&vmm->bitmap_lock);

	SYSMMU_EVENT_LOG_IOVMM_UNMAP(IOVMM_TO_LOG(vmm),
			region->start, region->start + region->size);

	kfree(region);
}

static dma_addr_t add_iovm_region(struct exynos_iovmm *vmm,
					dma_addr_t start, size_t size)
{
	struct exynos_vm_region *region, *pos;

	region = kmalloc(sizeof(*region), GFP_KERNEL);
	if (!region)
		return 0;

	INIT_LIST_HEAD(&region->node);
	region->start = start;
	region->size = size;

	spin_lock(&vmm->vmlist_lock);

	list_for_each_entry(pos, &vmm->regions_list, node) {
		if ((start < (pos->start + pos->size)) &&
					((start + size) > pos->start)) {
			spin_unlock(&vmm->vmlist_lock);
			kfree(region);
			return 0;
		}
	}

	list_add(&region->node, &vmm->regions_list);

	spin_unlock(&vmm->vmlist_lock);

	return start;
}

static void show_iovm_regions(struct exynos_iovmm *vmm)
{
	struct exynos_vm_region *pos;

	pr_err("LISTING IOVMM REGIONS...\n");
	spin_lock(&vmm->vmlist_lock);
	list_for_each_entry(pos, &vmm->regions_list, node) {
		pr_err("REGION: %pa (SIZE: %#zx)\n", &pos->start, pos->size);
	}
	spin_unlock(&vmm->vmlist_lock);
	pr_err("END OF LISTING IOVMM REGIONS...\n");
}

int iovmm_activate(struct device *dev)
{
	struct exynos_iovmm *vmm = exynos_get_iovmm(dev);

	return iommu_attach_device(vmm->domain, dev);
}

void iovmm_deactivate(struct device *dev)
{
	struct exynos_iovmm *vmm = exynos_get_iovmm(dev);

	iommu_detach_device(vmm->domain, dev);
}

/* iovmm_map - allocate and map IO virtual memory for the given device
 * dev: device that has IO virtual address space managed by IOVMM
 * sg: list of physically contiguous memory chunks. The preceding chunk needs to
 *     be larger than the following chunks in sg for efficient mapping and
 *     performance. If elements of sg are more than one, physical address of
 *     each chunk needs to be aligned by its size for efficent mapping and TLB
 *     utilization.
 * offset: offset in bytes to be mapped and accessed by dev.
 * size: size in bytes to be mapped and accessed by dev.
 *
 * This function allocates IO virtual memory for the given device and maps the
 * given physical memory conveyed by sg into the allocated IO memory region.
 * Returns allocated IO virtual address if it allocates and maps successfull.
 * Otherwise, minus error number. Caller must check if the return value of this
 * function with IS_ERR_VALUE().
 */
dma_addr_t iovmm_map(struct device *dev, struct scatterlist *sg, off_t offset,
		size_t size, enum dma_data_direction direction, int id)
{
	off_t start_off;
	dma_addr_t addr, start = 0;
	size_t mapped_size = 0;
	struct exynos_iovmm *vmm = exynos_get_iovmm(dev);
	size_t exact_align_mask = 0;
	size_t max_align, align;
	int ret = 0;
	int idx;
	struct scatterlist *tsg;

	/* Dedicate IOVMM Region id = 0 */
	id = 0;

	for (; (sg != NULL) && (sg->length < offset); sg = sg_next(sg))
		offset -= sg->length;

	if (sg == NULL) {
		dev_err(dev, "IOVMM: invalid offset to %s.\n", __func__);
		return -EINVAL;
	}

	tsg = sg;

	start_off = offset_in_page(sg_phys(sg) + offset);
	size = PAGE_ALIGN(size + start_off);

	if (size >= SECT_SIZE)
		max_align = SECT_SIZE;
	else if (size < LPAGE_SIZE)
		max_align = SPAGE_SIZE;
	else
		max_align = LPAGE_SIZE;

	if (sg_next(sg) == NULL) {/* physically contiguous chunk */
		/* 'align' must be biggest 2^n that satisfies:
		 * 'address of physical memory' % 'align' = 0
		 */
		align = 1 << __ffs(page_to_phys(sg_page(sg)));

		exact_align_mask = page_to_phys(sg_page(sg)) & (max_align - 1);

		if ((size - exact_align_mask) < max_align) {
			max_align /= 16;
			exact_align_mask = exact_align_mask & (max_align - 1);
		}

		if (align > max_align)
			align = max_align;

		exact_align_mask &= ~(align - 1);
	} else {
		align = 1 << __ffs(page_to_phys(sg_page(sg)));
		align = min_t(size_t, align, max_align);
		max_align = align;
	}

	start = alloc_iovm_region(vmm, size, align, max_align,
				exact_align_mask, start_off, id);
	if (!start) {
		spin_lock(&vmm->vmlist_lock);
		dev_err(dev, "%s: Not enough IOVM space to allocate %#zx/%#zx\n",
				__func__, size, align);
		dev_err(dev, "%s: Total %#zx (%d), Allocated %#zx , Chunks %d\n",
				__func__, vmm->iovm_size[id], id,
				vmm->allocated_size[id], vmm->num_areas[id]);
		spin_unlock(&vmm->vmlist_lock);
		ret = -ENOMEM;
		goto err_map_nomem;
	}

	addr = start - start_off;
	do {
		phys_addr_t phys;
		size_t len;

		phys = sg_phys(sg);
		len = sg->length;

		/* if back to back sg entries are contiguous consolidate them */
		while (sg_next(sg) &&
		       sg_phys(sg) + sg->length == sg_phys(sg_next(sg))) {
			len += sg_next(sg)->length;
			sg = sg_next(sg);
		}

		if (offset > 0) {
			len -= offset;
			phys += offset;
			offset = 0;
		}

		if (offset_in_page(phys)) {
			len += offset_in_page(phys);
			phys = round_down(phys, PAGE_SIZE);
		}

		len = PAGE_ALIGN(len);

		if (len > (size - mapped_size))
			len = size - mapped_size;

		ret = iommu_map(vmm->domain, addr, phys, len, 0);
		if (ret) {
			dev_err(dev, "iommu_map failed w/ err: %d\n", ret);
			break;
		}

		addr += len;
		mapped_size += len;
	} while ((sg = sg_next(sg)) && (mapped_size < size));

	BUG_ON(mapped_size > size);

	if (mapped_size < size) {
		dev_err(dev, "mapped_size(%#zx) is smaller than size(%#zx)\n",
				mapped_size, size);
		if (!ret) {
			dev_err(dev, "ret: %d\n", ret);
			ret = -EINVAL;
		}
		goto err_map_map;
	}

	TRACE_LOG_DEV(dev, "IOVMM: Allocated VM region @ %#x/%#x bytes.\n",
								start, size);

	{
		struct exynos_vm_region *reg = find_iovm_region(vmm, start);
		BUG_ON(!reg);

		SYSMMU_EVENT_LOG_IOVMM_MAP(IOVMM_TO_LOG(vmm),
					start, start + size, reg->size - size);
	}

	return start;

err_map_map:
	iommu_unmap(vmm->domain, start - start_off, mapped_size);
	free_iovm_region(vmm, remove_iovm_region(vmm, start));

	dev_err(dev,
	"Failed(%d) to map IOVMM REGION %pa (SIZE: %#zx, mapped: %#zx)\n",
		ret, &start, size, mapped_size);
	idx = 0;
	do {
		pr_err("SGLIST[%d].size = %#x\n", idx++, tsg->length);
	} while ((tsg = sg_next(tsg)));

	show_iovm_regions(vmm);

err_map_nomem:
	TRACE_LOG_DEV(dev,
		"IOVMM: Failed to allocated VM region for %#x bytes.\n", size);
	return (dma_addr_t)ret;
}

void iovmm_unmap(struct device *dev, dma_addr_t iova)
{
	struct exynos_iovmm *vmm = exynos_get_iovmm(dev);
	struct exynos_vm_region *region;
	size_t unmap_size;

	/* This function must not be called in IRQ handlers */
	BUG_ON(in_irq());

	region = remove_iovm_region(vmm, iova);
	if (region) {
		if (WARN_ON(region->start != iova)) {
			dev_err(dev,
			"IOVMM: iova %pa and region %#zx @ %pa mismatch\n",
				&iova, region->size, &region->start);
			show_iovm_regions(vmm);
			/* reinsert iovm region */
			add_iovm_region(vmm, region->start, region->size);
			kfree(region);
			return;
		}
		unmap_size = iommu_unmap(vmm->domain, iova & PAGE_MASK,
							region->size);
		if (unlikely(unmap_size != region->size)) {
			dev_err(dev, "Failed to unmap IOVMM REGION %pa "
				"(SIZE: %#zx, iova: %pa, unmapped: %#zx)\n",
				&region->start,
				region->size, &iova, unmap_size);
			show_iovm_regions(vmm);
			kfree(region);
			BUG();
			return;
		}

		exynos_sysmmu_tlb_invalidate(vmm->domain, region->start, region->size);

		free_iovm_region(vmm, region);

		TRACE_LOG_DEV(dev, "IOVMM: Unmapped %#x bytes from %#x.\n",
						unmap_size, iova);
	} else {
		dev_err(dev, "IOVMM: No IOVM region %pa to free.\n", &iova);
	}
}

int iovmm_map_oto(struct device *dev, phys_addr_t phys, size_t size)
{
	struct exynos_iovmm *vmm = exynos_get_iovmm(dev);
	int ret;

	BUG_ON(!IS_ALIGNED(phys, PAGE_SIZE));
	BUG_ON(!IS_ALIGNED(size, PAGE_SIZE));

	if (WARN_ON((phys + size) >= IOVA_START)) {
		dev_err(dev,
			"Unable to create one to one mapping for %#zx @ %pa\n",
			size, &phys);
		return -EINVAL;
	}

	if (!add_iovm_region(vmm, (dma_addr_t)phys, size))
		return -EADDRINUSE;

	ret = iommu_map(vmm->domain, (dma_addr_t)phys, phys, size, 0);
	if (ret < 0)
		free_iovm_region(vmm,
				remove_iovm_region(vmm, (dma_addr_t)phys));

	return ret;
}

void iovmm_unmap_oto(struct device *dev, phys_addr_t phys)
{
	struct exynos_iovmm *vmm = exynos_get_iovmm(dev);
	struct exynos_vm_region *region;
	size_t unmap_size;

	/* This function must not be called in IRQ handlers */
	BUG_ON(in_irq());
	BUG_ON(!IS_ALIGNED(phys, PAGE_SIZE));

	region = remove_iovm_region(vmm, (dma_addr_t)phys);
	if (region) {
		unmap_size = iommu_unmap(vmm->domain, (dma_addr_t)phys,
							region->size);
		WARN_ON(unmap_size != region->size);

		exynos_sysmmu_tlb_invalidate(vmm->domain, (dma_addr_t)phys,
					     region->size);

		free_iovm_region(vmm, region);

		TRACE_LOG_DEV(dev, "IOVMM: Unmapped %#x bytes from %#x.\n",
						unmap_size, phys);
	}
}

static struct dentry *exynos_iovmm_debugfs_root;
static struct dentry *exynos_iommu_debugfs_root;

static int exynos_iovmm_create_debugfs(void)
{
	exynos_iovmm_debugfs_root = debugfs_create_dir("iovmm", NULL);
	if (!exynos_iovmm_debugfs_root)
		pr_err("IOVMM: Failed to create debugfs entry\n");
	else
		pr_info("IOVMMU: Created debugfs entry at debugfs/iovmm\n");

	return 0;
}
arch_initcall(exynos_iovmm_create_debugfs);

static int iovmm_debug_show(struct seq_file *s, void *unused)
{
	struct exynos_iovmm *vmm = s->private;
	int i = 0;

	seq_printf(s, "%.6s  %10.s  %10.s  %10.s  %6.s\n",
			"REGION", "VASTART", "SIZE", "FREE", "CHUNKS");
	seq_puts(s, "---------------------------------------------\n");

	spin_lock(&vmm->vmlist_lock);
	while (i < vmm->inplanes) {
		seq_printf(s, "%3s[%d]  %pa  %#10zx  %#10zx  %d\n",
				"in", i, &vmm->iova_start[i], vmm->iovm_size[i],
				vmm->iovm_size[i] - vmm->allocated_size[i],
				vmm->num_areas[i]);
		i++;
	}
	while (i < (vmm->inplanes + vmm->onplanes)) {
		seq_printf(s, "%3s[%d]  %pa  %#10zx  %#10zx  %d\n",
				"out", i - vmm->inplanes, &vmm->iova_start[i],
				vmm->iovm_size[i],
				vmm->iovm_size[i] - vmm->allocated_size[i],
				vmm->num_areas[i]);
		i++;
	}
	seq_puts(s, "---------------------------------------------\n");
	seq_printf(s, "Total number of mappings  : %d\n", vmm->num_map);
	seq_printf(s, "Total number of unmappings: %d\n", vmm->num_unmap);
	spin_unlock(&vmm->vmlist_lock);

	return 0;
}

static int iovmm_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, iovmm_debug_show, inode->i_private);
}

static ssize_t iovmm_debug_write(struct file *filp, const char __user *p,
				size_t len, loff_t *off)
{
	struct seq_file *s = filp->private_data;
	struct exynos_iovmm *vmm = s->private;
	/* clears the map count in IOVMM */
	spin_lock(&vmm->vmlist_lock);
	vmm->num_map = 0;
	vmm->num_unmap = 0;
	spin_unlock(&vmm->vmlist_lock);
	return len;
}

static const struct file_operations iovmm_debug_fops = {
	.open = iovmm_debug_open,
	.read = seq_read,
	.write = iovmm_debug_write,
	.llseek = seq_lseek,
	.release = single_release,
};

static void iovmm_register_debugfs(struct exynos_iovmm *vmm)
{
	if (!exynos_iovmm_debugfs_root)
		return;

	debugfs_create_file(vmm->domain_name, 0664,
			exynos_iovmm_debugfs_root, vmm, &iovmm_debug_fops);
}

int exynos_create_iovmm(struct device *dev, int inplanes, int onplanes)
{
	return 0;
}

struct exynos_iovmm *exynos_create_single_iovmm(const char *name)
{
	struct exynos_iovmm *vmm;
	int ret = 0;

	vmm = kzalloc(sizeof(*vmm), GFP_KERNEL);
	if (!vmm) {
		ret = -ENOMEM;
		goto err_alloc_vmm;
	}

	vmm->iovm_size[0] = IOVM_SIZE;
	vmm->iova_start[0] = IOVA_START;
	vmm->vm_map[0] = kzalloc(IOVM_BITMAP_SIZE(IOVM_SIZE), GFP_KERNEL);
	if (!vmm->vm_map[0]) {
		ret = -ENOMEM;
		goto err_setup_domain;
	}

	vmm->inplanes = 1;
	vmm->onplanes = 0;
	vmm->domain = iommu_domain_alloc(&platform_bus_type);
	if (!vmm->domain) {
		ret = -ENOMEM;
		goto err_setup_domain;
	}

	ret = exynos_iommu_init_event_log(IOVMM_TO_LOG(vmm), IOVMM_LOG_LEN);
	if (!ret) {
		iovmm_add_log_to_debugfs(exynos_iovmm_debugfs_root,
					IOVMM_TO_LOG(vmm), name);

		iommu_add_log_to_debugfs(exynos_iommu_debugfs_root,
				IOMMU_TO_LOG(vmm->domain), name);
	} else {
		goto err_init_event_log;
	}

	spin_lock_init(&vmm->vmlist_lock);
	spin_lock_init(&vmm->bitmap_lock);

	INIT_LIST_HEAD(&vmm->regions_list);

	vmm->domain_name = name;

	iovmm_register_debugfs(vmm);

	pr_debug("%s IOVMM: Created %#x B IOVMM from %#x.\n",
			name, IOVM_SIZE, IOVA_START);
	return vmm;

err_init_event_log:
	iommu_domain_free(vmm->domain);
err_setup_domain:
	kfree(vmm);
err_alloc_vmm:
	pr_err("%s IOVMM: Failed to create IOVMM (%d)\n", name, ret);

	return ERR_PTR(ret);
}

void iovmm_set_fault_handler(struct device *dev,
			     iommu_fault_handler_t handler, void *token)
{
	int ret;

	ret = exynos_sysmmu_add_fault_notifier(dev, handler, token);
	if (ret)
		dev_err(dev, "Failed to %s's fault notifier\n", dev_name(dev));
}
