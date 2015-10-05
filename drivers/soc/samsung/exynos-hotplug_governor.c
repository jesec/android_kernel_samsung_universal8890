/*
 * linux/drivers/exynos/soc/samsung/exynos-hotplug_governor.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/cpumask.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/kthread.h>
#include <linux/kobject.h>
#include <linux/ktime.h>
#include <linux/hrtimer.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/cpu.h>
#include <linux/stringify.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/debugfs.h>
#include <linux/cpu_pm.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/sched.h>
#include <linux/fb.h>
#include <linux/irq_work.h>
#include <linux/workqueue.h>
#include <linux/pm_qos.h>

#include <soc/samsung/cpufreq.h>

#include <asm/atomic.h>
#include <asm/page.h>
#define CREATE_TRACE_POINTS
#include <trace/events/hotplug_governor.h>

#define DEFAULT_BOOT_ENABLE_MS (30000)		/* 30 s */

enum hpgov_event {
	/* need to add event here */
	/* ex. HPGOV_BIG_MODE_UPDATED */

	HPGOV_EVENT_END,
};

struct hpgov_attrib {
	struct kobj_attribute	enabled;

	struct attribute_group	attrib_group;
};

struct hpgov_data {
	enum hpgov_event event;
	int req_cpu_max;
	int req_cpu_min;
};

struct {
	uint32_t			enabled;
	atomic_t			cur_cpu_max;
	atomic_t			cur_cpu_min;

	struct hpgov_attrib		attrib;
	struct mutex			attrib_lock;
	struct task_struct		*task;
	struct task_struct		*hptask;
	struct irq_work			update_irq_work;
	struct hpgov_data		data;
	int				hp_state;
	wait_queue_head_t		wait_q;
	wait_queue_head_t		wait_hpq;
} exynos_hpgov;

static struct pm_qos_request hpgov_max_pm_qos;
static struct pm_qos_request hpgov_min_pm_qos;

static DEFINE_SPINLOCK(hpgov_lock);

enum {
	HP_STATE_WAITING = 0,		/* waiting for cpumask update */
	HP_STATE_SCHEDULED = 1,		/* hotplugging is scheduled */
	HP_STATE_IN_PROGRESS = 2,	/* in the process of hotplugging */
};

static void exynos_hpgov_irq_work(struct irq_work *irq_work)
{
	wake_up(&exynos_hpgov.wait_q);
}

static int exynos_hpgov_update_governor(enum hpgov_event event, int req_cpu_max, int req_cpu_min)
{
	int ret = 0;
	int cur_cpu_max = atomic_read(&exynos_hpgov.cur_cpu_max);
	int cur_cpu_min = atomic_read(&exynos_hpgov.cur_cpu_min);

	switch(event) {
	default:
		break;
	}

	if (req_cpu_max == cur_cpu_max)
		req_cpu_max = 0;

	if (req_cpu_min == cur_cpu_min)
		req_cpu_min = 0;

	if (!req_cpu_max && !req_cpu_min)
		return ret;

	trace_exynos_hpgov_governor_update(event, req_cpu_max, req_cpu_min);
	if (req_cpu_max)
		atomic_set(&exynos_hpgov.cur_cpu_max, req_cpu_max);
	if (req_cpu_min)
		atomic_set(&exynos_hpgov.cur_cpu_min, req_cpu_min);

	exynos_hpgov.hp_state = HP_STATE_SCHEDULED;
	wake_up(&exynos_hpgov.wait_hpq);

	return ret;
}

static int exynos_hpgov_do_update_governor(void *data)
{
	struct hpgov_data *pdata = (struct hpgov_data *)data;
	unsigned long flags;
	enum hpgov_event event;
	int req_cpu_max;
	int req_cpu_min;

	while (1) {
		wait_event(exynos_hpgov.wait_q, pdata->event || kthread_should_stop());
		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&hpgov_lock, flags);
		event = pdata->event;
		req_cpu_max = pdata->req_cpu_max;
		req_cpu_min = pdata->req_cpu_min;
		pdata->event = 0;
		pdata->req_cpu_max = 0;
		pdata->req_cpu_min = 0;
		spin_unlock_irqrestore(&hpgov_lock, flags);

		exynos_hpgov_update_governor(event, req_cpu_max, req_cpu_min);
	}
	return 0;
}

static int exynos_hpgov_do_hotplug(void *data)
{
	int *event = (int *)data;
	int cpu_max;
	int cpu_min;
	int last_max = 0;
	int last_min = 0;

	while (1) {
		wait_event(exynos_hpgov.wait_hpq, *event || kthread_should_stop());
		if (kthread_should_stop())
			break;

restart:
		exynos_hpgov.hp_state = HP_STATE_IN_PROGRESS;
		cpu_max = atomic_read(&exynos_hpgov.cur_cpu_max);
		cpu_min = atomic_read(&exynos_hpgov.cur_cpu_min);

		if (cpu_max != last_max) {
			pm_qos_update_request(&hpgov_max_pm_qos, cpu_max);
			last_max = cpu_max;
		}

		if (cpu_min != last_min) {
			pm_qos_update_request(&hpgov_min_pm_qos, cpu_min);
			last_min = cpu_min;
		}

		exynos_hpgov.hp_state = HP_STATE_WAITING;
		if (last_max != atomic_read(&exynos_hpgov.cur_cpu_max) ||
			last_min != atomic_read(&exynos_hpgov.cur_cpu_min))
			goto restart;
	}

	return 0;
}

