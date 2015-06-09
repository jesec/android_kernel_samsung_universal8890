#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/err.h>
#include <linux/mm.h>
#include <linux/errno.h>
#include <linux/memblock.h>
#include <linux/export.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/device.h>
#include <linux/clk-private.h>

#include <asm/cacheflush.h>
#include <asm/pgtable.h>

#include "exynos-iommu.h"

void *sysmmu_placeholder; /* Inidcate if a device is System MMU */

static struct kmem_cache *lv2table_kmem_cache;
static phys_addr_t fault_page;
unsigned long *zero_lv2_table;

static inline void pgtable_flush(void *vastart, void *vaend)
{
	dmac_flush_range(vastart, vaend);
	outer_flush_range(virt_to_phys(vastart),
				virt_to_phys(vaend));
}

void sysmmu_tlb_invalidate_flpdcache(struct device *dev, dma_addr_t iova)
{
	struct device *sysmmu;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			int i;
			__master_clk_enable(drvdata);
			for (i = 0; i < drvdata->nsfrs; i++)
				__sysmmu_tlb_invalidate_flpdcache(
						drvdata->sfrbases[i], iova);
			__master_clk_disable(drvdata);
		} else {
			dev_dbg(sysmmu, "Skip FLPD invalidation @ %#x\n", iova);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

void sysmmu_tlb_invalidate_entry(struct device *dev, dma_addr_t iova)
{
	struct device *sysmmu;

	for_each_sysmmu(dev, sysmmu) {
		unsigned long flags;
		struct sysmmu_drvdata *drvdata;

		drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			int i;
			__master_clk_enable(drvdata);
			for (i = 0; i < drvdata->nsfrs; i++)
				__sysmmu_tlb_invalidate_entry(
						drvdata->sfrbases[i], iova);
			__master_clk_disable(drvdata);
		} else {
			dev_dbg(sysmmu, "Skip TLB invalidation @ %#x\n", iova);
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
		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			spin_unlock_irqrestore(&drvdata->lock, flags);
			dev_dbg(dev, "Skip TLB invalidation of %#x@%#x\n",
				size, start);
			continue;
		}

		__master_clk_enable(drvdata);

		for (i = 0; i < drvdata->nsfrs; i++)
			__sysmmu_tlb_invalidate(drvdata->sfrbases[i],
						start, size);

		__master_clk_disable(drvdata);

		spin_unlock_irqrestore(&drvdata->lock, flags);
	}
}

