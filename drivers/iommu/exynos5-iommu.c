/* linux/drivers/iommu/exynos_iommu.c
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

#define SPSECT_MASK (~(SPSECT_SIZE - 1))
#define SECT_MASK (~(SECT_SIZE - 1))
#define LPAGE_MASK (~(LPAGE_SIZE - 1))
#define SPAGE_MASK (~(SPAGE_SIZE - 1))

#define lv1ent_fault(sent) ((*(sent) == ZERO_LV2LINK) || \
			   ((*(sent) & 3) == 0) || ((*(sent) & 3) == 3))
#define lv1ent_page(sent) ((*(sent) != ZERO_LV2LINK) && \
			  ((*(sent) & 3) == 1))
#define lv1ent_section(sent) ((*(sent) & 3) == 2)
#define lv1ent_spsection(sent) (((*(sent) & 3) == 2) && \
			       ((*(sent) & (0x1 << 18)) == 1))

#define lv2ent_fault(pent) ((*(pent) & 3) == 0 || \
			   ((*(pent) & SPAGE_MASK) == fault_page))
#define lv2ent_small(pent) ((*(pent) & 2) == 2)
#define lv2ent_large(pent) ((*(pent) & 3) == 1)

#define spsection_phys(sent) (*(sent) & SPSECT_MASK)
#define spsection_offs(iova) ((iova) & (SPSECT_SIZE - 1))
#define section_phys(sent) (*(sent) & SECT_MASK)
#define section_offs(iova) ((iova) & (SECT_SIZE - 1))
#define lpage_phys(pent) (*(pent) & LPAGE_MASK)
#define lpage_offs(iova) ((iova) & (LPAGE_SIZE - 1))
#define spage_phys(pent) (*(pent) & SPAGE_MASK)
#define spage_offs(iova) ((iova) & (SPAGE_SIZE - 1))

#define lv1ent_offset(iova) ((iova) >> SECT_ORDER)
#define lv2ent_offset(iova) (((iova) & 0xFF000) >> SPAGE_ORDER)

#define LV2TABLE_SIZE (NUM_LV2ENTRIES * 4) /* 32bit page table entry */

#define lv2table_base(sent) (*(sent) & 0xFFFFFC00)

#define mk_lv1ent_spsect(pa) ((sysmmu_pte_t) ((pa) | 0x40002))
#define mk_lv1ent_sect(pa) ((sysmmu_pte_t) ((pa) | 2))
#define mk_lv1ent_page(pa) ((sysmmu_pte_t) ((pa) | 1))
#define mk_lv2ent_lpage(pa) ((sysmmu_pte_t) ((pa) | 1))
#define mk_lv2ent_spage(pa) ((sysmmu_pte_t) ((pa) | 2))

#define CTRL_ENABLE	0x5
#define CTRL_BLOCK	0x7
#define CTRL_DISABLE	0x0

#define CFG_LRU		0x1
#define CFG_QOS(n)	((n & 0xF) << 7)
#define CFG_MASK	0x0150FFFF /* Selecting bit 0-15, 20, 22 and 24 */
#define CFG_ACGEN	(1 << 24) /* System MMU 3.3 only */
#define CFG_SYSSEL	(1 << 22) /* System MMU 3.2 only */
#define CFG_FLPDCACHE	(1 << 20) /* System MMU 3.2+ only */
#define CFG_SHAREABLE	(1 << 12) /* System MMU 3.x only */
#define CFG_QOS_OVRRIDE (1 << 11) /* System MMU 3.3 only */

#define PB_INFO_NUM(reg)	((reg) & 0xFF) /* System MMU 3.3 only */

#define REG_MMU_CTRL		0x000
#define REG_MMU_CFG		0x004
#define REG_MMU_STATUS		0x008
#define REG_MMU_FLUSH		0x00C
#define REG_MMU_FLUSH_ENTRY	0x010
#define REG_PT_BASE_ADDR	0x014
#define REG_INT_STATUS		0x018
#define REG_INT_CLEAR		0x01C
#define REG_PB_INFO		0x400
#define REG_PB_LMM		0x404
#define REG_PB_INDICATE		0x408
#define REG_PB_CFG		0x40C
#define REG_PB_START_ADDR	0x410
#define REG_PB_END_ADDR		0x414
#define REG_SPB_BASE_VPN	0x418

#define REG_PAGE_FAULT_ADDR	0x024
#define REG_AW_FAULT_ADDR	0x028
#define REG_AR_FAULT_ADDR	0x02C
#define REG_DEFAULT_SLAVE_ADDR	0x030
#define REG_FAULT_TRANS_INFO	0x04C
#define REG_L1TLB_READ_ENTRY	0x040
#define REG_L1TLB_ENTRY_PPN	0x044
#define REG_L1TLB_ENTRY_VPN	0x048

#define REG_MMU_VERSION		0x034

#define MAX_NUM_PBUF		6
#define SINGLE_PB_SIZE		16

#define NUM_MINOR_OF_SYSMMU_V3	4

#define MMU_MAJ_VER(val)	((val) >> 7)
#define MMU_MIN_VER(val)	((val) & 0x7F)
#define MMU_RAW_VER(reg)	(((reg) >> 21) & ((1 << 11) - 1)) /* 11 bits */

#define MAKE_MMU_VER(maj, min)  ((((maj) & 0xF) << 7) | ((min) & 0x7F))

#define MMU_TLB_ENT_NUM(val)	((val) & 0x7F)

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
static unsigned long *zero_lv2_table;
#define ZERO_LV2LINK mk_lv1ent_page(__pa(zero_lv2_table))

#define __sysmmu_clk_enable(drvdata)	if (drvdata->clk) \
						clk_enable(drvdata->clk)
#define __sysmmu_clk_disable(drvdata)	if (drvdata->clk) \
						clk_disable(drvdata->clk)
#define __master_clk_enable(drvdata)	if (drvdata->clk_master) \
						clk_enable(drvdata->clk_master)
