/* linux/drivers/iommu/exynos7_iommu.c
 *
 * Copyright (c) 2013-2014 Samsung Electronics Co., Ltd.
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
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/debugfs.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/clk-private.h>

#include <plat/cpu.h>

#include <asm/cacheflush.h>

#include "exynos-iommu.h"

#define MODULE_NAME "exynos-sysmmu"

#define PGBASE_TO_PHYS(pgent)	(phys_addr_t)((pgent) << PG_ENT_SHIFT)

#define SPSECT_MASK (~((SPSECT_SIZE >> PG_ENT_SHIFT) - 1))
#define LSECT_MASK (~((LSECT_SIZE >> PG_ENT_SHIFT) - 1))
#define SECT_MASK (~((SECT_SIZE >> PG_ENT_SHIFT) - 1))
#define LPAGE_MASK (~((LPAGE_SIZE >> PG_ENT_SHIFT) - 1))
#define SPAGE_MASK (~((SPAGE_SIZE >> PG_ENT_SHIFT) - 1))

#define lv1ent_fault(sent) ((*(sent) & 7) == 0)
#define lv1ent_page(sent) ((*(sent) & 7) == 1)
#define lv1ent_section(sent) ((*(sent) & 7) == 2)
#define lv1ent_lsection(sent) ((*(sent) & 7) == 4)
#define lv1ent_spsection(sent) ((*(sent) & 7) == 6)

#define lv2ent_fault(pent) ((*(pent) & 3) == 0 || \
			   (PGBASE_TO_PHYS(*(pent) & SPAGE_MASK) == fault_page))
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)

#define spsection_phys(sent) PGBASE_TO_PHYS(*(sent) & SPSECT_MASK)
#define spsection_offs(iova) ((iova) & (SPSECT_SIZE - 1))
#define lsection_phys(sent) PGBASE_TO_PHYS(*(sent) & LSECT_MASK)
#define lsection_offs(iova) ((iova) & (LSECT_SIZE - 1))
#define section_phys(sent) PGBASE_TO_PHYS(*(sent) & SECT_MASK)
#define section_offs(iova) ((iova) & (SECT_SIZE - 1))
#define lpage_phys(pent) PGBASE_TO_PHYS(*(pent) & LPAGE_MASK)
#define lpage_offs(iova) ((iova) & (LPAGE_SIZE - 1))
#define spage_phys(pent) PGBASE_TO_PHYS(*(pent) & SPAGE_MASK)
#define spage_offs(iova) ((iova) & (SPAGE_SIZE - 1))

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * 4) /* 32bit page table entry */

#define lv2table_base(sent) ((*((phys_addr_t *)(sent)) & ~0x3F) << 4)

#define mk_lv1ent_spsect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 6)
#define mk_lv1ent_lsect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 4)
#define mk_lv1ent_sect(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 2)
#define mk_lv1ent_page(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 1)
#define mk_lv2ent_lpage(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 1)
#define mk_lv2ent_spage(pa) ((sysmmu_pte_t) ((pa) >> PG_ENT_SHIFT) | 2)

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0

#define CFG_QOS(n)	(((n) & 0xF) << 7)
#define CFG_MASK	0x01101FBC /* Selecting bit 24, 20, 12-7, 5-2 */
#define CFG_ACGEN	(1 << 24)
#define CFG_FLPDCACHE	(1 << 20)
#define CFG_SHAREABLE	(1 << 12)
#define CFG_QOS_OVRRIDE (1 << 11)

#define REG_MMU_CTRL		0x000
#define REG_MMU_CFG		0x004
#define REG_MMU_STATUS		0x008
#define REG_PT_BASE_PPN		0x00C
#define REG_MMU_FLUSH		0x010
#define REG_MMU_FLUSH_ENTRY	0x014
#define REG_MMU_FLUSH_RANGE	0x018
#define REG_FLUSH_RANGE_START	0x020
#define REG_FLUSH_RANGE_END	0x024
#define REG_MMU_CAPA		0x030
#define REG_MMU_VERSION		0x034
#define REG_INT_STATUS		0x060
#define REG_INT_CLEAR		0x064
#define REG_FAULT_AR_ADDR	0x070
#define REG_FAULT_AR_TRANS_INFO	0x078
#define REG_FAULT_AW_ADDR	0x080
#define REG_FAULT_AW_TRANS_INFO	0x088
#define REG_L1TLB_CFG		0x100 /* sysmmu v5.1 only */
#define REG_L1TLB_CTRL		0x104 /* sysmmu v5.1 only */
#define REG_L2TLB_CFG		0x200 /* sysmmu that has L2TLB only*/
#define REG_PB_LMM		0x300
#define REG_PB_INDICATE		0x308
#define REG_PB_CFG		0x310
#define REG_PB_START_ADDR	0x320
#define REG_PB_END_ADDR		0x328
#define REG_PB_INFO		0x350
#define REG_SW_DF_VPN		0x400 /* sysmmu v5.1 only */
#define REG_SW_DF_VPN_CMD_NUM	0x408 /* sysmmu v5.1 only */
#define REG_L1TLB_READ_ENTRY	0x750
#define REG_L1TLB_ENTRY_VPN	0x754
#define REG_L1TLB_ENTRY_PPN	0x75C
#define REG_L1TLB_ENTRY_ATTR	0x764

#define MMU_MAJ_VER(reg)	(reg >> 28)
#define MMU_MIN_VER(reg)	((reg >> 21) & 0x7F)

#define MMU_PB_CAPA(reg)	((reg) & 0xFF)

#define MMU_MAX_DF_CMD		8

static void *sysmmu_placeholder; /* Inidcate if a device is System MMU */

#define is_sysmmu(sysmmu) (sysmmu->archdata.iommu == &sysmmu_placeholder)
#define has_sysmmu(dev)							\
	(dev->parent && dev->archdata.iommu && is_sysmmu(dev->parent))
#define for_each_sysmmu(dev, sysmmu)					\
	for (sysmmu = dev->parent; sysmmu && is_sysmmu(sysmmu);		\
			sysmmu = sysmmu->parent)
#define for_each_sysmmu_until(dev, sysmmu, until)			\
	for (sysmmu = dev->parent; sysmmu != until; sysmmu = sysmmu->parent)

static struct kmem_cache *lv2table_kmem_cache;
static phys_addr_t fault_page;

static sysmmu_pte_t *section_entry(sysmmu_pte_t *pgtable, unsigned long iova)
{
	return (sysmmu_pte_t *)(pgtable + lv1ent_offset(iova));
}

static sysmmu_pte_t *page_entry(sysmmu_pte_t *sent, unsigned long iova)
{
	return (sysmmu_pte_t *)(__va(lv2table_base(sent))) +
				lv2ent_offset(iova);
}

enum exynos_sysmmu_inttype {
	SYSMMU_PTW_ACCESS,
	SYSMMU_PAGEFAULT,
	SYSMMU_L1TLB_MULTIHIT,
	SYSMMU_ACCESS,
	SYSMMU_SECURITY,
	SYSMMU_FAULT_UNKNOWN,
	SYSMMU_FAULTS_NUM
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PTW ACCESS FAULT",
	"PAGE FAULT",
	"L1TLB MULTI-HIT FAULT",
	"ACCESS FAULT",
	"SECURITY FAULT",
	"UNKNOWN FAULT"
};

