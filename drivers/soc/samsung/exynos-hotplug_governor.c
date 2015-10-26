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

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/fb.h>

#include <soc/samsung/exynos-cpu_hotplug.h>

#define CREATE_TRACE_POINTS
#include <trace/events/hotplug_governor.h>

#define DEFAULT_DUAL_CHANGE_MS (15)		/* 15 ms */
#define DEFAULT_BOOT_ENABLE_MS (30000)		/* 30 s */
#define RETRY_BOOT_ENABLE_MS (100)		/* 100 ms */

enum hpgov_event {
	HPGOV_SLACK_TIMER_EXPIRED = 1,	/* slack timer expired */
	HPGOV_BIG_MODE_UPDATED = 2,	/* dual/quad mode updated */
	HPGOV_LCD_STATUS_UPDATED = 3,	/* lcd_status updated */
};

struct hpgov_attrib {
	struct kobj_attribute	enabled;
	struct kobj_attribute	dual_change_ms;

	struct attribute_group	attrib_group;
};

struct hpgov_data {
	enum hpgov_event event;
	int req_cpu_max;
	int req_cpu_min;
};

struct {
	uint32_t			enabled;
	int				cur_cpu_max;
	int				cur_cpu_min;
	long				use_fast_hp;

	bool				lcd_on;

	uint32_t			dual_change_ms;
	struct hpgov_attrib		attrib;
	struct mutex			attrib_lock;
	struct task_struct		*task;
	struct task_struct		*hptask;
	struct hrtimer			slack_timer;
	struct irq_work			update_irq_work;
	struct irq_work			start_slack_timer_irq_work;
	struct hpgov_data		data;
	int				hp_state;
	wait_queue_head_t		wait_q;
	wait_queue_head_t		wait_hpq;

	int				boost_cnt;
	int				delayed_boost_cnt;
} exynos_hpgov;

static struct pm_qos_request hpgov_max_pm_qos;
static struct pm_qos_request hpgov_min_pm_qos;

static DEFINE_SPINLOCK(hpgov_lock);
static DEFINE_SPINLOCK(hpgov_boost_cnt_lock);

enum {
	HP_STATE_WAITING = 0,		/* waiting for cpumask update */
	HP_STATE_SCHEDULED = 1,		/* hotplugging is scheduled */
	HP_STATE_IN_PROGRESS = 2,	/* in the process of hotplugging */
};

enum {
	BIG_DUAL_MODE,
	BIG_QUAD_MODE,
	BIG_OFF_MODE,
};

static inline bool hpgov_lcd_on_status(void)
{
	bool ret;
	unsigned long flags;

	spin_lock_irqsave(&hpgov_lock, flags);
	ret = exynos_hpgov.lcd_on;
	spin_unlock_irqrestore(&hpgov_lock, flags);

	return ret;
}

static int start_slack_timer(void)
{
	int ret;

	ktime_t curr_time = ktime_get();

	if (!exynos_hpgov.enabled || !hpgov_lcd_on_status())
		return 0;

	hrtimer_cancel(&exynos_hpgov.slack_timer);
	ret = hrtimer_start(&exynos_hpgov.slack_timer,
		ktime_add(curr_time, ktime_set(0,
			exynos_hpgov.dual_change_ms * NSEC_PER_MSEC)),
			HRTIMER_MODE_PINNED);
	if (ret)
		pr_err("Failed to register slack timer %d\n", ret);

	return ret;
}

static inline int slack_timer_is_queued(void)
{
	return hrtimer_is_queued(&exynos_hpgov.slack_timer);
}

static enum hrtimer_restart exynos_hpgov_slack_timer(struct hrtimer *timer)
{
	unsigned long flags;

	if (exynos_hpgov.boost_cnt || exynos_hpgov.delayed_boost_cnt)
		goto out;

	spin_lock_irqsave(&hpgov_lock, flags);
	exynos_hpgov.data.event = HPGOV_SLACK_TIMER_EXPIRED;
	exynos_hpgov.data.req_cpu_min = 6;
	spin_unlock_irqrestore(&hpgov_lock, flags);