static inline void __sysmmu_disable_nocount(struct sysmmu_drvdata *drvdata)
{
	int i;

	for (i = 0; i < drvdata->nsfrs; i++)
		__raw_sysmmu_disable(drvdata->sfrbases[i]);

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

static void __sysmmu_enable_nocount(struct sysmmu_drvdata *drvdata)
{
	int i;

	if (IS_ENABLED(CONFIG_EXYNOS_IOMMU_NO_MASTER_CLKGATE))
		__master_clk_enable(drvdata);
	__sysmmu_clk_enable(drvdata);

	for (i = 0; i < drvdata->nsfrs; i++) {
		__sysmmu_init_config(drvdata, i);

		__sysmmu_set_ptbase(drvdata->sfrbases[i],
					drvdata->pgtable / PAGE_SIZE);

		__raw_sysmmu_enable(drvdata->sfrbases[i]);
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

/* called by exynos5-iommu.c and exynos7-iommu.c */
#define PB_CFG_MASK	0x11111;
int __prepare_prefetch_buffers_by_plane(struct sysmmu_drvdata *drvdata,
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

		__master_clk_enable(drvdata);

		for (idx = 0; idx < drvdata->nsfrs; idx++) {
			__exynos_sysmmu_set_prefbuf_by_region(drvdata,
						idx, pb_reg, num_reg);
		}

		__master_clk_disable(drvdata);

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
		int idx = 0;

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

void exynos_sysmmu_set_df(struct device *dev, dma_addr_t iova)
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
		return;
	}

	plane = find_iovmm_plane(vmm, iova);
	if (plane < 0) {
		dev_err(dev, "%s: IOVA %#x is out of IOVMM\n", __func__, iova);
		return;
	}

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		int idx = 0;

		spin_lock_irqsave(&drvdata->lock, flags);

		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			if (drvdata->prop & SYSMMU_PROP_WINDOW_MASK) {
				unsigned long prop;
				prop = drvdata->prop & SYSMMU_PROP_WINDOW_MASK;
				prop >>= SYSMMU_PROP_WINDOW_SHIFT;
				if (prop & (1 << plane)) {
					for (idx = 0; idx < drvdata->nsfrs;
									idx++)
						__exynos_sysmmu_set_df(
							drvdata, idx, iova);
				}
			} else {
				for (idx = 0; idx < drvdata->nsfrs; idx++)
					__exynos_sysmmu_set_df(
							drvdata, idx, iova);
			}
			__master_clk_disable(drvdata);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

void exynos_sysmmu_release_df(struct device *dev)
{
	struct exynos_iommu_owner *owner = dev->archdata.iommu;
	struct device *sysmmu;
	unsigned long flags;

	BUG_ON(!has_sysmmu(dev));

	spin_lock_irqsave(&owner->lock, flags);

	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);

		spin_lock_irqsave(&drvdata->lock, flags);
		if (is_sysmmu_active(drvdata) && drvdata->runtime_active) {
			__master_clk_enable(drvdata);
			__exynos_sysmmu_release_df(drvdata);
			__master_clk_disable(drvdata);
		}
		spin_unlock_irqrestore(&drvdata->lock, flags);
	}

	spin_unlock_irqrestore(&owner->lock, flags);
}

static int __init __sysmmu_init_clock(struct device *sysmmu,
					struct sysmmu_drvdata *drvdata)
{
	int ret;

	drvdata->clk = devm_clk_get(sysmmu, "sysmmu");
	if (IS_ERR(drvdata->clk)) {
		if (PTR_ERR(drvdata->clk) == -ENOENT) {
			dev_info(sysmmu, "No gating clock found.\n");
			drvdata->clk = NULL;
			return 0;
		}

		dev_err(sysmmu, "Failed get sysmmu clock\n");
		return PTR_ERR(drvdata->clk);
	}

	ret = clk_prepare(drvdata->clk);
	if (ret) {
		dev_err(sysmmu, "Failed to prepare sysmmu clock\n");
		return ret;
	}

	drvdata->clk_master = devm_clk_get(sysmmu, "master");
	if (PTR_ERR(drvdata->clk_master) == -ENOENT) {
		drvdata->clk_master = NULL;
		return 0;
	} else if (IS_ERR(drvdata->clk_master)) {
		dev_err(sysmmu, "Failed to get master clock\n");
		clk_unprepare(drvdata->clk);
		return PTR_ERR(drvdata->clk_master);
	}

	ret = clk_prepare(drvdata->clk_master);
	if (ret) {
		clk_unprepare(drvdata->clk);
		dev_err(sysmmu, "Failed to prepare master clock\n");
		return ret;
	}

	return 0;
}

#define has_more_master(dev) ((unsigned long)dev->archdata.iommu & 1)
#define master_initialized(dev) (!((unsigned long)dev->archdata.iommu & 1) \
				&& ((unsigned long)dev->archdata.iommu & ~1))

static struct device * __init __sysmmu_init_master(
				struct device *sysmmu, struct device *dev)
{
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

static const char * const sysmmu_prop_opts[] = {
	[SYSMMU_PROP_RESERVED]		= "Reserved",
	[SYSMMU_PROP_READ]		= "r",
	[SYSMMU_PROP_WRITE]		= "w",
	[SYSMMU_PROP_READWRITE]		= "rw",	/* default */
};

static int __init __sysmmu_init_prop(struct device *sysmmu,
				     struct sysmmu_drvdata *drvdata)
{
	struct device_node *prop_node;
	const char *s;
	int winmap = 0;
	unsigned int qos = DEFAULT_QOS_VALUE;
	int ret;

	drvdata->prop = SYSMMU_PROP_READWRITE;

	ret = of_property_read_u32_index(sysmmu->of_node, "qos", 0, &qos);

	if ((ret == 0) && (qos > 15)) {
		dev_err(sysmmu, "%s: Invalid QoS value %d specified\n",
				__func__, qos);
		qos = DEFAULT_QOS_VALUE;
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
				drvdata->prop |= val;
				break;
			}
		}
	} else if (!of_property_read_u32_index(
					prop_node, "winmap", 0, &winmap)) {
		if (winmap) {
			drvdata->prop &= ~SYSMMU_PROP_RW_MASK;
			drvdata->prop |= winmap << SYSMMU_PROP_WINDOW_SHIFT;
		}
	}

	return 0;
}

