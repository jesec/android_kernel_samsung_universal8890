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
#include <linux/slab.h>

#include <asm/smp_plat.h>
#include <asm/psci.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>

#include "pwrcal/pwrcal.h"

#define NUM_WAKEUP_MASK		3

struct exynos_powermode_info {
	unsigned int	cpd_residency;		/* target residency of cpd */
	unsigned int	sicd_residency;		/* target residency of sicd */

	struct cpumask	c2_mask;		/* per cpu c2 status */

	/*
	 * cpd_blocked prevents to power down the cluster. It used by cpufreq
	 * driver using block_cpd() and release_cpd() or usespace using sysfs
	 * interface.
	 */
	int		cpd_blocked;

	/*
	 * sicd_enabled is changed by sysfs interface. It is just for
	 * development convenience because console does not work during
	 * SICD mode.
	 */
	int		sicd_enabled;
	int		sicd_entered;

	/*
	 * During intializing time, wakeup_mask and idle_ip_mask is intialized
	 * with device tree data. These are used when system enter system
	 * power down mode.
	 */
	unsigned int	wakeup_mask[NUM_SYS_POWERDOWN][NUM_WAKEUP_MASK];
	int		idle_ip_mask[NUM_SYS_POWERDOWN][NUM_IDLE_IP];
};

static struct exynos_powermode_info *pm_info;

/******************************************************************************
 *                                  IDLE_IP                                   *
 ******************************************************************************/
#define PMU_IDLE_IP_BASE		0x03E0
#define PMU_IDLE_IP_MASK_BASE		0x03F0
#define PMU_IDLE_IP(x)			(PMU_IDLE_IP_BASE + (x * 0x4))
#define PMU_IDLE_IP_MASK(x)		(PMU_IDLE_IP_MASK_BASE + (x * 0x4))

static int exynos_check_idle_ip_stat(int mode, int index)
{
	unsigned int val, mask;

	exynos_pmu_read(PMU_IDLE_IP(index), &val);
	mask = pm_info->idle_ip_mask[mode][index];

	return (val & ~mask) == ~mask ? 0 : -EBUSY;
}

static DEFINE_SPINLOCK(idle_ip_mask_lock);
static void exynos_set_idle_ip_mask(enum sys_powerdown mode)
{
	int i;
	unsigned long flags;

	spin_lock_irqsave(&idle_ip_mask_lock, flags);
	for_each_idle_ip(i)
		exynos_pmu_write(PMU_IDLE_IP_MASK(i), pm_info->idle_ip_mask[mode][i]);
	spin_unlock_irqrestore(&idle_ip_mask_lock, flags);
}

static void idle_ip_unmask(int mode, int idle_ip, int index)
{
	unsigned long flags;

	spin_lock_irqsave(&idle_ip_mask_lock, flags);
	pm_info->idle_ip_mask[mode][idle_ip] &= ~(0x1 << index);
	spin_unlock_irqrestore(&idle_ip_mask_lock, flags);
}

static void exynos_create_idle_ip_mask(int idle_ip, int ip_index)
{
	struct device_node *root = of_find_node_by_path("/exynos-powermode/idle_ip_mask");
	struct device_node *node;
	char prop_ref_mask[20] = "ref-mask-idle-ip";
	char buf[2];

	snprintf(buf, 2, "%d", idle_ip);
	strcat(prop_ref_mask, buf);

	for_each_child_of_node(root, node) {
		int mode, ref_mask;
		const __be32 *val;

		if (of_property_read_u32(node, "mode-index", &mode))
			continue;

		val = of_get_property(node, prop_ref_mask, NULL);
		if (val) {
			ref_mask = be32_to_cpup(val);
			if (ref_mask & (0x1 << ip_index))
				idle_ip_unmask(mode, idle_ip, ip_index);
		}
	}
}

int exynos_get_idle_ip_index(const char *ip_name)
{
	struct device_node *np = of_find_node_by_name(NULL, "exynos-powermode");
	int ip_index, i;

	for_each_idle_ip(i) {
		char prop_idle_ip[10] = "idle-ip";
		char buf[2];

		snprintf(buf, 2, "%d", i);
		strcat(prop_idle_ip, buf);

		ip_index = of_property_match_string(np, prop_idle_ip, ip_name);
		if (ip_index >= 0) {
			/**
			 * If it successes to find IP in idle_ip list, we set
			 * corresponding bit in idle_ip mask.
			 */
			exynos_create_idle_ip_mask(i, ip_index);
			goto out;
		}
	}

	pr_err("%s: Fail to find %s in idle-ip list with err %d\n",
					__func__, ip_name, ip_index);

out:
	return ip_index;
}