	wake_up(&exynos_hpgov.wait_q);
out:
	return HRTIMER_NORESTART;
}
static void exynos_hpgov_lcd_status_update(int big_mode)
{
	unsigned long flags;

	spin_lock_irqsave(&hpgov_lock, flags);
	exynos_hpgov.data.event = HPGOV_LCD_STATUS_UPDATED;
	if (big_mode == BIG_OFF_MODE) {
		exynos_hpgov.data.req_cpu_min = 4;
		exynos_hpgov.lcd_on = false;
	} else {
		exynos_hpgov.lcd_on = true;
		exynos_hpgov.data.req_cpu_min = 6;
	}
	spin_unlock_irqrestore(&hpgov_lock, flags);

	if (!exynos_hpgov.enabled)
		return;

	irq_work_queue(&exynos_hpgov.update_irq_work);
}

static void exynos_hpgov_big_mode_update(int big_mode)
{
	unsigned long flags;

	if (!exynos_hpgov.enabled || !hpgov_lcd_on_status())
		return;

	spin_lock_irqsave(&hpgov_lock, flags);
	exynos_hpgov.data.event = HPGOV_BIG_MODE_UPDATED;
	if (big_mode == BIG_QUAD_MODE)
		exynos_hpgov.data.req_cpu_min = 8;
	else
		exynos_hpgov.data.req_cpu_min = 6;
	spin_unlock_irqrestore(&hpgov_lock, flags);

	irq_work_queue(&exynos_hpgov.update_irq_work);
}

static void exynos_hpgov_irq_work(struct irq_work *irq_work)
{
	wake_up(&exynos_hpgov.wait_q);
}

static void exynos_hpgov_slack_timer_start(void)
{
	if (!exynos_hpgov.enabled)
		return;

	irq_work_queue(&exynos_hpgov.start_slack_timer_irq_work);
}

static void slack_timer_irq_work(struct irq_work *irq_work)
{
	start_slack_timer();
}

void inc_boost_req_count(bool delayed_boost)
{
	unsigned long flags;
	int boost_cnt;
	int delayed_boost_cnt;

	spin_lock_irqsave(&hpgov_boost_cnt_lock, flags);
	if (delayed_boost) {
		delayed_boost_cnt = ++exynos_hpgov.delayed_boost_cnt;
		boost_cnt = exynos_hpgov.boost_cnt;
	} else {
		boost_cnt = ++exynos_hpgov.boost_cnt;
		delayed_boost_cnt = exynos_hpgov.delayed_boost_cnt;
	}
	spin_unlock_irqrestore(&hpgov_boost_cnt_lock, flags);

	if (boost_cnt + delayed_boost_cnt == 1)
		exynos_hpgov_big_mode_update(BIG_QUAD_MODE);
}

void dec_boost_req_count(bool delayed_boost)
{
	unsigned long flags;
	int boost_cnt;
	int delayed_boost_cnt;

	spin_lock_irqsave(&hpgov_boost_cnt_lock, flags);
	if (delayed_boost) {
		delayed_boost_cnt = --exynos_hpgov.delayed_boost_cnt;
		boost_cnt = exynos_hpgov.boost_cnt;
	} else {
		boost_cnt = --exynos_hpgov.boost_cnt;
		delayed_boost_cnt = exynos_hpgov.delayed_boost_cnt;
	}

	if (delayed_boost_cnt == 0 && delayed_boost) {
		exynos_hpgov_slack_timer_start();
		spin_unlock_irqrestore(&hpgov_boost_cnt_lock, flags);
	} else if (delayed_boost_cnt + boost_cnt == 0 && !slack_timer_is_queued()) {
		spin_unlock_irqrestore(&hpgov_boost_cnt_lock, flags);
		exynos_hpgov_big_mode_update(BIG_DUAL_MODE);
	} else {
		spin_unlock_irqrestore(&hpgov_boost_cnt_lock, flags);
	}
}