enum sysmmu_property {
	SYSMMU_PROP_READ = 1,
	SYSMMU_PROP_WRITE = 2,
	SYSMMU_PROP_READWRITE = SYSMMU_PROP_READ | SYSMMU_PROP_WRITE,
	SYSMMU_PROP_RW_MASK = SYSMMU_PROP_READWRITE,
	SYSMMU_PROP_WINDOW_SHIFT = 16,
	SYSMMU_PROP_WINDOW_MASK = 0x1F << SYSMMU_PROP_WINDOW_SHIFT,
};

struct sysmmu_version {
	unsigned char major;
	unsigned char minor;
};

#define SYSMMU_PBUFCFG_TLB_UPDATE	(1 << 16)
#define SYSMMU_PBUFCFG_ASCENDING	(1 << 12)
#define SYSMMU_PBUFCFG_DSECENDING	(0 << 12)
#define SYSMMU_PBUFCFG_PREFETCH		(1 << 8)
#define SYSMMU_PBUFCFG_WRITE		(1 << 4)
#define SYSMMU_PBUFCFG_READ		(0 << 4)

struct sysmmu_prefbuf {
	unsigned long base;
	unsigned long size;
	unsigned long config;
};
/*
 * Metadata attached to each System MMU devices.
 */
struct sysmmu_drvdata {
	struct device *sysmmu;	/* System MMU's device descriptor */
	struct device *master;	/* Client device that needs System MMU */
	char *dbgname;
	int nsfrs;
	void __iomem **sfrbases;
	struct clk *clk;
	struct clk *clk_master;
	int activations;
	struct iommu_domain *domain; /* domain given to iommu_attach_device() */
	iommu_fault_handler_t fault_handler;
	phys_addr_t pgtable;
	struct sysmmu_version ver; /* mach/sysmmu.h */
	short qos;
	spinlock_t lock;
	struct sysmmu_prefbuf pbufs[MAX_NUM_PBUF];
	int num_pbufs;
	struct dentry *debugfs_root;
	bool runtime_active;
	enum sysmmu_property prop; /* mach/sysmmu.h */
};

struct exynos_iommu_domain {
	struct list_head clients; /* list of sysmmu_drvdata.node */
	sysmmu_pte_t *pgtable; /* lv1 page table, 16KB */
	short *lv2entcnt; /* free lv2 entry counter for each section */
	spinlock_t lock; /* lock for this structure */
	spinlock_t pgtablelock; /* lock for modifying page table @ pgtable */
};

static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}

static bool set_sysmmu_active(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU was not active previously
	   and it needs to be initialized */
	return ++data->activations == 1;
}

static bool set_sysmmu_inactive(struct sysmmu_drvdata *data)
{
	/* return true if the System MMU is needed to be disabled */
	BUG_ON(data->activations < 1);
	return --data->activations == 0;
}

static bool is_sysmmu_active(struct sysmmu_drvdata *data)
{
	return data->activations > 0;
}

unsigned int __sysmmu_version(struct sysmmu_drvdata *drvdata,
					int idx, unsigned int *minor)
{
	unsigned int major = 0;

	major = readl(drvdata->sfrbases[idx] + REG_MMU_VERSION);

	if (!(MMU_MAJ_VER(major) >= 5)) {
		pr_err("%s: sysmmu version is lower than v5.x\n", __func__);
		return major;
	}

	if (minor)
		*minor = MMU_MIN_VER(major);

	major = MMU_MAJ_VER(major);

	return major;
}

static bool has_sysmmu_capable_pbuf(struct sysmmu_drvdata *drvdata, int idx)
{
	unsigned long cfg =
		__raw_readl(drvdata->sfrbases[idx] + REG_PB_INFO);

	return MMU_PB_CAPA(cfg) ? true : false;
}

static void sysmmu_unblock(void __iomem *sfrbase)
{
	__raw_writel(CTRL_ENABLE, sfrbase + REG_MMU_CTRL);
}

static bool sysmmu_block(void __iomem *sfrbase)
{
	int i = 120;

	__raw_writel(CTRL_BLOCK, sfrbase + REG_MMU_CTRL);
	while ((i > 0) && !(__raw_readl(sfrbase + REG_MMU_STATUS) & 1))
		--i;

	if (!(__raw_readl(sfrbase + REG_MMU_STATUS) & 1)) {
		sysmmu_unblock(sfrbase);
		return false;
	}

	return true;
}

static void __sysmmu_tlb_invalidate(void __iomem *sfrbase)
{
	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH);
}

static void __sysmmu_tlb_invalidate_entry(void __iomem *sfrbase,
					  dma_addr_t iova)
{
	__raw_writel(iova | 0x1, sfrbase + REG_MMU_FLUSH_ENTRY);
}

static void __sysmmu_tlb_invalidate_range(void __iomem *sfrbase,
					  dma_addr_t iova, size_t size)
{
	__raw_writel(iova, sfrbase + REG_FLUSH_RANGE_START);
	__raw_writel(size - 1 + iova, sfrbase + REG_FLUSH_RANGE_END);
	__raw_writel(0x1, sfrbase + REG_MMU_FLUSH_RANGE);
}

static void __sysmmu_set_ptbase(void __iomem *sfrbase,
				       phys_addr_t pgtable)
{
	__raw_writel(pgtable, sfrbase + REG_PT_BASE_PPN);

	__sysmmu_tlb_invalidate(sfrbase);
}

#define SYSMMU_PBUFCFG_DEFAULT (		\
		SYSMMU_PBUFCFG_TLB_UPDATE |	\
		SYSMMU_PBUFCFG_ASCENDING |	\
		SYSMMU_PBUFCFG_PREFETCH		\
	)

static int __prepare_prefetch_buffers(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_pb)
{
	int ret_num_pb = 0;
	int i;
	struct exynos_iovmm *vmm;

	if (!drvdata->master || !drvdata->master->archdata.iommu) {
		dev_err(drvdata->sysmmu, "%s: No master device is specified\n",
					__func__);
		return 0;
	}

	vmm = exynos_get_iovmm(drvdata->master);

	if (!vmm || (drvdata->num_pbufs > 0)) {
		if (drvdata->num_pbufs > num_pb)
			drvdata->num_pbufs = num_pb;

		memcpy(prefbuf, drvdata->pbufs,
				drvdata->num_pbufs * sizeof(prefbuf[0]));

		return drvdata->num_pbufs;
	}

	if (drvdata->prop & SYSMMU_PROP_READ) {
		ret_num_pb = min(vmm->inplanes, num_pb);
		for (i = 0; i < ret_num_pb; i++) {
			prefbuf[i].base = vmm->iova_start[i];
			prefbuf[i].size = vmm->iovm_size[i];
			prefbuf[i].config =
				SYSMMU_PBUFCFG_DEFAULT | SYSMMU_PBUFCFG_READ;
		}
	}

	if ((drvdata->prop & SYSMMU_PROP_WRITE) &&
				(ret_num_pb < num_pb) && (vmm->onplanes > 0)) {
		for (i = 0; i < min(num_pb - ret_num_pb,
					vmm->inplanes + vmm->onplanes); i++) {
			prefbuf[ret_num_pb + i].base =
					vmm->iova_start[vmm->inplanes + i];
			prefbuf[ret_num_pb + i].size =
					vmm->iovm_size[vmm->inplanes + i];
			prefbuf[ret_num_pb + i].config =
				SYSMMU_PBUFCFG_DEFAULT | SYSMMU_PBUFCFG_WRITE;
		}

		ret_num_pb += i;
	}

	if (drvdata->prop & SYSMMU_PROP_WINDOW_MASK) {
		unsigned long prop = (drvdata->prop & SYSMMU_PROP_WINDOW_MASK)
						>> SYSMMU_PROP_WINDOW_SHIFT;
		BUG_ON(ret_num_pb != 0);
		for (i = 0; (i < (vmm->inplanes + vmm->onplanes)) &&
						(ret_num_pb < num_pb); i++) {
			if (prop & 1) {
				prefbuf[ret_num_pb].base = vmm->iova_start[i];
				prefbuf[ret_num_pb].size = vmm->iovm_size[i];
				prefbuf[ret_num_pb].config =
					SYSMMU_PBUFCFG_DEFAULT |
					SYSMMU_PBUFCFG_READ;
				ret_num_pb++;
			}
			prop >>= 1;
			if (prop == 0)
				break;
		}
	}

	return ret_num_pb;
}