static int __init __sysmmu_setup(struct device *sysmmu,
				struct sysmmu_drvdata *drvdata)
{
	struct device *master;
	int ret;

	ret = __sysmmu_init_prop(sysmmu, drvdata);
	if (ret) {
		dev_err(sysmmu, "Failed to initialize sysmmu properties\n");
		return ret;
	}

	ret = __sysmmu_init_clock(sysmmu, drvdata);
	if (ret) {
		dev_err(sysmmu, "Failed to initialize gating clocks\n");
		return ret;
	}

	master = __sysmmu_init_master(sysmmu, sysmmu->archdata.iommu);
	if (!master) {
		dev_dbg(sysmmu, "No master device is assigned\n");
	} else if (IS_ERR(master)) {
		dev_err(sysmmu, "Failed to initialize master device.\n");
		return PTR_ERR(master);
	}

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
		(!pm_runtime_enabled(dev) || drvdata->runtime_active))
		__sysmmu_enable_nocount(drvdata);
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
	{ .compatible = SYSMMU_OF_COMPAT_STRING, },
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
		unsigned long flags;

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
		 * blocking because the target address of TLB invalidation is
		 * not currently mapped.
		 */
		spin_lock_irqsave(&priv->lock, flags);
		list_for_each_entry(owner, &priv->clients, client)
			sysmmu_tlb_invalidate_flpdcache(owner->dev, iova);
		spin_unlock_irqrestore(&priv->lock, flags);
	} else if (!lv1ent_page(sent)) {
		BUG();
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
}

static int lv1ent_check_page(sysmmu_pte_t *sent, short *pgcnt)
{
	if (lv1ent_page(sent)) {
		if (WARN_ON(*pgcnt != NUM_LV2ENTRIES))
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

	if (WARN_ON(!lv1ent_fault(sent) && !lv1ent_page(sent)))
		return -EADDRINUSE;

	if (size == SECT_SIZE) {
		ret = lv1ent_check_page(sent, pgcnt);
		if (ret)
			return ret;
		*sent = mk_lv1ent_sect(paddr);
		pgtable_flush(sent, sent + 1);
	} else if (size == DSECT_SIZE) {
		int i;
		for (i = 0; i < SECT_PER_DSECT; i++, sent++, pgcnt++) {
			ret = lv1ent_check_page(sent, pgcnt);
			if (ret) {
				clear_lv1_page_table(sent - i, i);
				return ret;
			}
			*sent = mk_lv1ent_dsect(paddr);
		}
		pgtable_flush(sent - SECT_PER_DSECT, sent);
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
		if (WARN_ON(!lv2ent_fault(pent)))
			return -EADDRINUSE;

		*pent = mk_lv2ent_spage(paddr);
		pgtable_flush(pent, pent + 1);
		*pgcnt -= 1;
	} else { /* size == LPAGE_SIZE */
		int i;
		for (i = 0; i < SPAGES_PER_LPAGE; i++, pent++) {
			if (WARN_ON(!lv2ent_fault(pent))) {
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
		unsigned long flags2;

		spin_lock_irqsave(&priv->lock, flags2);
		list_for_each_entry(owner, &priv->clients, client) {
			int i;
			for (i = 0; i < num_entry; i++)
				if (entry[i] == ZERO_LV2LINK)
					sysmmu_tlb_invalidate_flpdcache(
							owner->dev,
							iova + i * SECT_SIZE);
		}
		spin_unlock_irqrestore(&priv->lock, flags2);

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

	if (lv1ent_dsection(ent)) {
		if (WARN_ON(size < DSECT_SIZE)) {
			err_pgsize = DSECT_SIZE;
			goto err;
		}

		*ent = ZERO_LV2LINK;
		*(++ent) = ZERO_LV2LINK;
		pgtable_flush(ent, ent + 2);
		size = DSECT_SIZE;
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
	} else if (lv1ent_dsection(entry)) {
		phys = dsection_phys(entry) + dsection_offs(iova);
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
	.pgsize_bitmap = PGSIZE_BITMAP,
};

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
	is_pfnmap = vma->vm_flags & VM_PFNMAP;

	start = vaddr & PAGE_MASK;
	end = PAGE_ALIGN(vaddr + size);

	dev_dbg(dev, "%s: Unmap starts @ %#x@%#lx\n", __func__, size, start);

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
	} else if (WARN_ON(!lv1ent_page(sent))) {
		return ERR_PTR(-EADDRINUSE);
	}

	return page_entry(sent, iova);
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

	if (WARN_ON(size == 0))
		return 0;

	down_read(&mm->mmap_sem);

	/*
	 * Assumes that the VMA is safe.
	 * The caller must check the range of address space before calling this.
	 */
	vma = find_vma(mm, vaddr);
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
			} while (pte++, pent++,
				start += PAGE_SIZE, start < pmd_next);

			pte_unmap(pte_first);
			pgtable_flush(pent_first, pent);
		} while (pmd++, start = pmd_next, start != pgd_next);

	} while (pgd++, start = pgd_next, start != end);

	ret = 0;
out_unmap:
	spin_unlock_irqrestore(&priv->pgtablelock, flags);
	up_read(&mm->mmap_sem);

	if (ret) {
		pr_err("%s: Failed to map for %#lx ~ %#lx\n",
					__func__, start, end);
		__sysmmu_unmap_user_pages(dev, mm, vaddr, start - vaddr);
	}

	return ret;
}