#define __master_clk_disable(drvdata)	if (drvdata->clk_master) \
						clk_disable(drvdata->clk_master)

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
	SYSMMU_PAGEFAULT,
	SYSMMU_AR_MULTIHIT,
	SYSMMU_AW_MULTIHIT,
	SYSMMU_BUSERROR,
	SYSMMU_AR_SECURITY,
	SYSMMU_AR_ACCESS,
	SYSMMU_AW_SECURITY,
	SYSMMU_AW_PROTECTION, /* 7 */
	SYSMMU_FAULT_UNDEF,
	SYSMMU_FAULTS_NUM
};

static unsigned short fault_reg_offset[9] = {
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_PAGE_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AR_FAULT_ADDR,
	REG_AW_FAULT_ADDR,
	REG_AW_FAULT_ADDR
};

static char *sysmmu_fault_name[SYSMMU_FAULTS_NUM] = {
	"PAGE FAULT",
	"AR MULTI-HIT FAULT",
	"AW MULTI-HIT FAULT",
	"BUS ERROR",
	"AR SECURITY PROTECTION FAULT",
	"AR ACCESS PROTECTION FAULT",
	"AW SECURITY PROTECTION FAULT",
	"AW ACCESS PROTECTION FAULT",
	"UNDEFINED FAULT"
};

enum sysmmu_property {
	SYSMMU_PROP_RESERVED,
	SYSMMU_PROP_READ,
	SYSMMU_PROP_WRITE,
	SYSMMU_PROP_READWRITE = SYSMMU_PROP_READ | SYSMMU_PROP_WRITE,
	SYSMMU_PROP_RW_MASK = SYSMMU_PROP_READWRITE,
	SYSMMU_PROP_WINDOW_SHIFT = 16,
	SYSMMU_PROP_WINDOW_MASK = 0x1F << SYSMMU_PROP_WINDOW_SHIFT,
};

static const char * const sysmmu_prop_opts[] = {
	[SYSMMU_PROP_RESERVED]		= "Reserved",
	[SYSMMU_PROP_READ]		= "r",
	[SYSMMU_PROP_WRITE]		= "w",
	[SYSMMU_PROP_READWRITE]		= "rw",	/* default */
};

struct sysmmu_version {
	unsigned char major;
	unsigned char minor;
};

/*
 * Metadata attached to each System MMU devices.
 */