static int exynos_hpgov_update_governor(enum hpgov_event event, int req_cpu_max, int req_cpu_min)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&hpgov_lock, flags);

	switch(event) {
	case HPGOV_BIG_MODE_UPDATED:
	case HPGOV_SLACK_TIMER_EXPIRED:
		exynos_hpgov.use_fast_hp = 1;
		break;
	case HPGOV_LCD_STATUS_UPDATED:
		exynos_hpgov.use_fast_hp = 0;
	default:
		break;
	}

	if (req_cpu_max == exynos_hpgov.cur_cpu_max)
		req_cpu_max = 0;

	if (req_cpu_min == exynos_hpgov.cur_cpu_min)
		req_cpu_min = 0;

	if (!req_cpu_max && !req_cpu_min) {
		spin_unlock_irqrestore(&hpgov_lock, flags);
		return ret;
	}

	trace_exynos_hpgov_governor_update(event, req_cpu_max, req_cpu_min);
	if (req_cpu_max)
		exynos_hpgov.cur_cpu_max = req_cpu_max;
	if (req_cpu_min)
		exynos_hpgov.cur_cpu_min = req_cpu_min;

	spin_unlock_irqrestore(&hpgov_lock, flags);

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
	unsigned long flags;

	int cpu_max;
	int cpu_min;
	long use_fast_hp;
	int last_max = 0;
	int last_min = 0;

	while (1) {
		wait_event(exynos_hpgov.wait_hpq, *event || kthread_should_stop());
		if (kthread_should_stop())
			break;

restart:
		exynos_hpgov.hp_state = HP_STATE_IN_PROGRESS;

		spin_lock_irqsave(&hpgov_lock, flags);
		cpu_max = exynos_hpgov.cur_cpu_max;
		cpu_min = exynos_hpgov.cur_cpu_min;
		use_fast_hp = exynos_hpgov.use_fast_hp;
		spin_unlock_irqrestore(&hpgov_lock, flags);

		if (cpu_max != last_max) {
			pm_qos_update_request_param(&hpgov_max_pm_qos,
						cpu_max, (void *)use_fast_hp);
			last_max = cpu_max;
		}

		if (cpu_min != last_min) {
			pm_qos_update_request_param(&hpgov_min_pm_qos,
						cpu_min, (void *)use_fast_hp);
			last_min = cpu_min;
		}

		exynos_hpgov.hp_state = HP_STATE_WAITING;
		if (last_max != exynos_hpgov.cur_cpu_max ||
			last_min != exynos_hpgov.cur_cpu_min)
			goto restart;
	}

	return 0;
}