static DEFINE_SPINLOCK(ip_idle_lock);
void exynos_update_ip_idle_status(int index, int idle)
{
	unsigned long flags;

	spin_lock_irqsave(&ip_idle_lock, flags);
	exynos_pmu_update(PMU_IDLE_IP(0), 1 << index, idle << index);
	spin_unlock_irqrestore(&ip_idle_lock, flags);

	return;
}

static DEFINE_SPINLOCK(pd_idle_lock);
void exynos_update_pd_idle_status(int index, int idle)
{
	unsigned long flags;

	spin_lock_irqsave(&pd_idle_lock, flags);
	exynos_pmu_update(PMU_IDLE_IP(1), 1 << index, idle << index);
	spin_unlock_irqrestore(&pd_idle_lock, flags);

	return;
}

/******************************************************************************
 *                          Local power gating (C2)                           *
 ******************************************************************************/
/**
 * If cpu is powered down, c2_mask in struct exynos_powermode_info is set. On
 * the contrary, cpu is powered on, c2_mask is cleard. To keep coherency of
 * c2_mask, use the spinlock, c2_lock. In Exynos, it supports C2 subordinate
 * power mode, CPD.
 *
 * - CPD (Cluster Power Down)
 * All cpus in a cluster are set c2_mask, and these cpus have enough idle
 * time which is longer than cpd_residency, cluster can be powered off.
 *
 * SICD (System Idle Clock Down) : All cpus are set c2_mask and these cpus
 * have enough idle time which is longer than sicd_residency, AP can be put
 * into SICD. During SICD, no one access to DRAM.
 */

static DEFINE_SPINLOCK(c2_lock);