#if 0
static void __exynos_sysmmu_set_pbuf(struct sysmmu_drvdata *drvdata,
					   int idx)
{
	int num_pb, num_bufs;
	struct sysmmu_prefbuf prefbuf[6];
	int i;
	static char lmm_preset[4][6] = {  /* [num of PB][num of buffers] */
	/*	  1,  2,  3,  4,  5,  6 */
		{ 1,  1,  0, -1, -1, -1}, /* num of pb: 3 */
		{ 3,  2,  1,  0, -1, -1}, /* num of pb: 4 */
		{-1, -1, -1, -1, -1, -1},
		{ 5,  5,  4,  2,  1,  0}, /* num of pb: 6 */
		};

	num_pb = __raw_readl(drvdata->sfrbases[idx] + REG_PB_INFO) & 0xFF;
	if ((num_pb != 3) && (num_pb != 4) && (num_pb != 6)) {
		dev_err(drvdata->master,
			"%s: Read invalid PB information from %s\n",
			__func__, drvdata->dbgname);
		return;
	}

	num_bufs = __prepare_prefetch_buffers(drvdata, prefbuf, num_pb);
	if (num_bufs == 0) {
		dev_err(drvdata->master,
			"%s: Unable to initialize PB of %s -"
			"NUM_PB %d, numbufs %d\n",
			__func__, drvdata->dbgname, num_pb, num_bufs);
		return;
	}

	if (lmm_preset[num_pb - 3][num_bufs - 1] == -1) {
		dev_err(drvdata->master,
			"%s: Unable to initialize PB of %s -"
			"NUM_PB %d, prop %d, numbuf %d\n",
			__func__, drvdata->dbgname, num_pb, drvdata->prop,
			num_bufs);
		return;
	}

	__raw_writel(lmm_preset[num_pb - 3][num_bufs - 1],
		     drvdata->sfrbases[idx] + REG_PB_LMM);

	for (i = 0; i < num_bufs; i++) {
		__raw_writel(i, drvdata->sfrbases[idx] + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbases[idx] + REG_PB_CFG);
		__raw_writel(prefbuf[i].base,
			     drvdata->sfrbases[idx] + REG_PB_START_ADDR);
		__raw_writel(prefbuf[i].size - 1 + prefbuf[i].base,
				drvdata->sfrbases[idx] + REG_PB_END_ADDR);
		__raw_writel(prefbuf[i].config | 1,
					drvdata->sfrbases[idx] + REG_PB_CFG);
	}
}
#else
#define __exynos_sysmmu_set_pbuf(drvdata, idx) do { } while (0)
#endif

void exynos_sysmmu_set_pbuf(struct device *dev, int nbufs,
				struct sysmmu_prefbuf prefbuf[])
{
	struct device *sysmmu;
	int nsfrs;