int exynos_sysmmu_unmap_user_pages(struct device *dev,
					struct mm_struct *mm,
					unsigned long vaddr, size_t size)
{
	if (WARN_ON(size == 0))
		return 0;

	return __sysmmu_unmap_user_pages(dev, mm, vaddr, size);
}

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

static void sysmmu_dump_lv2_page_table(unsigned int lv1idx, sysmmu_pte_t *base)
{
	unsigned int i;
	for (i = 0; i < NUM_LV2ENTRIES; i += 4) {
		if (!base[i] && !base[i + 1] && !base[i + 2] && !base[i + 3])
			continue;
		pr_info("    LV2[%04d][%03d] %08x %08x %08x %08x\n",
			lv1idx, i,
			base[i], base[i + 1], base[i + 2], base[i + 3]);
	}
}

static void sysmmu_dump_page_table(sysmmu_pte_t *base)
{
	unsigned int i;

	pr_info("---- System MMU Page Table @ %#010x (ZeroLv2Desc: %#x) ----\n",
		virt_to_phys(base), ZERO_LV2LINK);

	for (i = 0; i < NUM_LV1ENTRIES; i += 4) {
		unsigned int j;
		if ((base[i] == ZERO_LV2LINK) &&
			(base[i + 1] == ZERO_LV2LINK) &&
			(base[i + 2] == ZERO_LV2LINK) &&
			(base[i + 3] == ZERO_LV2LINK))
			continue;
		pr_info("LV1[%04d] %08x %08x %08x %08x\n",
			i, base[i], base[i + 1], base[i + 2], base[i + 3]);

		for (j = 0; j < 4; j++)
			if (lv1ent_page(&base[i + j]))
				sysmmu_dump_lv2_page_table(i + j,
						page_entry(&base[i + j], 0));
	}
}

void exynos_sysmmu_show_status(struct device *dev)
{
	struct device *sysmmu;
	for_each_sysmmu(dev, sysmmu) {
		struct sysmmu_drvdata *drvdata = dev_get_drvdata(sysmmu);
		unsigned int i;

		if (!is_sysmmu_active(drvdata) || !drvdata->runtime_active) {
			dev_info(sysmmu, "%s: System MMU is not active\n",
					__func__);
			continue;
		}

		pr_info("DUMPING SYSTEM MMU: %s\n", dev_name(sysmmu));

		__master_clk_enable(drvdata);

		for (i = 0; i < drvdata->nsfrs; i++) {
			/* System MMU must be enabled */
			if (sysmmu_block(drvdata->sfrbases[i]))
				dump_sysmmu_tlb_pb(drvdata->sfrbases[i]);
			else
				pr_err("!!Failed to block Sytem MMU!\n");
			sysmmu_unblock(drvdata->sfrbases[i]);

		}

		__master_clk_disable(drvdata);

		sysmmu_dump_page_table(phys_to_virt(drvdata->pgtable));
	}
}