struct sysmmu_drvdata {
	struct device *sysmmu;	/* System MMU's device descriptor */
	struct device *master;	/* Client device that needs System MMU */
	int nsfrs;
	void __iomem **sfrbases;
	struct clk *clk;
	struct clk *clk_master;
	int activations;
	struct iommu_domain *domain; /* domain given to iommu_attach_device() */
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

static unsigned int __raw_sysmmu_version(struct sysmmu_drvdata *drvdata,
					 int idx)
{
	return MMU_RAW_VER(
		__raw_readl(drvdata->sfrbases[idx] + REG_MMU_VERSION));
}

static unsigned int __sysmmu_version(struct sysmmu_drvdata *drvdata,
					int idx, unsigned int *minor)
{
	unsigned int major = 0;

	major = __raw_sysmmu_version(drvdata, idx);
	if ((MMU_MAJ_VER(major) > 3)) {
		pr_err("%s: version(%d.%d) of %s[%d] is higher than 3.3\n",
			__func__, MMU_MAJ_VER(major), MMU_MIN_VER(major),
			dev_name(drvdata->sysmmu), idx);
		BUG();
		return major;
	}

	if (minor)
		*minor = MMU_MIN_VER(major);

	major = MMU_MAJ_VER(major);

	return major;
}

static bool has_sysmmu_capable_pbuf(struct sysmmu_drvdata *drvdata, int idx,
				int *min)
{
	unsigned int ver;

	ver = __raw_sysmmu_version(drvdata, idx);
	if (min)
		*min = MMU_MIN_VER(ver);
	return MMU_MAJ_VER(ver) == 3;
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

static void __sysmmu_set_ptbase(void __iomem *sfrbase,
				       phys_addr_t pgtable)
{
	__raw_writel(pgtable, sfrbase + REG_PT_BASE_ADDR);

	__sysmmu_tlb_invalidate(sfrbase);
}

static void __sysmmu_set_prefbuf(void __iomem *pbufbase, unsigned long base,
					unsigned long size, int idx)
{
	__raw_writel(base, pbufbase + idx * 8);
	__raw_writel(size - 1 + base,  pbufbase + 4 + idx * 8);
}

/*
 * Offset of prefetch buffer setting registers are different
 * between SysMMU 3.1 and 3.2. 3.3 has a single prefetch buffer setting.
 */
static unsigned short
	pbuf_offset[NUM_MINOR_OF_SYSMMU_V3] = {0x04C, 0x04C, 0x070, 0x410};

#define PB_CFG_MASK	0x11111;

static int __prepare_prefetch_buffers_by_plane(struct sysmmu_drvdata *drvdata,
				struct sysmmu_prefbuf prefbuf[], int num_pb,
				int inplanes, int onplanes,
				int ipoption, int opoption)
{
	int ret_num_pb = 0;
	int i = 0;
	struct exynos_iovmm *vmm;

	if (!drvdata->master || !drvdata->master->archdata.iommu) {
		dev_err(drvdata->sysmmu, "%s: No master device is specified\n",
					__func__);
		return 0;
	}

	vmm = ((struct exynos_iommu_owner *)
			(drvdata->master->archdata.iommu))->vmm_data;

	if (!vmm)
		return 0; /* No VMM information to set prefetch buffers */

	if (!inplanes && !onplanes) {
		inplanes = vmm->inplanes;
		onplanes = vmm->onplanes;
	}

	ipoption &= PB_CFG_MASK;
	opoption &= PB_CFG_MASK;

	if (drvdata->prop & SYSMMU_PROP_READ) {
		ret_num_pb = min(inplanes, num_pb);
		for (i = 0; i < ret_num_pb; i++) {
			prefbuf[i].base = vmm->iova_start[i];
			prefbuf[i].size = vmm->iovm_size[i];
			prefbuf[i].config = ipoption;
		}
	}

	if ((drvdata->prop & SYSMMU_PROP_WRITE) &&
				(ret_num_pb < num_pb) && (onplanes > 0)) {
		for (i = 0; i < min(num_pb - ret_num_pb, onplanes); i++) {
			prefbuf[ret_num_pb + i].base =
					vmm->iova_start[vmm->inplanes + i];
			prefbuf[ret_num_pb + i].size =
					vmm->iovm_size[vmm->inplanes + i];
			prefbuf[ret_num_pb + i].config = opoption;
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
				prefbuf[ret_num_pb].config = ipoption;
				ret_num_pb++;
			}
			prop >>= 1;
			if (prop == 0)
				break;
		}
	}

	return ret_num_pb;
}

static void __sysmmu_set_pbuf_ver31(struct sysmmu_drvdata *drvdata,
					struct sysmmu_prefbuf prefbuf[],
					int num_bufs, int idx)
{
	unsigned long cfg =
		__raw_readl(drvdata->sfrbases[idx] + REG_MMU_CFG) & CFG_MASK;

	/* Only the first 2 buffers are set to PB */
	if (num_bufs >= 2) {
		/* Separate PB mode */
		cfg |= 2 << 28;

		if (prefbuf[1].size == 0)
			prefbuf[1].size = 1;
		__sysmmu_set_prefbuf(drvdata->sfrbases[idx] + pbuf_offset[1],
					prefbuf[1].base, prefbuf[1].size, 1);
	} else {
		/* Combined PB mode */
		cfg |= 3 << 28;
		drvdata->num_pbufs = 1;
		drvdata->pbufs[0] = prefbuf[0];
	}

	__raw_writel(cfg, drvdata->sfrbases[idx] + REG_MMU_CFG);

	if (prefbuf[0].size == 0)
		prefbuf[0].size = 1;
	__sysmmu_set_prefbuf(drvdata->sfrbases[idx] + pbuf_offset[1],
				prefbuf[0].base, prefbuf[0].size, 0);
}

static void __sysmmu_set_pbuf_ver32(struct sysmmu_drvdata *drvdata,
					struct sysmmu_prefbuf prefbuf[],
					int num_bufs, int idx)
{
	int i;
	unsigned long cfg =
		__raw_readl(drvdata->sfrbases[idx] + REG_MMU_CFG) & CFG_MASK;

	cfg |= 7 << 16; /* enabling PB0 ~ PB2 */

	switch (num_bufs) {
	case 1:
		/* Combined PB mode (0 ~ 2) */
		cfg |= 1 << 19;
		break;
	case 2:
		/* Combined PB mode (0 ~ 1) */
		cfg |= 1 << 21;
		break;
	case 3:
		break;
	default:
		num_bufs = 3; /* Only the first 3 buffers are set to PB */
	}

	for (i = 0; i < num_bufs; i++) {
		if (prefbuf[i].size == 0) {
			dev_err(drvdata->sysmmu,
				"%s: Trying to init PB[%d/%d]with zero-size\n",
				__func__, idx, num_bufs);
			prefbuf[i].size = 1;
		}
		__sysmmu_set_prefbuf(drvdata->sfrbases[idx] + pbuf_offset[2],
			prefbuf[i].base, prefbuf[i].size, i);
	}

	__raw_writel(cfg, drvdata->sfrbases[idx] + REG_MMU_CFG);
}

static unsigned int find_lmm_preset(unsigned int num_pb, unsigned int num_bufs)
{
	static char lmm_preset[4][6] = {  /* [num of PB][num of buffers] */
	/*	  1,  2,  3,  4,  5,  6 */
		{ 1,  1,  0, -1, -1, -1}, /* num of pb: 3 */
		{ 3,  2,  1,  0, -1, -1}, /* num of pb: 4 */
		{-1, -1, -1, -1, -1, -1},
		{ 5,  5,  4,  2,  1,  0}, /* num of pb: 6 */
		};
	unsigned int lmm;

	BUG_ON(num_bufs > 6);
	lmm = lmm_preset[num_pb - 3][num_bufs - 1];
	BUG_ON(lmm == -1);
	return lmm;
}

static unsigned int find_num_pb(unsigned int num_pb, unsigned int lmm)
{
	static char lmm_preset[6][6] = { /* [pb_num - 1][pb_lmm] */
		{0, 0, 0, 0, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{3, 2, 0, 0, 0, 0},
		{4, 3, 2, 1, 0, 0},
		{0, 0, 0, 0, 0, 0},
		{6, 5, 4, 3, 3, 2},
	};

	num_pb = lmm_preset[num_pb - 1][lmm];
	BUG_ON(num_pb == 0);
	return num_pb;
}

static void __sysmmu_set_pbuf_ver33(struct sysmmu_drvdata *drvdata,
					struct sysmmu_prefbuf prefbuf[],
					int num_bufs, int idx)
{
	unsigned int i, num_pb, lmm;

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbases[idx] + REG_PB_INFO));

	lmm = find_lmm_preset(num_pb, (unsigned int)num_bufs);
	num_pb = find_num_pb(num_pb, lmm);

	__raw_writel(lmm, drvdata->sfrbases[idx] + REG_PB_LMM);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, drvdata->sfrbases[idx] + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbases[idx] + REG_PB_CFG);
		if (prefbuf[i].size == 0) {
			dev_err(drvdata->sysmmu,
				"%s: Trying to init PB[%d/%d]with zero-size\n",
				__func__, idx, num_bufs);
			continue;
		}
		if (num_bufs <= i)
			continue; /* unused PB */
		__sysmmu_set_prefbuf(drvdata->sfrbases[idx] + pbuf_offset[3],
					prefbuf[i].base, prefbuf[i].size, 0);
		__raw_writel(prefbuf[i].config | 1,
					drvdata->sfrbases[idx] + REG_PB_CFG);
	}
}

static void (*func_set_pbuf[NUM_MINOR_OF_SYSMMU_V3])
		(struct sysmmu_drvdata *, struct sysmmu_prefbuf *, int, int) = {
		__sysmmu_set_pbuf_ver31,
		__sysmmu_set_pbuf_ver31,
		__sysmmu_set_pbuf_ver32,
		__sysmmu_set_pbuf_ver33,
};


static void __sysmmu_disable_pbuf_ver31(struct sysmmu_drvdata *drvdata, int idx)
{
	__raw_writel(
		__raw_readl(drvdata->sfrbases[idx] + REG_MMU_CFG) & CFG_MASK,
		drvdata->sfrbases[idx] + REG_MMU_CFG);
}