	if (WARN_ON(nbufs < 1))
		return;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);

		drvdata->num_pbufs = min(6, nbufs);

		if ((drvdata->num_pbufs == 0) || !is_sysmmu_active(drvdata) ||
			!drvdata->runtime_active) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		memcpy(drvdata->pbufs, prefbuf,
				sizeof(prefbuf[0]) * drvdata->num_pbufs);

		for (nsfrs = 0; nsfrs < drvdata->nsfrs; nsfrs++) {

			if (!has_sysmmu_capable_pbuf(drvdata, nsfrs))
				continue;

			if (sysmmu_block(drvdata->sfrbases[nsfrs])) {
				__exynos_sysmmu_set_pbuf(drvdata, nsfrs);
				sysmmu_unblock(drvdata->sfrbases[nsfrs]);
			}
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static void __sysmmu_set_df(void __iomem *sfrbase,
				dma_addr_t iova)
{
	__raw_writel(iova, sfrbase + REG_SW_DF_VPN);
}

static void __exynos_sysmmu_set_df(struct sysmmu_drvdata *drvdata,
				   int idx, dma_addr_t iova)
{
	int maj, min = 0;
	u32 cfg = 0;

	maj = __sysmmu_version(drvdata, idx, &min);
	if ((maj < 5) || ((maj == 5) && !min)) {
		dev_err(drvdata->sysmmu, "%s: %s: doesn't support SW direct fetch\n",
			drvdata->dbgname, __func__);
		return;
	}

	cfg = __raw_readl(drvdata->sfrbases[idx] + REG_SW_DF_VPN_CMD_NUM);

	if ((cfg & 0xFF) < 9)
		__sysmmu_set_df(drvdata->sfrbases[idx], iova);
	else
		pr_info("%s: DF command queue is full\n", __func__);
}

int exynos_sysmmu_set_df(struct device *dev, dma_addr_t iova)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct device *sysmmu;
	unsigned long flags;
	struct exynos_iovmm *vmm;
	int plane;

	BUG_ON(!has_sysmmu(dev));

	vmm = exynos_get_iovmm(dev);
	if (!vmm) {
		dev_err(dev, "%s: IOVMM not found\n", __func__);
		return 0;
	}

	plane = find_iovmm_plane(vmm, iova);
	if (plane < 0) {
		dev_err(dev, "%s: IOVA %#x is out of IOVMM\n", __func__, iova);
		return 0;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		int idx = 0;

		spin_lock_irqsave(&drvdata->lock, flags);

		if (drvdata->prop & SYSMMU_PROP_WINDOW_MASK) {
			unsigned long prop =
				(drvdata->prop & SYSMMU_PROP_WINDOW_MASK)
						>> SYSMMU_PROP_WINDOW_SHIFT;

			if (prop & (1 << plane)) {
				for (idx = 0; idx < drvdata->nsfrs; idx++)
					__exynos_sysmmu_set_df(
							drvdata, idx, iova);
			}
		} else {
			for (idx = 0; idx < drvdata->nsfrs; idx++)
				__exynos_sysmmu_set_df(drvdata, idx, iova);
		}

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return 0;
}

static void __sysmmu_restore_state(struct sysmmu_drvdata *drvdata)
{
	int i;

	for (i = 0; i < drvdata->nsfrs; i++) {
		if (has_sysmmu_capable_pbuf(drvdata, i)) {
			if (sysmmu_block(drvdata->sfrbases[i])) {
				__exynos_sysmmu_set_pbuf(drvdata, i);
				sysmmu_unblock(drvdata->sfrbases[i]);
			}
		}
	}
}

static void sysmmu_tlb_invalidate_entry(struct device *dev, dma_addr_t iova)
{
	struct device *sysmmu;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) &&
				drvdata->runtime_active) {
			int i;
			for (i = 0; i < drvdata->nsfrs; i++)
				__sysmmu_tlb_invalidate_entry(
						drvdata->sfrbases[i], iova);
		} else {
			dev_dbg(dev,
			"%s is disabled. Skipping TLB invalidation @ %#x\n",
			drvdata->dbgname, iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static void sysmmu_tlb_invalidate_range(struct device *dev, dma_addr_t iova,
					size_t size)
{
	struct device *sysmmu;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) &&
				drvdata->runtime_active) {
			int i;
			for (i = 0; i < drvdata->nsfrs; i++)
				__sysmmu_tlb_invalidate_range(
					drvdata->sfrbases[i], iova, size);
		} else {
			dev_dbg(dev,
			"%s is disabled. Skipping TLB invalidation @ %#x\n",
			drvdata->dbgname, iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void exynos_sysmmu_tlb_invalidate(struct device *dev, dma_addr_t start,
				  size_t size)
{
	sysmmu_tlb_invalidate_range(dev, start, size);
}

static void debug_sysmmu_l1tlb_show(void __iomem *sfrbase)
{
	int i, l1tlb_num;
	u32 vpn, ppn, attr, vvalid, pvalid;

	l1tlb_num = __raw_readl(sfrbase + REG_MMU_CAPA);

	pr_err("%s: Show evel1 TLB entries\n", __func__);
	pr_err("%10.s %s %10.s %s %10.s\n", "VPN", "V", "PPN", "V", "ATTR");
	pr_err("----------------------------------------------------\n");

	for (i = 0; i < l1tlb_num; i++) {
		__raw_writel(i, sfrbase + REG_L1TLB_READ_ENTRY);
		vpn = __raw_readl(sfrbase + REG_L1TLB_ENTRY_VPN);
		ppn = __raw_readl(sfrbase + REG_L1TLB_ENTRY_PPN);
		attr = __raw_readl(sfrbase + REG_L1TLB_ENTRY_ATTR);
		vvalid = (vpn & (1 << 28)) ? 1 : 0;
		pvalid = (ppn & (1 << 28)) ? 1 : 0;
		pr_err("%10.x %d %10.x %d %10.x\n",
			vpn & 0xFFFFF, vvalid, ppn & 0xFFFFFF, pvalid, attr);
	}
}

static void __set_fault_handler(struct sysmmu_drvdata *data,
					iommu_fault_handler_t handler)
{
	data->fault_handler = handler;
}

static int default_fault_handler(struct iommu_domain *domain,
				 struct device *dev, unsigned long fault_addr,
				 int itype, void *reserved)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	sysmmu_pte_t *ent;

	if (!((itype >= 0) && (itype < SYSMMU_FAULTS_NUM)))
			itype = SYSMMU_FAULT_UNKNOWN;

	pr_err("%s occured at 0x%lx by '%s'(Page table base: 0x%lx)\n",
		sysmmu_fault_name[itype], fault_addr, dev_name(owner->dev),
				__pa(priv->pgtable));

	ent = section_entry(priv->pgtable, fault_addr);
	pr_err("\tLv1 entry: 0x%x\n", *ent);

	if (lv1ent_page(ent)) {
		ent = page_entry(ent, fault_addr);
		pr_err("\t Lv2 entry: 0x%x\n", *ent);
	}

	pr_err("Generating Kernel OOPS... because it is unrecoverable.\n");

	BUG();

	return 0;
}

static irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *drvdata = dev_id;
	struct exynos_iommu_owner *owner = NULL;
	unsigned int itype;
	unsigned long addr = -1;
	const char *mmuname = NULL;
	int i, ret = -ENOSYS;
	int awfault = 0;

	if (drvdata->master)
		owner = drvdata->master->archdata.iommu;

	if (owner)
		spin_lock(&owner->lock);

	WARN_ON(!is_sysmmu_active(drvdata));

	for (i = 0; i < drvdata->nsfrs; i++) {
		struct resource *irqres;
		irqres = platform_get_resource(
				to_platform_device(drvdata->sysmmu),
				IORESOURCE_IRQ, i);
		if (irqres && ((int)irqres->start == irq)) {
			mmuname = irqres->name;
			break;
		}
	}

	if (i == drvdata->nsfrs) {
		itype = SYSMMU_FAULT_UNKNOWN;
	} else {
		itype =  __ffs(__raw_readl(
				drvdata->sfrbases[i] + REG_INT_STATUS));
		if (itype > 15) {
			awfault = 1;
			itype -= 16;
		}

		if (WARN_ON(!(itype < SYSMMU_FAULT_UNKNOWN)))
			itype = SYSMMU_FAULT_UNKNOWN;
		else
			addr = __raw_readl(drvdata->sfrbases[i] + (awfault ?
					REG_FAULT_AW_ADDR : REG_FAULT_AR_ADDR));
	}

	if (drvdata->domain) /* owner is always set if drvdata->domain exists */
		ret = report_iommu_fault(drvdata->domain,
					owner->dev, addr, itype + 16 * awfault);

	if (ret == -ENOSYS) {
		if (drvdata->fault_handler)
			ret = drvdata->fault_handler(drvdata->domain,
				owner->dev, addr, itype + 16 * awfault, NULL);
	}

	if (!ret && (itype != SYSMMU_FAULT_UNKNOWN)) {
		if (itype == SYSMMU_L1TLB_MULTIHIT)
			debug_sysmmu_l1tlb_show(drvdata->sfrbases[i]);
		__raw_writel(1 << itype, drvdata->sfrbases[i] + REG_INT_CLEAR);
	} else {
		dev_dbg(owner ? owner->dev : drvdata->sysmmu,
				"%s is not handled by %s\n",
				sysmmu_fault_name[itype], drvdata->dbgname);
	}

	sysmmu_unblock(drvdata->sfrbases[i]);

	if (owner)
		spin_unlock(&owner->lock);

	return IRQ_HANDLED;
}

static void __sysmmu_disable_nocount(struct sysmmu_drvdata *drvdata)
{
	int i;

	for (i = 0; i < drvdata->nsfrs; i++) {
		__raw_writel(0, drvdata->sfrbases[i] + REG_MMU_CFG);
		__raw_writel(CTRL_DISABLE,
				drvdata->sfrbases[i] + REG_MMU_CTRL);
	}

	clk_disable(drvdata->clk);
}

static bool __sysmmu_disable(struct sysmmu_drvdata *drvdata)
{
	bool disabled;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->lock, flags);

	disabled = set_sysmmu_inactive(drvdata);

	if (disabled) {
		drvdata->pgtable = 0;
		drvdata->domain = NULL;

		if (drvdata->runtime_active)
			__sysmmu_disable_nocount(drvdata);

		dev_dbg(drvdata->sysmmu, "Disabled %s\n", drvdata->dbgname);
	} else  {
		dev_dbg(drvdata->sysmmu, "%d times left to disable %s\n",
					drvdata->activations, drvdata->dbgname);
	}

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return disabled;
}