static int exynos_hpgov_set_enabled(uint32_t enable)
{
	int ret = 0;
	static uint32_t last_enable = 0;

	enable = (enable > 0) ? 1 : 0;
	if (last_enable == enable)
		return ret;

	last_enable = enable;

	if (enable) {
		exynos_hpgov.task = kthread_create(exynos_hpgov_do_update_governor,
					      &exynos_hpgov.data, "exynos_hpgov");
		if (IS_ERR(exynos_hpgov.task))
			return -EFAULT;

		set_user_nice(exynos_hpgov.task, MIN_NICE);
		kthread_bind(exynos_hpgov.task, 0);
		wake_up_process(exynos_hpgov.task);

		exynos_hpgov.hptask = kthread_create(exynos_hpgov_do_hotplug,
						&exynos_hpgov.hp_state, "exynos_hp");
		if (IS_ERR(exynos_hpgov.hptask)) {
			kthread_stop(exynos_hpgov.task);
			return -EFAULT;
		}

		set_user_nice(exynos_hpgov.hptask, MIN_NICE);
		kthread_bind(exynos_hpgov.hptask, 0);
		wake_up_process(exynos_hpgov.hptask);

		exynos_hpgov.enabled = 1;
		smp_wmb();
		start_slack_timer();
	} else {
		kthread_stop(exynos_hpgov.hptask);
		kthread_stop(exynos_hpgov.task);

		exynos_hpgov.enabled = 0;
		smp_wmb();

		pm_qos_update_request(&hpgov_max_pm_qos, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
		pm_qos_update_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
		exynos_hpgov.cur_cpu_max = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
		exynos_hpgov.cur_cpu_min = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
	}

	return ret;
}

int hpgov_default_level(void)
{
	/* FIXME: need to use information from cpufreq */
	return 2;
}

static int exynos_hpgov_set_dual_change_ms(uint32_t val)
{
	exynos_hpgov.dual_change_ms = val;
	return 0;
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
HPGOV_PARAM(dual_change_ms, exynos_hpgov.dual_change_ms);

static void hpgov_boot_enable(struct work_struct *work);
static DECLARE_DELAYED_WORK(hpgov_boot_work, hpgov_boot_enable);
static void hpgov_boot_enable(struct work_struct *work)
{
	if (exynos_hpgov_set_enabled(1))
		schedule_delayed_work_on(0, &hpgov_boot_work, msecs_to_jiffies(RETRY_BOOT_ENABLE_MS));
}

static int exynos_hpgov_fb_notifier(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	unsigned int blank;
	int ret = NOTIFY_OK;

	if (val != FB_EVENT_BLANK && val != FB_R_EARLY_EVENT_BLANK)
		return 0;

	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (info->node)
		return ret;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		pr_info("%s: LCD is off\n", __func__);
		exynos_hpgov_lcd_status_update(BIG_OFF_MODE);
		hrtimer_cancel(&exynos_hpgov.slack_timer);
		break;
	case FB_BLANK_UNBLANK:
		pr_info("%s: LCD is on\n", __func__);
		exynos_hpgov_lcd_status_update(BIG_DUAL_MODE);
		break;
	default:
		break;
	}

	return ret;
}

static struct notifier_block exynos_hpgov_fb_notifier_block = {
        .notifier_call = exynos_hpgov_fb_notifier,
};

static int __init exynos_hpgov_init(void)
{
	int ret = 0;
	const int attr_count = 3;

	hrtimer_init(&exynos_hpgov.slack_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_PINNED);

	exynos_hpgov.slack_timer.function = exynos_hpgov_slack_timer;

	mutex_init(&exynos_hpgov.attrib_lock);
	init_waitqueue_head(&exynos_hpgov.wait_q);
	init_waitqueue_head(&exynos_hpgov.wait_hpq);
	init_irq_work(&exynos_hpgov.update_irq_work, exynos_hpgov_irq_work);
	init_irq_work(&exynos_hpgov.start_slack_timer_irq_work, slack_timer_irq_work);

	exynos_hpgov.attrib.attrib_group.attrs =
		kzalloc(attr_count * sizeof(struct attribute *), GFP_KERNEL);
	if (!exynos_hpgov.attrib.attrib_group.attrs) {
		ret = -ENOMEM;
		goto done;
	}

	HPGOV_RW_ATTRIB(0, enabled);
	HPGOV_RW_ATTRIB(1, dual_change_ms);

	exynos_hpgov.attrib.attrib_group.name = "governor";
	ret = sysfs_create_group(exynos_cpu_hotplug_kobj(), &exynos_hpgov.attrib.attrib_group);
	if (ret)
		pr_err("Unable to create sysfs objects :%d\n", ret);

	exynos_hpgov.dual_change_ms = DEFAULT_DUAL_CHANGE_MS;
	exynos_hpgov.cur_cpu_max = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
	exynos_hpgov.cur_cpu_min = PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE;

	pm_qos_add_request(&hpgov_max_pm_qos, PM_QOS_CPU_ONLINE_MAX, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
	pm_qos_add_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MIN, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	/* Register FB notifier */
	fb_register_client(&exynos_hpgov_fb_notifier_block);

	schedule_delayed_work_on(0, &hpgov_boot_work, msecs_to_jiffies(DEFAULT_BOOT_ENABLE_MS));

done:
	return ret;
}
late_initcall(exynos_hpgov_init);