static int exynos_hpgov_set_enabled(uint32_t enable)
{
	int ret = 0;
	static uint32_t last_enable;

	enable = (enable > 0) ? 1 : 0;
	if (last_enable == enable)
		return ret;

	last_enable = enable;

	if (enable) {
		exynos_hpgov.task = kthread_create(exynos_hpgov_do_update_governor,
					      &exynos_hpgov.data, "exynos_hpgov");
		if (IS_ERR(exynos_hpgov.task))
			return -EFAULT;

		kthread_bind(exynos_hpgov.task, 0);
		wake_up_process(exynos_hpgov.task);

		exynos_hpgov.hptask = kthread_create(exynos_hpgov_do_hotplug,
						&exynos_hpgov.hp_state, "exynos_hp");
		if (IS_ERR(exynos_hpgov.hptask))
			return -EFAULT;

		kthread_bind(exynos_hpgov.hptask, 0);
		wake_up_process(exynos_hpgov.hptask);

		exynos_hpgov.enabled = 1;
	} else {
		kthread_stop(exynos_hpgov.hptask);
		kthread_stop(exynos_hpgov.task);
		exynos_hpgov.enabled = 0;
	}

	return ret;
}

#define HPGOV_PARAM(_name, _param) \
static ssize_t exynos_hpgov_attr_##_name##_show(struct kobject *kobj, \
			struct kobj_attribute *attr, char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%d\n", _param); \
} \
static ssize_t exynos_hpgov_attr_##_name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
{ \
	int ret = 0; \
	uint32_t val; \
	uint32_t old_val; \
	mutex_lock(&exynos_hpgov.attrib_lock); \
	ret = kstrtouint(buf, 10, &val); \
	if (ret) { \
		pr_err("Invalid input %s for %s %d\n", \
				buf, __stringify(_name), ret);\
		return 0; \
	} \
	old_val = _param; \
	ret = exynos_hpgov_set_##_name(val); \
	if (ret) { \
		pr_err("Error %d returned when setting param %s to %d\n",\
				ret, __stringify(_name), val); \
		_param = old_val; \
	} \
	mutex_unlock(&exynos_hpgov.attrib_lock); \
	return count; \
}

#define HPGOV_RW_ATTRIB(i, _name) \
	exynos_hpgov.attrib._name.attr.name = __stringify(_name); \
	exynos_hpgov.attrib._name.attr.mode = S_IRUGO | S_IWUSR; \
	exynos_hpgov.attrib._name.show = exynos_hpgov_attr_##_name##_show; \
	exynos_hpgov.attrib._name.store = exynos_hpgov_attr_##_name##_store; \
	exynos_hpgov.attrib.attrib_group.attrs[i] = &exynos_hpgov.attrib._name.attr;

HPGOV_PARAM(enabled, exynos_hpgov.enabled);

static void hpgov_boot_enable(struct work_struct *work)
{
	exynos_hpgov_set_enabled(1);
}

static DECLARE_DELAYED_WORK(hpgov_boot_work, hpgov_boot_enable);

static int __init exynos_hpgov_init(void)
{
	int ret = 0;
	const int attr_count = 3;

	mutex_init(&exynos_hpgov.attrib_lock);
	init_waitqueue_head(&exynos_hpgov.wait_q);
	init_waitqueue_head(&exynos_hpgov.wait_hpq);
	init_irq_work(&exynos_hpgov.update_irq_work, exynos_hpgov_irq_work);

	exynos_hpgov.attrib.attrib_group.attrs =
		kzalloc(attr_count * sizeof(struct attribute *), GFP_KERNEL);
	if (!exynos_hpgov.attrib.attrib_group.attrs) {
		ret = -ENOMEM;
		goto done;
	}

	HPGOV_RW_ATTRIB(0, enabled);

	exynos_hpgov.attrib.attrib_group.name = "hotplug_governor";
	ret = sysfs_create_group(power_kobj, &exynos_hpgov.attrib.attrib_group);
	if (ret)
		pr_err("Unable to create sysfs objects :%d\n", ret);

	atomic_set(&exynos_hpgov.cur_cpu_max, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
	atomic_set(&exynos_hpgov.cur_cpu_min, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	pm_qos_add_request(&hpgov_max_pm_qos, PM_QOS_CPU_ONLINE_MAX, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
	pm_qos_add_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MIN, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	schedule_delayed_work_on(0, &hpgov_boot_work, msecs_to_jiffies(DEFAULT_BOOT_ENABLE_MS));

done:
	return ret;
}
late_initcall(exynos_hpgov_init);