static void __sysmmu_init_config(struct sysmmu_drvdata *drvdata, int idx)
{
	unsigned long cfg = 0;

	cfg |= CFG_FLPDCACHE | CFG_ACGEN;
	if (!(drvdata->qos < 0))
		cfg |= CFG_QOS_OVRRIDE | CFG_QOS(drvdata->qos);

	if (has_sysmmu_capable_pbuf(drvdata, idx))
		__exynos_sysmmu_set_pbuf(drvdata, idx);

	cfg |= __raw_readl(drvdata->sfrbases[idx] + REG_MMU_CFG) & ~CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbases[idx] + REG_MMU_CFG);
}

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *drvdata)
{
	int i;

	clk_enable(drvdata->clk);

	for (i = 0; i < drvdata->nsfrs; i++) {
		if (soc_is_exynos5430())
			__raw_writel(0, drvdata->sfrbases[i] + REG_MMU_CTRL);

		BUG_ON(__raw_readl(drvdata->sfrbases[i] + REG_MMU_CTRL)
								& CTRL_ENABLE);

		__sysmmu_init_config(drvdata, i);

		__sysmmu_set_ptbase(drvdata->sfrbases[i],
				__phys_to_pfn(drvdata->pgtable));

		__raw_writel(CTRL_ENABLE, drvdata->sfrbases[i] + REG_MMU_CTRL);
	}
}

static int __sysmmu_enable(struct sysmmu_drvdata *drvdata,
			phys_addr_t pgtable, struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&drvdata->lock, flags);
	if (set_sysmmu_active(drvdata)) {
		drvdata->pgtable = pgtable;
		drvdata->domain = domain;

		if (drvdata->runtime_active)
			__sysmmu_enable_nocount(drvdata);

		dev_dbg(drvdata->sysmmu, "Enabled %s\n", drvdata->dbgname);
	} else {
		ret = (pgtable == drvdata->pgtable) ? 1 : -EBUSY;

		dev_dbg(drvdata->sysmmu, "%s is already enabled\n",
							drvdata->dbgname);
	}

	if (WARN_ON(ret < 0))
		set_sysmmu_inactive(drvdata); /* decrement count */

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return ret;
}

/* __exynos_sysmmu_enable: Enables System MMU
 *
 * returns -error if an error occurred and System MMU is not enabled,
 * 0 if the System MMU has been just enabled and 1 if System MMU was already
 * enabled before.
 */
static int __exynos_sysmmu_enable(struct device *dev, phys_addr_t pgtable,
				struct iommu_domain *domain)
{
	int ret = 0;
	unsigned long flags;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct device *sysmmu;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		drvdata->master = dev;
		ret = __sysmmu_enable(drvdata, pgtable, domain);
		if (ret < 0) {
			struct device *iter;
			for_each_sysmmu_until(dev, iter, sysmmu) {
				drvdata = dev_get_drvdata(iter);
				__sysmmu_disable(drvdata);
				drvdata->master = NULL;
			}
		}
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return ret;
}

int exynos_sysmmu_enable(struct device *dev, unsigned long pgtable)
{
	int ret;

	BUG_ON(!memblock_is_memory(pgtable));

	ret = __exynos_sysmmu_enable(dev, pgtable, NULL);

	return ret;
}

bool exynos_sysmmu_disable(struct device *dev)
{
	unsigned long flags;
	bool disabled = true;
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct device *sysmmu;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	/* Every call to __sysmmu_disable() must return same result */
	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		disabled = __sysmmu_disable(drvdata);
		if (disabled)
			drvdata->master = NULL;
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return disabled;
}

#ifdef CONFIG_EXYNOS_IOMMU_RECOVER_FAULT_HANDLER
int recover_fault_handler (struct iommu_domain *domain,
				struct device *dev, unsigned long fault_addr,
				int itype, void *reserved)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	unsigned long flags;

	itype %= 16;

	if (itype == SYSMMU_PAGEFAULT) {
		struct exynos_iovmm *vmm_data;
		sysmmu_pte_t *sent;
		sysmmu_pte_t *pent;

		BUG_ON(priv->pgtable == NULL);

		spin_lock_irqsave(&priv->pgtablelock, flags);

		sent = section_entry(priv->pgtable, fault_addr);
		if (!lv1ent_page(sent)) {
			pent = kmem_cache_zalloc(lv2table_kmem_cache,
						 GFP_ATOMIC);
			if (!pent)
				return -ENOMEM;

			*sent = mk_lv1ent_page(__pa(pent));
			pgtable_flush(sent, sent + 1);
		}
		pent = page_entry(sent, fault_addr);
		if (lv2ent_fault(pent)) {
			*pent = mk_lv2ent_spage(fault_page);
			pgtable_flush(pent, pent + 1);
		} else {
			pr_err("[%s] 0x%lx by '%s' is already mapped\n",
				sysmmu_fault_name[itype], fault_addr,
				dev_name(dev));
		}

		spin_unlock_irqrestore(&priv->pgtablelock, flags);

		owner = dev->archdata.iommu;
		vmm_data = (struct exynos_iovmm *)owner->vmm_data;
		if (find_iovm_region(vmm_data, fault_addr)) {
			pr_err("[%s] 0x%lx by '%s' is remapped\n",
				sysmmu_fault_name[itype],
				fault_addr, dev_name(dev));
		} else {
			pr_err("[%s] '%s' accessed unmapped address(0x%lx)\n",
				sysmmu_fault_name[itype], dev_name(dev),
				fault_addr);
		}
	} else if (itype == SYSMMU_L1TLB_MULTIHIT) {
		spin_lock_irqsave(&priv->lock, flags);
		list_for_each_entry(owner, &priv->clients, client)
			sysmmu_tlb_invalidate_entry(owner->dev,
						    (dma_addr_t)fault_addr);
		spin_unlock_irqrestore(&priv->lock, flags);

		pr_err("[%s] occured at 0x%lx by '%s'\n",
			sysmmu_fault_name[itype], fault_addr, dev_name(dev));
	} else {
		return -ENOSYS;
	}

	return 0;
}
#else
int recover_fault_handler (struct iommu_domain *domain,
				struct device *dev, unsigned long fault_addr,
				int itype, void *reserved)
{
	return -ENOSYS;
}
#endif

static int __init __sysmmu_init_clock(struct device *sysmmu,
					struct sysmmu_drvdata *drvdata)
{
	int ret;

	drvdata->clk = devm_clk_get(sysmmu, "sysmmu");
	if (IS_ERR(drvdata->clk)) {
		dev_dbg(sysmmu, "No gating clock found.\n");
		drvdata->clk = NULL;
		return 0;
	}

	ret = clk_prepare(drvdata->clk);
	if (ret) {
		dev_dbg(sysmmu, "clk_prepare() failed\n");
		return ret;
	}

	drvdata->clk_master = devm_clk_get(sysmmu, "master");
	if (PTR_ERR(drvdata->clk_master) == -ENOENT) {
		drvdata->clk_master = NULL;
		return 0;
	} else if (IS_ERR(drvdata->clk_master)) {
		dev_dbg(sysmmu, "No master clock found\n");
		clk_unprepare(drvdata->clk);
		return PTR_ERR(drvdata->clk_master);
	}

