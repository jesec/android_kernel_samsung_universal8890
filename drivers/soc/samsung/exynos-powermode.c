/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS Power mode
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/of.h>

#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>

/******************************************************************************
 *                                  IDLE_IP                                   *
 ******************************************************************************/
int exynos_get_idle_ip_index(const char *name)
{
	/* To do */
	return 0;
}

void exynos_update_pd_idle_status(int index, int idle)
{
	/* To do */
}

void exynos_update_ip_idle_status(int index, int idle)
{
	/* To do */
}

/******************************************************************************
 *                          Local power gating (C2)                           *
 ******************************************************************************/
/**
 * Exynos cpuidle driver call enter_c2() and wakeup_from_c2() to handle platform
 * specific configuration to power off the cpu power domain. It handles not only
 * cpu power control, but also power mode subordinate to C2.
 */
int enter_c2(unsigned int cpu, int index)
{
	exynos_cpu.power_down(cpu);

	return index;
}

void wakeup_from_c2(unsigned int cpu, int early_wakeup)
{
	if (early_wakeup)
		exynos_cpu.power_up(cpu);
}

/******************************************************************************
 *                          Wakeup mask configuration                         *
 ******************************************************************************/
#define NUM_WAKEUP_MASK		3

#define PMU_EINT_WAKEUP_MASK	0x60C
#define PMU_WAKEUP_MASK		0x610
#define PMU_WAKEUP_MASK2	0x614
#define PMU_WAKEUP_MASK3	0x618

static unsigned int wakeup_mask[NUM_SYS_POWERDOWN][NUM_WAKEUP_MASK];

extern u64 exynos_get_eint_wake_mask(void);
static void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	u64 eintmask = exynos_get_eint_wake_mask();

	/* Set external interrupt mask */
	exynos_pmu_write(PMU_EINT_WAKEUP_MASK, (u32)eintmask);

	exynos_pmu_write(PMU_WAKEUP_MASK, wakeup_mask[mode][0]);
	exynos_pmu_write(PMU_WAKEUP_MASK2, wakeup_mask[mode][1]);
	exynos_pmu_write(PMU_WAKEUP_MASK3, wakeup_mask[mode][2]);
}

static int parsing_dt_wakeup_mask(struct device_node *np)
{
	int ret;
	unsigned int pdn_num;

	for (pdn_num = 0; pdn_num < NUM_SYS_POWERDOWN; pdn_num++) {
		ret = of_property_read_u32_index(np, "wakeup_mask",
					pdn_num, &wakeup_mask[pdn_num][0]);
		if (ret)
			return ret;

		ret = of_property_read_u32_index(np, "wakeup_mask2",
					pdn_num, &wakeup_mask[pdn_num][1]);
		if (ret)
			return ret;

		ret = of_property_read_u32_index(np, "wakeup_mask3",
					pdn_num, &wakeup_mask[pdn_num][2]);
		if (ret)
			return ret;
	}

	return 0;
}

/******************************************************************************
 *                              Driver initialized                            *
 ******************************************************************************/
static int __init dt_init_exynos_powermode(void)
{
	struct device_node *np = of_find_node_by_name(NULL, "exynos-powermode");
	int ret;

	ret = parsing_dt_wakeup_mask(np);
	if (ret)
		pr_warn("Fail to initialize the wakeup mask with err = %d\n", ret);

	return 0;
}

int __init exynos_powermode_init(void)
{
	dt_init_exynos_powermode();

	return 0;
}
arch_initcall(exynos_powermode_init);
