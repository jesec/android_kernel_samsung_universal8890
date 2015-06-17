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
#include <linux/tick.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

#include <asm/smp_plat.h>
#include <asm/psci.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>

#include "pwrcal/pwrcal.h"

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
/*
 * If cpu is powered down, "c2_state_mask" is set. On the contrary, cpu is
 * powered on, "c2_state_mask" is cleard. To keep coherency of c2_state_mask,
 * use the spinlock, "c2_lock". In Exynos, it supports C2 subordinate power
 * mode, CPD.
 *
 * - CPD (Cluster Power Down)
 * All cpus in a cluster are set c2_state_mask, and these cpus have enough
 * idle time which is longer than cpd_residency, cluster can be powered off.
 */

static unsigned int cpd_residency = UINT_MAX;

static DEFINE_SPINLOCK(c2_lock);
static struct cpumask c2_state_mask;

static void update_c2_state(bool down, unsigned int cpu)
{
	if (down)
		cpumask_set_cpu(cpu, &c2_state_mask);
	else
		cpumask_clear_cpu(cpu, &c2_state_mask);
}

static s64 get_next_event_time_us(unsigned int cpu)
{
	struct clock_event_device *dev = per_cpu(tick_cpu_device, cpu).evtdev;

	return ktime_to_us(ktime_sub(dev->next_event, ktime_get()));
}

static int is_cpus_busy(unsigned int target_residency,
				const struct cpumask *mask)
{
	int cpu;

	/*
	 * If there is even one cpu in "mask" which has the smaller idle time
	 * than "target_residency", it returns -EBUSY.
	 */
	for_each_cpu_and(cpu, cpu_possible_mask, mask) {
		if (!cpumask_test_cpu(cpu, &c2_state_mask))
			return -EBUSY;

		/*
		 * Compare cpu's next event time and target_residency.
		 * Next event time means idle time.
		 */
		if (get_next_event_time_us(cpu) < target_residency)
			return -EBUSY;
	}

	return 0;
}

/**
 * cpd_blocked prevents to power down the cluster while cpu frequency is
 * changed. Before frequency changing, cpufreq driver call block_cpd() to
 * block cluster power down. After finishing changing frequency, call
 * release_cpd() to allow cluster power down again.
 */
static int cpd_blocked;

void block_cpd(void)
{
	cpd_blocked = true;
}

void release_cpd(void)
{
	cpd_blocked = false;
}

static int is_cpd_available(unsigned int cpu)
{
	if (cpd_blocked)
		return false;

	if (is_cpus_busy(cpd_residency, cpu_coregroup_mask(cpu)))
		return false;

	return true;
}

static int get_cluster_id(unsigned int cpu)
{
	return MPIDR_AFFINITY_LEVEL(cpu_logical_map(cpu), 1);
}

/**
 * cluster_idle_state shows whether cluster is in idle or not.
 *
 * check_cluster_idle_state() : Show cluster idle state.
 * 		If it returns true, cluster is in idle state.
 * update_cluster_idle_state() : Update cluster idle state.
 */
#define CLUSTER_TYPE_MAX	2
static int cluster_idle_state[CLUSTER_TYPE_MAX];

int check_cluster_idle_state(int cpu)
{
	return cluster_idle_state[get_cluster_id(cpu)];
}

static void update_cluster_idle_state(int idle, int cpu)
{
	cluster_idle_state[get_cluster_id(cpu)] = idle;
}

/**
 * Exynos cpuidle driver call enter_c2() and wakeup_from_c2() to handle platform
 * specific configuration to power off the cpu power domain. It handles not only
 * cpu power control, but also power mode subordinate to C2.
 */
int enter_c2(unsigned int cpu, int index)
{
	exynos_cpu.power_down(cpu);

	spin_lock(&c2_lock);
	update_c2_state(true, cpu);

	/*
	 * Below sequence determines whether to power down the cluster or not.
	 * If idle time of cpu is not enough, go out of this function.
	 */
	if (get_next_event_time_us(cpu) < cpd_residency)
		goto out;

	/*
	 * Power down of LITTLE cluster have nothing to gain power consumption,
	 * so it is not supported. For you reference, cluster id "1" indicates LITTLE.
	 */
	if (get_cluster_id(cpu))
		goto out;

	if (is_cpd_available(cpu)) {
		exynos_cpu_sequencer_ctrl(true);
		update_cluster_idle_state(true, cpu);

		index = PSCI_CLUSTER_SLEEP;
	}

out:
	spin_unlock(&c2_lock);

	return index;
}

void wakeup_from_c2(unsigned int cpu, int early_wakeup)
{
	if (early_wakeup)
		exynos_cpu.power_up(cpu);

	spin_lock(&c2_lock);

	if (check_cluster_idle_state(cpu)) {
		exynos_cpu_sequencer_ctrl(false);
		update_cluster_idle_state(false, cpu);
	}

	update_c2_state(false, cpu);

	spin_unlock(&c2_lock);
}

/**
 * powermode_attr_read() / show_##file_name() -
 * print out power mode information
 *
 * powermode_attr_write() / store_##file_name() -
 * sysfs write access
 */
#define show_one(file_name, object)			\
static ssize_t show_##file_name(struct kobject *kobj,	\
	struct kobj_attribute *attr, char *buf)		\
{							\
	return snprintf(buf, 3, "%d\n", object);	\
}

#define store_one(file_name, object)			\
static ssize_t store_##file_name(struct kobject *kobj,	\
	struct kobj_attribute *attr, const char *buf,	\
	size_t count)					\
{							\
	int input;					\
							\
	if (!sscanf(buf, "%1d", &input))		\
		return -EINVAL;				\
							\
	object = !!input;				\
							\
	return count;					\
}

#define attr_rw(_name)					\
static struct kobj_attribute _name =			\
__ATTR(_name, 0644, show_##_name, store_##_name)


show_one(blocking_cpd, cpd_blocked);
store_one(blocking_cpd, cpd_blocked);

attr_rw(blocking_cpd);

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
 *                           System power down mode                           *
 ******************************************************************************/
void exynos_prepare_sys_powerdown(enum sys_powerdown mode)
{
	/*
	 * exynos_prepare_sys_powerdown() is called by only cpu0.
	 */
	unsigned int cpu = 0;

	exynos_set_wakeupmask(mode);

	cal_pm_enter(mode);

	switch (mode) {
	case SYS_ALPA:
		exynos_pm_lpa_enter();
	case SYS_AFTR:
		exynos_cpu.power_down(cpu);
		exynos_cpu.power_down(cpu);
		break;
	default:
		break;
	}
}

void exynos_wakeup_sys_powerdown(enum sys_powerdown mode, bool early_wakeup)
{
	/*
	 * exynos_wakeup_sys_powerdown() is called by only cpu0.
	 */
	unsigned int cpu = 0;

	if (early_wakeup)
		cal_pm_earlywakeup(mode);
	else
		cal_pm_exit(mode);

	switch (mode) {
	case SYS_ALPA:
		exynos_pm_lpa_exit();
	case SYS_AFTR:
		if (early_wakeup)
			exynos_cpu.power_up(cpu);
		break;
	default:
		break;
	}
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

	if (of_property_read_u32(np, "cpd_residency", &cpd_residency))
		pr_warn("No matching property: cpd_residency\n");

	return 0;
}

int __init exynos_powermode_init(void)
{
	dt_init_exynos_powermode();

	if (sysfs_create_file(power_kobj, &blocking_cpd.attr))
		pr_err("%s: failed to create sysfs to control CPD\n", __func__);

	return 0;
}
arch_initcall(exynos_powermode_init);