	dev_dbg(sysmmu, "sysmmu clk = %s, master clk = %s\n",
		__clk_get_name(drvdata->clk),
		__clk_get_name(drvdata->clk_master));

	return 0;
}

#define has_more_master(dev) ((unsigned long)dev->archdata.iommu & 1)
#define master_initialized(dev) (!((unsigned long)dev->archdata.iommu & 1) \
				&& ((unsigned long)dev->archdata.iommu & ~1))

static struct device * __init __sysmmu_init_master(
				struct device *sysmmu, struct device *dev) {
	struct exynos_iommu_owner *owner;
	struct device *master = (struct device *)((unsigned long)dev & ~1);
	int ret;

	if (!master)
		return NULL;

	/*
	 * has_more_master() call to the main master device returns false while
	 * the same call to the other master devices (shared master devices)
	 * return true.
	 * Shared master devices are moved after 'sysmmu' in the DPM list while
	 * 'sysmmu' is moved before the master device not to break the order of
	 * suspend/resume.
	 */
	if (has_more_master(master)) {
		void *pret;
		pret = __sysmmu_init_master(sysmmu, master->archdata.iommu);
		if (IS_ERR(pret))
			return pret;

		ret = device_move(master, sysmmu, DPM_ORDER_DEV_AFTER_PARENT);
		if (ret)
			return ERR_PTR(ret);
	} else {
		struct device *child = master;
		/* Finding the topmost System MMU in the hierarchy of master. */
		while (child && child->parent && is_sysmmu(child->parent))
			child = child->parent;

		ret = device_move(child, sysmmu, DPM_ORDER_PARENT_BEFORE_DEV);
		if (ret)
			return ERR_PTR(ret);

		if (master_initialized(master)) {
			dev_dbg(sysmmu,
				"Assigned initialized master device %s.\n",
							dev_name(master));
			return master;
		}
	}

	/*
	 * There must not be a master device which is initialized and
	 * has a link to another master device.
	 */
	BUG_ON(master_initialized(master));

	owner = devm_kzalloc(sysmmu, sizeof(*owner), GFP_KERNEL);
	if (!owner) {
		dev_err(sysmmu, "Failed to allcoate iommu data.\n");
		return ERR_PTR(-ENOMEM);
	}

	INIT_LIST_HEAD(&owner->client);
	owner->dev = master;
	spin_lock_init(&owner->lock);

	master->archdata.iommu = owner;

	dev_dbg(sysmmu, "Assigned master device %s.\n", dev_name(master));

	return master;
}

static int __init __sysmmu_setup(struct device *sysmmu,
				struct sysmmu_drvdata *drvdata)
{
	struct device *master;
	int ret;

	master = __sysmmu_init_master(sysmmu, sysmmu->archdata.iommu);
	if (!master) {
		dev_dbg(sysmmu, "No master device is assigned\n");
	} else if (IS_ERR(master)) {
		dev_err(sysmmu, "Failed to initialize master device.\n");
		return PTR_ERR(master);
	}

	ret = __sysmmu_init_clock(sysmmu, drvdata);
	if (ret)
		dev_err(sysmmu, "Failed to initialize gating clocks\n");

	return ret;
}

static int __init exynos_sysmmu_probe(struct platform_device *pdev)
{
	int i, ret;
	struct device *dev = &pdev->dev;
	struct sysmmu_drvdata *data;

	data = devm_kzalloc(dev,
			sizeof(*data) + sizeof(*data->sfrbases) *
				(pdev->num_resources / 2),
			GFP_KERNEL);
	if (!data) {
		dev_err(dev, "Not enough memory\n");
		return -ENOMEM;
	}

	data->nsfrs = pdev->num_resources / 2;
	data->sfrbases = (void __iomem **)(data + 1);

	for (i = 0; i < data->nsfrs; i++) {
		struct resource *res;
		res = platform_get_resource(pdev, IORESOURCE_MEM, i);
		if (!res) {
			dev_err(dev, "Unable to find IOMEM region\n");
			return -ENOENT;
		}

		data->sfrbases[i] = devm_request_and_ioremap(dev, res);
		if (!data->sfrbases[i]) {
			dev_err(dev, "Unable to map IOMEM @ PA:%#x\n",
							res->start);
			return -EBUSY;
		}
	}

	for (i = 0; i < data->nsfrs; i++) {
		ret = platform_get_irq(pdev, i);
		if (ret <= 0) {
			dev_err(dev, "Unable to find IRQ resource\n");
			return ret;
		}

		ret = devm_request_irq(dev, ret, exynos_sysmmu_irq, 0,
					dev_name(dev), data);
		if (ret) {
			dev_err(dev, "Unabled to register interrupt handler\n");
			return ret;
		}
	}

	pm_runtime_enable(dev);

	ret = __sysmmu_setup(dev, data);
	if (!ret) {
		data->runtime_active = !pm_runtime_enabled(dev);
		data->sysmmu = dev;
		spin_lock_init(&data->lock);

		__set_fault_handler(data, &default_fault_handler);

		platform_set_drvdata(pdev, data);

		dev->archdata.iommu = &sysmmu_placeholder;
		dev_dbg(dev, "Initialized!\n");
	}

	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int sysmmu_suspend(struct device *dev)
{
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);
	unsigned long flags;
	spin_lock_irqsave(&drvdata->lock, flags);
	if (is_sysmmu_active(drvdata) &&
		(!pm_runtime_enabled(dev) || drvdata->runtime_active))
		__sysmmu_disable_nocount(drvdata);
	spin_unlock_irqrestore(&drvdata->lock, flags);
	return 0;
}

static int sysmmu_resume(struct device *dev)
{
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);
	unsigned long flags;
	spin_lock_irqsave(&drvdata->lock, flags);
	if (is_sysmmu_active(drvdata) &&
		(!pm_runtime_enabled(dev) || drvdata->runtime_active)) {
		__sysmmu_enable_nocount(drvdata);
		__sysmmu_restore_state(drvdata);
	}
	spin_unlock_irqrestore(&drvdata->lock, flags);
	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int sysmmu_runtime_suspend(struct device *dev)
{
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);
	unsigned long flags;
	spin_lock_irqsave(&drvdata->lock, flags);
	if (is_sysmmu_active(drvdata))
		__sysmmu_disable_nocount(drvdata);
	drvdata->runtime_active = false;
	spin_unlock_irqrestore(&drvdata->lock, flags);
	return 0;
}

static int sysmmu_runtime_resume(struct device *dev)
{
	struct sysmmu_drvdata *drvdata = dev_get_drvdata(dev);
	unsigned long flags;
	spin_lock_irqsave(&drvdata->lock, flags);
	drvdata->runtime_active = true;
	if (is_sysmmu_active(drvdata))
		__sysmmu_enable_nocount(drvdata);
	spin_unlock_irqrestore(&drvdata->lock, flags);
	return 0;
}
#endif

static const struct dev_pm_ops __pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sysmmu_suspend, sysmmu_resume)
	SET_RUNTIME_PM_OPS(sysmmu_runtime_suspend, sysmmu_runtime_resume, NULL)
};