#define __sysmmu_disable_pbuf_ver32 __sysmmu_disable_pbuf_ver31

static void __sysmmu_disable_pbuf_ver33(struct sysmmu_drvdata *drvdata, int idx)
{
	unsigned int i, num_pb;
	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbases[idx] + REG_PB_INFO));

	__raw_writel(0, drvdata->sfrbases[idx] + REG_PB_LMM);

	for (i = 0; i < num_pb; i++) {
		__raw_writel(i, drvdata->sfrbases[idx] + REG_PB_INDICATE);
		__raw_writel(0, drvdata->sfrbases[idx] + REG_PB_CFG);
	}
}

static void (*func_disable_pbuf[NUM_MINOR_OF_SYSMMU_V3])
					(struct sysmmu_drvdata *, int) = {
		__sysmmu_disable_pbuf_ver31,
		__sysmmu_disable_pbuf_ver31,
		__sysmmu_disable_pbuf_ver32,
		__sysmmu_disable_pbuf_ver33,
};

static u32 __sysmmu_get_num_pb(struct sysmmu_drvdata *drvdata, int idx,
				int *min)
{
	unsigned long major;
	int num_pb = 0;

	major = __raw_sysmmu_version(drvdata, idx);
	if (MMU_MAJ_VER(major) == 3) {
		*min = MMU_MIN_VER(major);
		if (*min < 3)
			return (*min == 2) ? 3 : 2;
	} else {
		return 0;
	}

	num_pb = PB_INFO_NUM(__raw_readl(drvdata->sfrbases[idx] + REG_PB_INFO));
	if ((num_pb != 3) && (num_pb != 4) && (num_pb != 6)) {
		dev_err(drvdata->master,
				"%s: Read invalid PB information from %s\n",
				__func__, dev_name(drvdata->sysmmu));
		return -EINVAL;
	}

	return num_pb;
}

static void __exynos_sysmmu_set_prefbuf_by_region(
		struct sysmmu_drvdata *drvdata, int idx,
		struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	unsigned int i;
	int num_bufs = 0;
	struct sysmmu_prefbuf prefbuf[6];
	unsigned int min;

	min = __raw_sysmmu_version(drvdata, idx);
	if (min == 0)
		return;

	if ((num_reg == 0) || (pb_reg == NULL)) {
		/* Disabling prefetch buffers */
		func_disable_pbuf[MMU_MIN_VER(min)](drvdata, idx);
		return;
	}

	for (i = 0; i < num_reg; i++) {
		if (((pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
					(drvdata->prop & SYSMMU_PROP_WRITE)) ||
			(!(pb_reg[i].config & SYSMMU_PBUFCFG_WRITE) &&
				 (drvdata->prop & SYSMMU_PROP_READ)))
			prefbuf[num_bufs++] = pb_reg[i];
	}

	func_set_pbuf[MMU_MIN_VER(min)](drvdata, prefbuf, num_bufs, idx);
}

static void __exynos_sysmmu_set_prefbuf_by_plane(struct sysmmu_drvdata *drvdata,
			int idx, unsigned int inplanes, unsigned int onplanes,
			unsigned int ipoption, unsigned int opoption)
{
	unsigned int num_pb;
	int num_bufs, min;
	struct sysmmu_prefbuf prefbuf[6];

	num_pb = __sysmmu_get_num_pb(drvdata, idx, &min);
	if (num_pb == 0) /* No Prefetch buffers */
		return;

	num_bufs = __prepare_prefetch_buffers_by_plane(drvdata,
			prefbuf, num_pb, inplanes, onplanes,
			ipoption, opoption);

	if (num_bufs == 0)
		func_disable_pbuf[min](drvdata, idx);
	else
		func_set_pbuf[min](drvdata, prefbuf, num_bufs, idx);
}

void sysmmu_set_prefetch_buffer_by_region(struct device *dev,
			struct sysmmu_prefbuf pb_reg[], unsigned int num_reg)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct device *sysmmu;
	unsigned long flags;

	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: No System MMU is configured\n", __func__);
		return;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		int idx;

		spin_lock_irqsave(&drvdata->lock, flags);

		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		for (idx = 0; idx < drvdata->nsfrs; idx++)
			__exynos_sysmmu_set_prefbuf_by_region(drvdata,
						idx, pb_reg, num_reg);

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

int sysmmu_set_prefetch_buffer_by_plane(struct device *dev,
		unsigned int inplanes, unsigned int onplanes,
		unsigned int ipoption, unsigned int opoption)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm;
	struct device *sysmmu;
	unsigned long flags;

	if (!dev->archdata.iommu) {
		dev_err(dev, "%s: No System MMU is configured\n", __func__);
		return -EINVAL;
	}

	vmm = exynos_get_iovmm(dev);
	if (!vmm) {
		dev_err(dev, "%s: IOVMM is not configured\n", __func__);
		return -EINVAL;
	}

	if ((inplanes > vmm->inplanes) || (onplanes > vmm->onplanes)) {
		dev_err(dev, "%s: Given planes [%d, %d] exceeds [%d, %d]\n",
				__func__, inplanes, onplanes,
				vmm->inplanes, vmm->onplanes);
		return -EINVAL;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		int idx;

		spin_lock_irqsave(&drvdata->lock, flags);

		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			continue;
		}

		__master_clk_enable(drvdata);

		for (idx = 0; idx < drvdata->nsfrs; idx++)
			__exynos_sysmmu_set_prefbuf_by_plane(drvdata, idx,
					inplanes, onplanes, ipoption, opoption);

		__master_clk_disable(drvdata);

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	spin_unlock_irqrestore(&owner->lock, flags);

	return 0;
}