static void update_c2_state(bool down, unsigned int cpu)
{
	if (down)
		cpumask_set_cpu(cpu, &pm_info->c2_mask);
	else
		cpumask_clear_cpu(cpu, &pm_info->c2_mask);
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
		if (!cpumask_test_cpu(cpu, &pm_info->c2_mask))
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
 * pm_info->cpd_blocked prevents to power down the cluster while cpu
 * frequency is changed. Before frequency changing, cpufreq driver call
 * block_cpd() to block cluster power down. After finishing changing
 * frequency, call release_cpd() to allow cluster power down again.
 */
void block_cpd(void)
{
	pm_info->cpd_blocked = true;
}

void release_cpd(void)
{
	pm_info->cpd_blocked = false;
}

static int is_cpd_available(unsigned int cpu)
{
	if (pm_info->cpd_blocked)
		return false;

	if (is_cpus_busy(pm_info->cpd_residency, cpu_coregroup_mask(cpu)))
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
 * If AP put into SICD, console cannot work normally. For development,
 * support sysfs to enable or disable SICD. Refer below :
 *
 * echo 0/1 > /sys/power/sicd (0:disable, 1:enable)
 */
static int is_sicd_available(void)
{
	int index;

	if (!pm_info->sicd_enabled)
		return false;

	if (is_cpus_busy(pm_info->sicd_residency, cpu_possible_mask))
		return false;

	if (!exynos_check_cp_status())
		return false;

	for_each_idle_ip(index)
		if (exynos_check_idle_ip_stat(SYS_SICD, index))
			return false;

	return true;
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
	 * Below sequence determines whether to power down the cluster/enter SICD
	 * or not. If idle time of cpu is not enough, go out of this function.
	 */
	if (get_next_event_time_us(cpu) <
			min(pm_info->cpd_residency, pm_info->sicd_residency))
		goto out;

	/*
	 * Power down of LITTLE cluster have nothing to gain power consumption,
	 * so it is not supported. For you reference, cluster id "1" indicates LITTLE.
	 */
	if (get_cluster_id(cpu))
		goto system_idle_clock_down;

	if (is_cpd_available(cpu)) {
		exynos_cpu_sequencer_ctrl(true);
		update_cluster_idle_state(true, cpu);

		index = PSCI_CLUSTER_SLEEP;
	}

system_idle_clock_down:
	if (is_sicd_available()) {
		if (check_cluster_idle_state(cpu)) {
			exynos_prepare_sys_powerdown(SYS_SICD_CPD);
			index = PSCI_SYSTEM_IDLE_CLUSTER_SLEEP;
		} else {
			exynos_prepare_sys_powerdown(SYS_SICD);
			index = PSCI_SYSTEM_IDLE;
		}

		s3c24xx_serial_fifo_wait();
		pm_info->sicd_entered = true;
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

	if (pm_info->sicd_entered) {
		exynos_wakeup_sys_powerdown(SYS_SICD, early_wakeup);
		pm_info->sicd_entered = false;
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
	return snprintf(buf, 3, "%d\n",			\
				pm_info->object);	\
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
	pm_info->object = !!input;				\
							\
	return count;					\
}

#define attr_rw(_name)					\
static struct kobj_attribute _name =			\
__ATTR(_name, 0644, show_##_name, store_##_name)


show_one(blocking_cpd, cpd_blocked);
show_one(sicd, sicd_enabled);
store_one(blocking_cpd, cpd_blocked);
store_one(sicd, sicd_enabled);

attr_rw(blocking_cpd);
attr_rw(sicd);

/******************************************************************************
 *                          Wakeup mask configuration                         *
 ******************************************************************************/
#define PMU_EINT_WAKEUP_MASK	0x60C
#define PMU_WAKEUP_MASK		0x610
#define PMU_WAKEUP_MASK2	0x614
#define PMU_WAKEUP_MASK3	0x618

static void exynos_set_wakeupmask(enum sys_powerdown mode)
{
	u64 eintmask = exynos_get_eint_wake_mask();

	/* Set external interrupt mask */
	exynos_pmu_write(PMU_EINT_WAKEUP_MASK, (u32)eintmask);

	exynos_pmu_write(PMU_WAKEUP_MASK, pm_info->wakeup_mask[mode][0]);
	exynos_pmu_write(PMU_WAKEUP_MASK2, pm_info->wakeup_mask[mode][1]);
	exynos_pmu_write(PMU_WAKEUP_MASK3, pm_info->wakeup_mask[mode][2]);
}

static int parsing_dt_wakeup_mask(struct device_node *np)
{
	int ret;
	unsigned int pdn_num;

	for_each_syspower_mode(pdn_num) {
		ret = of_property_read_u32_index(np, "wakeup_mask",
				pdn_num, &pm_info->wakeup_mask[pdn_num][0]);
		if (ret)
			return ret;

		ret = of_property_read_u32_index(np, "wakeup_mask2",
				pdn_num, &pm_info->wakeup_mask[pdn_num][1]);
		if (ret)
			return ret;

		ret = of_property_read_u32_index(np, "wakeup_mask3",
				pdn_num, &pm_info->wakeup_mask[pdn_num][2]);
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

	exynos_set_idle_ip_mask(mode);
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

/**
 * To determine which power mode system enter, check clock or power
 * registers and other devices by notifier.
 */
int determine_lpm(void)
{
	int index;

	if (!exynos_check_cp_status())
		return SYS_AFTR;

	for_each_idle_ip(index) {
		if (exynos_check_idle_ip_stat(SYS_ALPA, index))
			return SYS_AFTR;
	}

	return SYS_ALPA;
}

void exynos_prepare_cp_call(void)
{
	exynos_set_idle_ip_mask(SYS_SLEEP);
	exynos_set_wakeupmask(SYS_SLEEP);

	cal_pm_enter(SYS_ALPA);
}

void exynos_wakeup_cp_call(bool early_wakeup)
{
	if (early_wakeup)
		cal_pm_earlywakeup(SYS_ALPA);
	else
		cal_pm_exit(SYS_ALPA);
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

	if (of_property_read_u32(np, "cpd_residency", &pm_info->cpd_residency))
		pr_warn("No matching property: cpd_residency\n");

	if (of_property_read_u32(np, "sicd_residency", &pm_info->sicd_residency))
		pr_warn("No matching property: sicd_residency\n");

	return 0;
}

int __init exynos_powermode_init(void)
{
	int mode, index;

	pm_info = kzalloc(sizeof(struct exynos_powermode_info), GFP_KERNEL);
	if (pm_info == NULL) {
		pr_err("%s: failed to allocate exynos_powermode_info\n", __func__);
		return -ENOMEM;
	}

	pm_info->sicd_enabled = true;

	dt_init_exynos_powermode();

	for_each_syspower_mode(mode)
		for_each_idle_ip(index)
			pm_info->idle_ip_mask[mode][index] = 0xFFFFFFFF;

	if (sysfs_create_file(power_kobj, &blocking_cpd.attr))
		pr_err("%s: failed to create sysfs to control CPD\n", __func__);

	if (sysfs_create_file(power_kobj, &sicd.attr))
		pr_err("%s: failed to create sysfs to control CPD\n", __func__);

	return 0;
}
arch_initcall(exynos_powermode_init);