#ifdef CONFIG_OF
static struct of_device_id sysmmu_of_match[] __initconst = {
	{ .compatible = "samsung,exynos5430-sysmmu", },
	{ },
};
#endif

static struct platform_driver exynos_sysmmu_driver __refdata = {
	.probe		= exynos_sysmmu_probe,
	.driver		= {
		.owner		= THIS_MODULE,
		.name		= MODULE_NAME,
		.pm		= &__pm_ops,
		.of_match_table = of_match_ptr(sysmmu_of_match),
	}
};

static int exynos_iommu_domain_init(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv;

	priv = kzalloc(sizeof(*priv), GFP_KERNEL);
	if (!priv)
		return -ENOMEM;

	priv->pgtable = (sysmmu_pte_t *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 2);
	if (!priv->pgtable)
		goto err_pgtable;

	priv->lv2entcnt = (short *)__get_free_pages(
						GFP_KERNEL | __GFP_ZERO, 1);
	if (!priv->lv2entcnt)
		goto err_counter;

	pgtable_flush(priv->pgtable, priv->pgtable + NUM_LV1ENTRIES);

	spin_lock_init(&priv->lock);
	spin_lock_init(&priv->pgtablelock);
	INIT_LIST_HEAD(&priv->clients);

	domain->priv = priv;
	domain->handler = recover_fault_handler;
	return 0;

err_counter:
	free_pages((unsigned long)priv->pgtable, 2);
err_pgtable:
	kfree(priv);
	return -ENOMEM;
}

static void exynos_iommu_domain_destroy(struct iommu_domain *domain)
{
	struct exynos_iommu_domain *priv = domain->priv;
	struct exynos_iommu_owner *owner;
	unsigned long flags;
	int i;

	WARN_ON(!list_empty(&priv->clients));

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client) {
		while (!exynos_sysmmu_disable(owner->dev))
			; /* until System MMU is actually disabled */
		list_del_init(&owner->client);
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	for (i = 0; i < NUM_LV1ENTRIES; i++)
		if (lv1ent_page(priv->pgtable + i))
			kmem_cache_free(lv2table_kmem_cache,
					__va(lv2table_base(priv->pgtable + i)));

	free_pages((unsigned long)priv->pgtable, 2);
	free_pages((unsigned long)priv->lv2entcnt, 1);
	kfree(domain->priv);
	domain->priv = NULL;
}

static int exynos_iommu_attach_device(struct iommu_domain *domain,
				   struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long flags;
	int ret;

	spin_lock_irqsave(&priv->lock, flags);

	ret = __exynos_sysmmu_enable(dev, __pa(priv->pgtable), domain);

	if (ret == 0)
		list_add_tail(&owner->client, &priv->clients);

	spin_unlock_irqrestore(&priv->lock, flags);

	if (ret < 0)
		dev_err(dev, "%s: Failed to attach IOMMU with pgtable %#lx\n",
				__func__, __pa(priv->pgtable));
	else
		dev_dbg(dev, "%s: Attached new IOMMU with pgtable 0x%lx%s\n",
					__func__, __pa(priv->pgtable),
					(ret == 0) ? "" : ", again");

	return ret;
}

static void exynos_iommu_detach_device(struct iommu_domain *domain,
				    struct device *dev)
{
	struct exynos_iommu_owner *owner;
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long flags;

	spin_lock_irqsave(&priv->lock, flags);

	list_for_each_entry(owner, &priv->clients, client) {
		if (owner == dev->archdata.iommu) {
			if (exynos_sysmmu_disable(dev))
				list_del_init(&owner->client);
			break;
		}
	}

	spin_unlock_irqrestore(&priv->lock, flags);

	if (owner == dev->archdata.iommu)
		dev_dbg(dev, "%s: Detached IOMMU with pgtable %#lx\n",
					__func__, __pa(priv->pgtable));
	else
		dev_dbg(dev, "%s: No IOMMU is attached\n", __func__);
}