__attribute__ ((unused))
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
			__master_clk_enable(drvdata);
			for (i = 0; i < drvdata->nsfrs; i++)
				__sysmmu_tlb_invalidate_entry(
						drvdata->sfrbases[i], iova);
			__master_clk_disable(drvdata);
		} else {
			dev_dbg(sysmmu,
			"Disabled. Skipping TLB invalidation @ %#x\n", iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static void sysmmu_tlb_invalidate_flpdcache(struct device *dev, dma_addr_t iova)
{
	struct device *sysmmu;
	int min;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) &&
				drvdata->runtime_active) {
			int i;
			__master_clk_enable(drvdata);
			for (i = 0; i < drvdata->nsfrs; i++)
				if (has_sysmmu_capable_pbuf(drvdata, i, &min)) {
					if (min == 3)
					__sysmmu_tlb_invalidate_entry(
						drvdata->sfrbases[i], iova);
				}
			__master_clk_disable(drvdata);
		} else {
			dev_dbg(sysmmu,
			"Disabled. Skipping TLB invalidation @ %#x\n", iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void exynos_sysmmu_tlb_invalidate(struct device *dev, dma_addr_t start,
				  size_t size)
{
	struct device *sysmmu;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;
		int i;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (!is_sysmmu_active(drvdata) ||
				!drvdata->runtime_active) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			dev_dbg(sysmmu,
				"%s: Skipping TLB invalidation of %#x@%#x\n",
				__func__, size, start);
			continue;
		}

		__master_clk_enable(drvdata);

		for (i = 0; i < drvdata->nsfrs; i++) {
			if (!WARN_ON(!sysmmu_block(drvdata->sfrbases[i])))
				__sysmmu_tlb_invalidate(
						drvdata->sfrbases[i]);
			sysmmu_unblock(drvdata->sfrbases[i]);
		}

		__master_clk_disable(drvdata);

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static void dump_sysmmu_tlb_pb(void __iomem *sfrbase)
{
	unsigned int i, capa, lmm, tlb_ent_num;

	lmm = MMU_RAW_VER(__raw_readl(sfrbase + REG_MMU_VERSION));

	pr_crit("---------- System MMU Status -----------------------------\n");
	pr_crit("VERSION %d.%d, MMU_CFG: %#010x, MMU_STATUS: %#010x\n",
		MMU_MAJ_VER(lmm), MMU_MIN_VER(lmm),
		__raw_readl(sfrbase + REG_MMU_CFG),
		__raw_readl(sfrbase + REG_MMU_STATUS));


	pr_crit("---------- Level 1 TLB -----------------------------------\n");

	tlb_ent_num = MMU_TLB_ENT_NUM(__raw_readl(sfrbase + REG_MMU_VERSION));
	for (i = 0; i < tlb_ent_num; i++) {
		__raw_writel(i, sfrbase + REG_L1TLB_READ_ENTRY);
		pr_crit("[%02d] VPN: %#010x, PPN: %#010x\n",
			i, __raw_readl(sfrbase + REG_L1TLB_ENTRY_VPN),
			__raw_readl(sfrbase + REG_L1TLB_ENTRY_PPN));
	}

	capa = __raw_readl(sfrbase + REG_PB_INFO);
	lmm = __raw_readl(sfrbase + REG_PB_LMM);

	pr_crit("---------- Prefetch Buffers ------------------------------\n");
	pr_crit("PB_INFO: %#010x, PB_LMM: %#010x\n", capa, lmm);

	capa = find_num_pb(capa & 0xFF, lmm);

	for (i = 0; i < capa; i++) {
		__raw_writel(i, sfrbase + REG_PB_INDICATE);
		pr_crit("PB[%d] = CFG: %#010x, START: %#010x, END: %#010x\n", i,
			__raw_readl(sfrbase + REG_PB_CFG),
			__raw_readl(sfrbase + REG_PB_START_ADDR),
			__raw_readl(sfrbase + REG_PB_END_ADDR));

		pr_crit("PB[%d]_SUB0 BASE_VPN = %#010x\n", i,
			__raw_readl(sfrbase + REG_SPB_BASE_VPN));
		__raw_writel(i | 0x100, sfrbase + REG_PB_INDICATE);
		pr_crit("PB[%d]_SUB1 BASE_VPN = %#010x\n", i,
			__raw_readl(sfrbase + REG_SPB_BASE_VPN));
	}

	/* Reading L2TLB is not provided by H/W */
}

static void show_fault_information(struct sysmmu_drvdata *drvdata, int idx,
					enum exynos_sysmmu_inttype itype,
					unsigned long fault_addr)
{
	unsigned int info;
	phys_addr_t pgtable;
	int min;

	pgtable = __raw_readl(drvdata->sfrbases[idx] + REG_PT_BASE_ADDR);

	pr_crit("----------------------------------------------------------\n");
	pr_crit("%s[%d] %s at %#010lx by %s (page table @ %#010x)\n",
		dev_name(drvdata->sysmmu), idx,
		sysmmu_fault_name[itype], fault_addr,
		dev_name(drvdata->master), pgtable);

	if (itype== SYSMMU_FAULT_UNDEF) {
		pr_crit("The fault is not caused by this System MMU.\n");
		pr_crit("Please check IRQ and SFR base address.\n");
		goto finish;
	}

	if (__sysmmu_version(drvdata, idx, &min) == 3) {
		if (min == 3) {
			info = __raw_readl(drvdata->sfrbases[idx] +
					REG_FAULT_TRANS_INFO);
			pr_crit("AxID: %#x, AxLEN: %#x RW: %s\n",
				info & 0xFFFF, (info >> 16) & 0xF,
				(info >> 20) ? "WRITE" : "READ");
		}
	}

	if (pgtable != drvdata->pgtable)
		pr_crit("Page table base of driver: %#010x\n",
			drvdata->pgtable);

	if (itype == SYSMMU_BUSERROR) {
		pr_crit("System MMU has failed to access page table\n");
		goto finish;
	}

	if (!pfn_valid(pgtable >> PAGE_SHIFT)) {
		pr_crit("Page table base is not in a valid memory region\n");
	} else {
		sysmmu_pte_t *ent;
		ent = section_entry(phys_to_virt(pgtable), fault_addr);
		pr_crit("Lv1 entry: %#010x\n", *ent);

		if (lv1ent_page(ent)) {
			ent = page_entry(ent, fault_addr);
			pr_crit("Lv2 entry: %#010x\n", *ent);
		}
	}

	if (min == 3)
		dump_sysmmu_tlb_pb(drvdata->sfrbases[idx]);

finish:
	pr_crit("----------------------------------------------------------\n");
}

static irqreturn_t exynos_sysmmu_irq(int irq, void *dev_id)
{
	/* SYSMMU is in blocked when interrupt occurred. */
	struct sysmmu_drvdata *drvdata = dev_id;
	unsigned int itype;
	unsigned long addr = -1;
	int i, ret = -ENOSYS;
	int flags = 0;

	WARN(!is_sysmmu_active(drvdata),
		"Fault occurred while System MMU %s is not enabled!\n",
		dev_name(drvdata->sysmmu));

	for (i = 0; i < drvdata->nsfrs; i++) {
		struct resource *irqres;
		irqres = platform_get_resource(
				to_platform_device(drvdata->sysmmu),
				IORESOURCE_IRQ, i);
		if (irqres && ((int)irqres->start == irq))
			break;
	}

	__master_clk_enable(drvdata);

	if (i == drvdata->nsfrs) {
		itype = SYSMMU_FAULT_UNDEF;
	} else {
		itype =  __ffs(__raw_readl(
				drvdata->sfrbases[i] + REG_INT_STATUS));

		if (WARN_ON(!((itype >= 0) && (itype < SYSMMU_FAULT_UNDEF))))
			itype = SYSMMU_FAULT_UNDEF;
		else
			addr = __raw_readl(
				drvdata->sfrbases[i] + fault_reg_offset[itype]);
	}

	show_fault_information(drvdata, i, itype, addr);

	if (drvdata->domain) /* master is set if drvdata->domain exists */
		ret = report_iommu_fault(drvdata->domain,
					drvdata->master, addr, flags);

	__master_clk_disable(drvdata);

	panic("Unrecoverable System MMU Fault!!");

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

	__sysmmu_clk_disable(drvdata);
	if (IS_ENABLED(CONFIG_EXYNOS_IOMMU_NO_MASTER_CLKGATE))
		__master_clk_disable(drvdata);
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

		if (drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			__sysmmu_disable_nocount(drvdata);
			__master_clk_disable(drvdata);
		}

		dev_dbg(drvdata->sysmmu, "Disabled\n");
	} else  {
		dev_dbg(drvdata->sysmmu, "%d times left to disable\n",
					drvdata->activations);
	}

	spin_unlock_irqrestore(&drvdata->lock, flags);

	return disabled;
}


static void __sysmmu_init_config(struct sysmmu_drvdata *drvdata, int idx)
{
	unsigned long cfg = CFG_LRU | CFG_QOS(drvdata->qos);
	int maj, min = 0;

	maj = __sysmmu_version(drvdata, idx, &min);
	if ((maj == 1) || (maj == 2))
		goto set_cfg;

	BUG_ON(maj != 3);

	if (min < 2)
		goto set_pb;

	BUG_ON(min > 3);

	cfg |= CFG_FLPDCACHE;
	cfg |= (min == 2) ? CFG_SYSSEL : CFG_ACGEN;
	cfg |= CFG_QOS_OVRRIDE;

set_pb:
	__exynos_sysmmu_set_prefbuf_by_plane(drvdata, idx, 0, 0,
				SYSMMU_PBUFCFG_DEFAULT_INPUT,
				SYSMMU_PBUFCFG_DEFAULT_OUTPUT);
set_cfg:
	cfg |= __raw_readl(drvdata->sfrbases[idx] + REG_MMU_CFG) & ~CFG_MASK;
	__raw_writel(cfg, drvdata->sfrbases[idx] + REG_MMU_CFG);
}

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *drvdata)
{
	int i;

	if (IS_ENABLED(CONFIG_EXYNOS_IOMMU_NO_MASTER_CLKGATE))
		__master_clk_enable(drvdata);
	__sysmmu_clk_enable(drvdata);

	for (i = 0; i < drvdata->nsfrs; i++) {
		__raw_writel(0, drvdata->sfrbases[i] + REG_MMU_CTRL);

		__sysmmu_init_config(drvdata, i);

		__sysmmu_set_ptbase(drvdata->sfrbases[i], drvdata->pgtable);

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

		if (drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			__sysmmu_enable_nocount(drvdata);
			__master_clk_disable(drvdata);
		}

		dev_dbg(drvdata->sysmmu, "Enabled\n");
	} else {
		ret = (pgtable == drvdata->pgtable) ? 1 : -EBUSY;

		dev_dbg(drvdata->sysmmu, "Already enabled\n");
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
		if (PTR_ERR(drvdata->clk) == -ENOENT) {
			dev_dbg(sysmmu, "No gating clock found.\n");
			drvdata->clk = NULL;
			return 0;
		}

		dev_err(sysmmu, "Failed get sysmmu clock\n");
		return PTR_ERR(drvdata->clk);
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

	ret = clk_prepare(drvdata->clk_master);
	if (ret) {
		dev_dbg(sysmmu, "clk_prepare() failed\n");
		return ret;
	}

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

static int __init __sysmmu_init_prop(struct device *sysmmu,
				     struct sysmmu_drvdata *drvdata)
{
	struct device_node *prop_node;
	const char *s;
	int winmap = 0;
	unsigned int qos = 8;
	int ret;

	drvdata->prop = SYSMMU_PROP_READWRITE;

	ret = of_property_read_u32_index(sysmmu->of_node, "qos", 0, &qos);

	if ((ret == 0) && (qos > 15)) {
		dev_err(sysmmu, "%s: Invalid QoS value %d specified\n",
				__func__, qos);
		qos = 8;
	}

	drvdata->qos = (short)qos;

	prop_node = of_get_child_by_name(sysmmu->of_node, "prop-map");
	if (!prop_node)
		return 0;

	if (!of_property_read_string(prop_node, "iomap", &s)) {
		int val;
		for (val = 1; val < ARRAY_SIZE(sysmmu_prop_opts); val++) {
			if (!strcasecmp(s, sysmmu_prop_opts[val])) {
				drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
				drvdata->prop = val;
				break;
			}
		}
	} else if (!of_property_read_u32_index(
					prop_node, "winmap", 0, &winmap)) {
		if (winmap) {
			drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
			drvdata->prop = winmap << SYSMMU_PROP_WINDOW_SHIFT;
		}
	}

	return 0;
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

	ret = __sysmmu_init_prop(sysmmu, drvdata);
	if (ret)
		dev_err(sysmmu, "Failed to initialize sysmmu properties\n");

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
		(!pm_runtime_enabled(dev) || drvdata->runtime_active)) {
		__master_clk_enable(drvdata);
		__sysmmu_disable_nocount(drvdata);
		__master_clk_disable(drvdata);
	}
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
		__master_clk_enable(drvdata);
		__sysmmu_enable_nocount(drvdata);
		__master_clk_disable(drvdata);
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
	if (is_sysmmu_active(drvdata)) {
		__master_clk_enable(drvdata);
		__sysmmu_disable_nocount(drvdata);
		__master_clk_disable(drvdata);
	}
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
	if (is_sysmmu_active(drvdata)) {
		__master_clk_enable(drvdata);
		__sysmmu_enable_nocount(drvdata);
		__master_clk_disable(drvdata);
	}
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
	{ .compatible = "samsung,exynos4210-sysmmu", },
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
	int i;

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

	for (i = 0; i < NUM_LV1ENTRIES; i += 8) {
		priv->pgtable[i + 0] = ZERO_LV2LINK;
		priv->pgtable[i + 1] = ZERO_LV2LINK;
		priv->pgtable[i + 2] = ZERO_LV2LINK;
		priv->pgtable[i + 3] = ZERO_LV2LINK;
		priv->pgtable[i + 4] = ZERO_LV2LINK;
		priv->pgtable[i + 5] = ZERO_LV2LINK;
		priv->pgtable[i + 6] = ZERO_LV2LINK;
		priv->pgtable[i + 7] = ZERO_LV2LINK;
	}

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

	list_for_each_entry(owner, &priv->clients, client)
		while (!exynos_sysmmu_disable(owner->dev))
			; /* until System MMU is actually disabled */

	while (!list_empty(&priv->clients))
		list_del_init(priv->clients.next);

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

static sysmmu_pte_t *alloc_lv2entry(struct exynos_iommu_domain *priv,
		sysmmu_pte_t *sent, unsigned long iova, short *pgcounter)
{
	if (lv1ent_fault(sent)) {
		sysmmu_pte_t *pent;
		struct exynos_iommu_owner *owner;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(__pa(pent));
		kmemleak_ignore(pent);
		*pgcounter = NUM_LV2ENTRIES;
		pgtable_flush(pent, pent + NUM_LV2ENTRIES);
		pgtable_flush(sent, sent + 1);

		/*
		 * If pretched SLPD is a fault SLPD in zero_l2_table, FLPD cache
		 * caches the address of zero_l2_table. This function replaces
		 * the zero_l2_table with new L2 page table to write valid
		 * mappings.
		 * Accessing the valid area may cause page fault since FLPD
		 * cache may still caches zero_l2_table for the valid area
		 * instead of new L2 page table that have the mapping
		 * information of the valid area
		 * Thus any replacement of zero_l2_table with other valid L2
		 * page table must involve FLPD cache invalidation if the System
		 * MMU have prefetch feature and FLPD cache (version 3.3).
		 * FLPD cache invalidation is performed with TLB invalidation
		 * by VPN without blocking. It is safe to invalidate TLB without
		 * blocking because the target address of TLB invalidation is not
		 * currently mapped.
		 */
		list_for_each_entry(owner, &priv->clients, client)
			sysmmu_tlb_invalidate_flpdcache(owner->dev, iova);
	} else if (!lv1ent_page(sent)) {
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static sysmmu_pte_t *alloc_lv2entry_fast(struct exynos_iommu_domain *priv,
		sysmmu_pte_t *sent, unsigned long iova)
{
	if (lv1ent_fault(sent)) {
		sysmmu_pte_t *pent;

		pent = kmem_cache_zalloc(lv2table_kmem_cache, GFP_ATOMIC);
		BUG_ON((unsigned long)pent & (LV2TABLE_SIZE - 1));
		if (!pent)
			return ERR_PTR(-ENOMEM);

		*sent = mk_lv1ent_page(__pa(pent));
		kmemleak_ignore(pent);
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

static void clear_lv1_page_table(sysmmu_pte_t *ent, int n)
{
	int i;
	for (i = 0; i < n; i++)
		ent[i] = ZERO_LV2LINK;
}

static void clear_lv2_page_table(sysmmu_pte_t *ent, int n)
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
	} else {
		int i;
		for (i = 0; i < SECT_PER_SPSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(sent, pgcnt);
			if (ret) {
				clear_lv1_page_table(sent - i, i);
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
				clear_lv2_page_table(pent - i, i);
				return -EADDRINUSE;
			}

			*pent = mk_lv2ent_lpage(paddr);
		}
		pgtable_flush(pent - SPAGES_PER_LPAGE, pent);
		*pgcnt -= SPAGES_PER_LPAGE;
	}

	return 0;
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
		int num_entry = size / SECT_SIZE;
		struct exynos_iommu_owner *owner;
		int i;

		list_for_each_entry(owner, &priv->clients, client)
			for (i = 0; i < num_entry; i++)
				if (entry[i] == ZERO_LV2LINK)
					sysmmu_tlb_invalidate_flpdcache(
						owner->dev,
						iova + i * SECT_SIZE);

		ret = lv1set_section(entry, paddr, size,
					&priv->lv2entcnt[lv1ent_offset(iova)]);
	} else {
		sysmmu_pte_t *pent;

		pent = alloc_lv2entry(priv, entry, iova,
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

		clear_lv1_page_table(ent, SECT_PER_SPSECT);

		pgtable_flush(ent, ent + SECT_PER_SPSECT);
		size = SPSECT_SIZE;
		goto done;
	}

	if (lv1ent_section(ent)) {
		if (WARN_ON(size < SECT_SIZE)) {
			err_pgsize = SECT_SIZE;
			goto err;
		}

		*ent = ZERO_LV2LINK;
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

	clear_lv2_page_table(ent, SPAGES_PER_LPAGE);
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

static int __sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct exynos_iommu_domain *priv = domain->priv;
	struct vm_area_struct *vma;
	unsigned long start, end;
	unsigned long flags;
	bool is_pfnmap;
	sysmmu_pte_t *sent, *pent;

	down_read(&mm->mmap_sem);

	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
	if (IS_ERR_OR_NULL(vma)) {
		pr_err("%s: invalid vma\n", __func__);
		return -EFAULT;
	}

	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	start = vaddr & PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);

	pr_debug("%s: unmap start @ %#lx for %d bytes\n", __func__, start, size);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	do {
		sysmmu_pte_t *pent_first;
		int i;

		sent = section_entry(priv->pgtable, start);
		pent = page_entry(sent, start);

		pent_first = pent;
		do {
			i = lv2ent_offset(start);

			if (!lv2ent_fault(pent) && !is_pfnmap)
				put_page(phys_to_page(spage_phys(pent)));

			*pent = 0;
			start += PAGE_SIZE;
		} while (pent++, (start != end) && (i < (NUM_LV2ENTRIES - 1)));

		pgtable_flush(pent_first, pent);
	} while (start != end);

	spin_unlock_irqrestore(&priv->pgtablelock, flags);
	up_read(&mm->mmap_sem);

	pr_debug("%s: unmap done @ %#lx\n", __func__, start);

	return 0;
}

int exynos_sysmmu_map_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr,
					size_t size, int write)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct exynos_iovmm *vmm = owner->vmm_data;
	struct iommu_domain *domain = vmm->domain;
	struct exynos_iommu_domain *priv = domain->priv;
	struct vm_area_struct *vma;
	unsigned long flags;
	unsigned long start, end;
	unsigned long pgd_next;
	int ret = -EINVAL;
	bool is_pfnmap;
	pgd_t *pgd;

	down_read(&mm->mmap_sem);

	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
	if (IS_ERR_OR_NULL(vma)) {
		pr_err("%s: invalid vma\n", __func__);
		return -EFAULT;
	}

	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	start = vaddr & PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);

	pr_debug("%s: map @ %#lx--%#lx, %d bytes, vm_flags: %#lx\n",
			__func__, start, end, size, vma->vm_flags);

	spin_lock_irqsave(&priv->pgtablelock, flags);

	pgd = pgd_offset(mm, start);
	do {
		unsigned long pmd_next;
		pmd_t *pmd;

		if (pgd_none_or_clear_bad(pgd))
			goto out_unmap;

		pgd_next = pgd_addr_end(start, end);
		pmd = pmd_offset((pud_t *)pgd, start);

		do {
			pte_t *pte, *pte_first;
			sysmmu_pte_t *pent, *pent_first;
			sysmmu_pte_t *sent;

			if (pmd_none_or_clear_bad(pmd))
				goto out_unmap;

			pmd_next = pmd_addr_end(start, pgd_next);
			pmd_next = min(ALIGN(start + 1, SECT_SIZE), pmd_next);

			pte = pte_offset_map(pmd, start);
			pte_first = pte;

			sent = section_entry(priv->pgtable, start);
			pent = alloc_lv2entry_fast(priv, sent, start);
			if (IS_ERR(pent)) {
				ret = PTR_ERR(pent);
				goto out_unmap;
			}

			pent_first = pent;
			do {
				if (pte_none(*pte))
					goto out_unmap;

				if (write && (!pte_write(*pte) ||
						!pte_dirty(*pte))) {
					ret = handle_pte_fault(mm,
							vma, start,
							pte, pmd,
							FAULT_FLAG_WRITE);
					if (IS_ERR_VALUE(ret))
						goto out_unmap;
				}

				if (lv2ent_fault(pent)) {
					if (!is_pfnmap)
						get_page(pte_page(*pte));
					*pent = mk_lv2ent_spage(__pfn_to_phys(
								pte_pfn(*pte)));
				}
			} while (pte++, pent++, start += PAGE_SIZE, start < pmd_next);

			pte_unmap(pte_first);
			pgtable_flush(pent_first, pent);
		} while (pmd++, start = pmd_next, start != pgd_next);

	} while (pgd++, start = pgd_next, start != end);

	ret = 0;
out_unmap:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);
	up_read(&mm->mmap_sem);

	if (ret) {
		pr_err("%s: out unmap @ %#lx, end %#lx\n", __func__, start, end);
		__sysmmu_unmap_user_pages(dev, mm, vaddr, start - vaddr);
	}

	return ret;
}

int exynos_sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size)
{
	return __sysmmu_unmap_user_pages(dev, mm, vaddr, size);
}

