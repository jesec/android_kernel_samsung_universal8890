/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS - CPU PMU(Power Management Unit) support
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/smp.h>
#include <linux/regmap.h>
#include <linux/mfd/syscon.h>

#include <asm/smp_plat.h>

#include <soc/samsung/exynos-pmu.h>

/**
 * register offset value from base address
 */
#define PMU_CPU_CONFIG_BASE			0x2000
#define PMU_CPU_STATUS_BASE			0x2004
#define PMU_CPU_ADDR_OFFSET			0x80
#define CPU_LOCAL_PWR_CFG			0xF

#define PMU_NONBOOT_CLUSTER_CPUSEQ_OPTION	0x2488

#define PMU_NONCPU_STATUS_BASE			0x2404
#define PMU_L2_STATUS_BASE			0x2604
#define PMU_CLUSTER_ADDR_OFFSET			0x20
#define NONCPU_LOCAL_PWR_CFG			0xF
#define L2_LOCAL_PWR_CFG			0x7

/**
 * "pmureg" has the mapped base address of PMU(Power Management Unit)
 */
static struct regmap *pmureg;

/**
 * No driver refers the "pmureg" directly, through the only exported API.
 */
int exynos_pmu_read(unsigned int offset, unsigned int *val)
{
	return regmap_read(pmureg, offset, val);
}

int exynos_pmu_write(unsigned int offset, unsigned int val)
{
	return regmap_write(pmureg, offset, val);
}

int exynos_pmu_update(unsigned int offset, unsigned int mask, unsigned int val)
{
	return regmap_update_bits(pmureg, offset, mask, val);
}

EXPORT_SYMBOL(exynos_pmu_read);
EXPORT_SYMBOL(exynos_pmu_write);
EXPORT_SYMBOL(exynos_pmu_update);

/**
 * CPU power control registers in PMU are arranged at regular intervals
 * (interval = 0x80). pmu_cpu_offset calculates how far cpu is from address
 * of first cpu. This expression is based on cpu and cluster id in MPIDR,
 * refer below.

 * cpu address offset : ((cluster id << 2) | (cpu id)) * 0x80
 */
#define pmu_cpu_offset(mpidr)			\
	(( MPIDR_AFFINITY_LEVEL(mpidr, 1) << 2	\
	 | MPIDR_AFFINITY_LEVEL(mpidr, 0))	\
	 * PMU_CPU_ADDR_OFFSET)

static void exynos_cpu_up(unsigned int cpu)
{
	unsigned int mpidr = cpu_logical_map(cpu);
	unsigned int offset;

	offset = pmu_cpu_offset(mpidr);
	regmap_update_bits(pmureg, PMU_CPU_CONFIG_BASE + offset,
			CPU_LOCAL_PWR_CFG, CPU_LOCAL_PWR_CFG);
}

static void exynos_cpu_down(unsigned int cpu)
{
	unsigned int mpidr = cpu_logical_map(cpu);
	unsigned int offset;

	offset = pmu_cpu_offset(mpidr);
	regmap_update_bits(pmureg, PMU_CPU_CONFIG_BASE + offset,
			CPU_LOCAL_PWR_CFG, 0);
}

static int exynos_cpu_state(unsigned int cpu)
{
	unsigned int mpidr = cpu_logical_map(cpu);
	unsigned int offset, val;

	offset = pmu_cpu_offset(mpidr);
	regmap_read(pmureg, PMU_CPU_STATUS_BASE + offset, &val);

	return ((val & CPU_LOCAL_PWR_CFG) == CPU_LOCAL_PWR_CFG);
}

static int exynos_cluster_state(unsigned int cluster)
{
	unsigned int noncpu_stat, l2_stat;
	unsigned int offset;

	offset = cluster * PMU_CLUSTER_ADDR_OFFSET;

	regmap_read(pmureg, PMU_NONCPU_STATUS_BASE + offset, &noncpu_stat);
	regmap_read(pmureg, PMU_L2_STATUS_BASE + offset, &l2_stat);

	return ((l2_stat & L2_LOCAL_PWR_CFG) == L2_LOCAL_PWR_CFG) &&
		((noncpu_stat & NONCPU_LOCAL_PWR_CFG) == NONCPU_LOCAL_PWR_CFG);
}

struct exynos_cpu_power_ops exynos_cpu = {
	.power_up = exynos_cpu_up,
	.power_down = exynos_cpu_down,
	.power_state = exynos_cpu_state,
	.cluster_state = exynos_cluster_state,
};

/**
 * While Exynos with multi cluster supports to shutdown down both cluster,
 * there is no benefit in boot cluster. So Exynos-PMU driver supports
 * only non-boot cluster down.
 */
void exynos_cpu_sequencer_ctrl(int enable)
{
	regmap_update_bits(pmureg, PMU_NONBOOT_CLUSTER_CPUSEQ_OPTION, 1, enable);
}

char *pmu_syscon_name = "105c0000.system-controller";

int __init exynos_pmu_init(void)
{
	int ret;

	pmureg = syscon_regmap_lookup_by_pdevname(pmu_syscon_name);
	if (IS_ERR(pmureg)) {
		ret = PTR_ERR(pmureg);
		pr_err("Fail to get regmap of PMU with err %d\n", ret);
		return ret;
	}

	return 0;
}
subsys_initcall(exynos_pmu_init);