static sysmmu_pte_t *alloc_lv2entry(sysmmu_pte_t *sent, unsigned long iova,
					short *pgcounter)
{
	if (lv1ent_fault(sent)) {
		sysmmu_pte_t *pent;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(__pa(pent));
		kmemleak_ignore(pent);
		*pgcounter = NUM_LV2ENTRIES;
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		pgtable_flush(sent, sent + 1);
	} else if (!lv1ent_page(sent)) {
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static int lv1ent_check_page(sysmmu_pte_t *sent, short *pgcnt)
{
	if (lv1ent_page(sent)) {
		if (*pgcnt != NUM_LV2ENTRIES)
			return -EADDRINUSE;

		kmem_cache_free(lv2table_kmem_cache, page_entry(sent, 0));

		*pgcnt = 0;
	}

	return 0;
}

static void clear_page_table(sysmmu_pte_t *ent, int n)
{
	if (n > 0)
		memset(ent, 0, sizeof(*ent) * n);
}

static int lv1set_section(sysmmu_pte_t *sent, phys_addr_t paddr,
			  size_t size,  short *pgcnt)
{
	int ret;

	if (!lv1ent_fault(sent) && !lv1ent_page(sent))
		return -EADDRINUSE;

	if (size == SECT_SIZE) {
		ret = lv1ent_check_page(sent, pgcnt);
		if (ret)
			return ret;
		*sent = mk_lv1ent_sect(paddr);
		pgtable_flush(sent, sent + 1);
	} else if (size == LSECT_SIZE) {
		int i;
		for (i = 0; i < SECT_PER_LSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(sent, pgcnt);
			if (ret) {
				clear_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_lsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_LSECT, sent);
	} else {
		int i;
		for (i = 0; i < SECT_PER_SPSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(sent, pgcnt);
			if (ret) {
				clear_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_spsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_SPSECT, sent);
	}

	return 0;
}

static int lv2set_page(sysmmu_pte_t *pent, phys_addr_t paddr,
		       size_t size, short *pgcnt)
{
	if (size == SPAGE_SIZE) {
		if (!lv2ent_fault(pent))
			return -EADDRINUSE;

		*pent = mk_lv2ent_spage(paddr);
		pgtable_flush(pent, pent + 1);
		*pgcnt -= 1;
	} else { /* size == LPAGE_SIZE */
		int i;
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (!lv2ent_fault(pent)) {
				clear_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		*pgcnt -= SPAGES_PER_LPAGE;
	}

	return 0;
}

void lv2_dummy_map(struct iommu_domain *domain, unsigned long iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	sysmmu_pte_t *entry;
	sysmmu_pte_t *pent;
	unsigned long flags;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);
	entry = section_entry(priv->pgtable, iova);

	pent = alloc_lv2entry(entry, iova,
			&priv->lv2entcnt[lv1ent_offset(iova)]);
	if (IS_ERR(pent)) {
		pr_info("%s: Failed(%ld) to map @ %#lx\n",
				__func__, PTR_ERR(pent), iova);
	}
	spin_unlock_irqrestore(&priv->pgtablelock, flags);
}

static int exynos_iommu_map(struct iommu_domain *domain, unsigned long iova,
			 phys_addr_t paddr, size_t size, int prot)
{
	struct exynos_iommu_domain *priv = domain->priv;
	sysmmu_pte_t *entry;
	unsigned long flags;
	int ret = -ENOMEM;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (size >= SECT_SIZE) {
		ret = lv1set_section(entry, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		sysmmu_pte_t *pent;

		pent = alloc_lv2entry(entry, iova,
					&priv->lv2entcnt[lv1ent_offset(iova)]);

		if (IS_ERR(pent)) {
			ret = PTR_ERR(pent);
		} else {
			ret = lv2set_page(pent, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
		}
	}

	if (ret)
		pr_err("%s: Failed(%d) to map %#x bytes @ %#lx\n",
			__func__, ret, size, iova);

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return ret;
}

static size_t exynos_iommu_unmap(struct iommu_domain *domain,
					unsigned long iova, size_t size)
{
	struct exynos_iommu_domain *priv = domain->priv;
	size_t err_pgsize;
	sysmmu_pte_t *ent;
	unsigned long flags;

	BUG_ON(priv->pgtable == NULL);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	ent = section_entry(priv->pgtable, iova);

	if (lv1ent_spsection(ent)) {
		if (WARN_ON(size < SPSECT_SIZE)) {
			err_pgsize = SPSECT_SIZE;
			goto err;
		}

		clear_page_table(ent, SECT_PER_SPSECT);

		pgtable_flush(ent, ent + SECT_PER_SPSECT);
		size = SPSECT_SIZE;
		goto done;
	}

	if (lv1ent_lsection(ent)) {
		if (WARN_ON(size < LSECT_SIZE)) {
			err_pgsize = LSECT_SIZE;
			goto err;
		}

		*ent = 0;
		*(++ent) = 0;
		pgtable_flush(ent, ent + 2);
		size = LSECT_SIZE;
		goto done;
	}

	if (lv1ent_section(ent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*ent = 0;
		pgtable_flush(ent, ent + 1);
		size = SECT_SIZE;
		goto done;
	}

	if (unlikely(lv1ent_fault(ent))) {
		if (size > SECT_SIZE)
			size = SECT_SIZE;
		goto done;
	}

	/* lv1ent_page(sent) == true here */

	ent = page_entry(ent, iova);

	if (unlikely(lv2ent_fault(ent))) {
		size = SPAGE_SIZE;
		goto done;
	}

	if (lv2ent_small(ent)) {
		*ent = 0;
		size = SPAGE_SIZE;
		pgtable_flush(ent, ent + 1);
		priv->lv2entcnt[lv1ent_offset(iova)] += 1;
		goto done;
	}

	/* lv1ent_large(ent) == true here */
	if (WARN_ON(size < LPAGE_SIZE)) {
		err_pgsize = LPAGE_SIZE;
		goto err;
	}

	clear_page_table(ent, SPAGES_PER_LPAGE);
	pgtable_flush(ent, ent + SPAGES_PER_LPAGE);

	size = LPAGE_SIZE;
	priv->lv2entcnt[lv1ent_offset(iova)] += SPAGES_PER_LPAGE;
done:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	/* TLB invalidation is performed by IOVMM */
	return size;
err:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	pr_err("%s: Failed: size(%#lx) @ %#x is smaller than page size %#x\n",
		__func__, iova, size, err_pgsize);

	return 0;
}

static phys_addr_t exynos_iommu_iova_to_phys(struct iommu_domain *domain,
					     dma_addr_t iova)
{
	struct exynos_iommu_domain *priv = domain->priv;
	unsigned long flags;
	sysmmu_pte_t *entry;
	phys_addr_t phys = 0;

	spin_lock_irqsave(&priv->pgtablelock, flags);

	entry = section_entry(priv->pgtable, iova);

	if (lv1ent_spsection(entry)) {
		phys = spsection_phys(entry) + spsection_offs(iova);
	} else if (lv1ent_lsection(entry)) {
		phys = lsection_phys(entry) + lsection_offs(iova);
	} else if (lv1ent_section(entry)) {
		phys = section_phys(entry) + section_offs(iova);
	} else if (lv1ent_page(entry)) {
		entry = page_entry(entry, iova);

		if (lv2ent_large(entry))
			phys = lpage_phys(entry) + lpage_offs(iova);
		else if (lv2ent_small(entry))
			phys = spage_phys(entry) + spage_offs(iova);
	}

	spin_unlock_irqrestore(&priv->pgtablelock, flags);

	return phys;
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.pgsize_bitmap = LSECT_SIZE | SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE,
};

/* exynos_set_sysmmu - link a System MMU with its master device
 *
 * This function links System MMU with its master device. Since a System MMU
 * is dedicated to its master device by H/W design, it is important to inform
 * their relationship to System MMU device driver. This function informs System
 * MMU driver what is the master device of the probing System MMU.
 * This information is used by the System MMU (exynos-iommu) to make their
 * relationship in the heirarch of kobjs of registered devices.
 * The link created here:
 * - Before call: NULL <- @sysmmu
 * - After call : @dev <- @sysmmu
 *
 * If a master is already assigned to @sysmmu and @sysmmu->archdata.iommu & 1
 * is 1, the link is created as follows:
 *  - Before call: existing_master <- @sysmmu <- existing_master
 *  - After call : existing_master <- @dev <- @sysmmu
 */

static int __init exynos_set_sysmmu(struct device *dev, void *unused)
{
	int i;
	size_t size;
	const __be32 *phandle;

	phandle = of_get_property(dev->of_node, "iommu", &size);
	if (!phandle)
		return 0;

	size = size / sizeof(*phandle);

	for (i = 0; i < size; i++) {
		struct device_node *np;
		struct platform_device *sysmmu;
		struct device *sdev;

		/* this always success: see above of_find_property() */
		np = of_parse_phandle(dev->of_node, "iommu", i);

		sysmmu = of_find_device_by_node(np);
		if (!sysmmu) {
			dev_err(dev, "sysmmu node '%s' is not found\n",
				np->name);
		}

		sdev = &sysmmu->dev;

		if ((unsigned long)sdev->archdata.iommu & 1)
			dev->archdata.iommu = sdev->archdata.iommu;

		sdev->archdata.iommu = (void *)((unsigned long)dev | 1);
	}

	return 0;
}

static int __init exynos_iommu_init(void)
{
	int ret;
	struct page *page;

	ret = bus_for_each_dev(&platform_bus_type, NULL, NULL,
				   exynos_set_sysmmu);
	if (ret)
		return ret;

	lv2table_kmem_cache = kmem_cache_create("exynos-iommu-lv2table",
		LV2TABLE_SIZE, LV2TABLE_SIZE, 0, NULL);
	if (!lv2table_kmem_cache) {
		pr_err("%s: failed to create kmem cache\n", __func__);
		return -ENOMEM;
	}

	page = alloc_page(GFP_KERNEL | __GFP_ZERO);
	if (!page) {
		pr_err("%s: failed to allocate fault page\n", __func__);
		kmem_cache_destroy(lv2table_kmem_cache);
		return -ENOMEM;
	}
	fault_page = page_to_phys(page);

	ret = bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);
	if (ret) {
		__free_page(page);
		kmem_cache_destroy(lv2table_kmem_cache);
		pr_err("%s: Failed to register IOMMU ops\n", __func__);
	}

	return platform_driver_register(&exynos_sysmmu_driver);
}
subsys_initcall(exynos_iommu_init);