static struct iommu_ops exynos_iommu_ops = {
	.domain_init = &exynos_iommu_domain_init,
	.domain_destroy = &exynos_iommu_domain_destroy,
	.attach_dev = &exynos_iommu_attach_device,
	.detach_dev = &exynos_iommu_detach_device,
	.map = &exynos_iommu_map,
	.unmap = &exynos_iommu_unmap,
	.iova_to_phys = &exynos_iommu_iova_to_phys,
	.pgsize_bitmap = SECT_SIZE | LPAGE_SIZE | SPAGE_SIZE,
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
			continue;
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
	struct page *page;
	int ret = -ENOMEM;

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
		goto err_fault_page;
	}
	fault_page = page_to_phys(page);

	ret = bus_set_iommu(&platform_bus_type, &exynos_iommu_ops);
	if (ret) {
		pr_err("%s: Failed to register IOMMU ops\n", __func__);
		goto err_set_iommu;
	}

	zero_lv2_table = kmem_cache_zalloc(lv2table_kmem_cache, GFP_KERNEL);
	if (zero_lv2_table == NULL) {
		pr_err("%s: Failed to allocate zero level2 page table\n",
			__func__);
		ret = -ENOMEM;
		goto err_zero_lv2;
	}

	ret = platform_driver_register(&exynos_sysmmu_driver);
	if (ret) {
		pr_err("%s: Failed to register System MMU driver.\n", __func__);
		goto err_driver_register;
	}

	return 0;
err_driver_register:
	kmem_cache_free(lv2table_kmem_cache, zero_lv2_table);
err_zero_lv2:
	bus_set_iommu(&platform_bus_type, NULL);
err_set_iommu:
	__free_page(page);
err_fault_page:
	kmem_cache_destroy(lv2table_kmem_cache);
	return ret;
}
subsys_initcall(exynos_iommu_init);
