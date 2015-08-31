/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/wakeup_reason.h>
#include <linux/gpio.h>
#include <linux/syscore_ops.h>
#include <asm/psci.h>
#include <asm/suspend.h>

#include <soc/samsung/exynos-pm.h>
#include <soc/samsung/exynos-pmu.h>
#include <soc/samsung/exynos-powermode.h>

#define EXYNOS8890_PA_GPIO_ALIVE	0x10580000
#define WAKEUP_STAT_EINT                (1 << 0)
#define WAKEUP_STAT_RTC_ALARM           (1 << 1)
/*
 * PMU register offset
 */
#define EXYNOS_PMU_WAKEUP_STAT		0x0600
#define EXYNOS_PMU_EINT_WAKEUP_MASK	0x060C

static void __iomem *exynos_eint_base;
extern u32 exynos_eint_to_pin_num(int eint);
#define EXYNOS_EINT_PEND(b, x)      ((b) + 0xA00 + (((x) >> 3) * 4))

static void exynos_show_wakeup_reason_eint(void)
{
	int bit;
	int i, size;
	long unsigned int ext_int_pend;
	u64 eint_wakeup_mask;
	bool found = 0;
	unsigned int val;

	exynos_pmu_read(EXYNOS_PMU_EINT_WAKEUP_MASK, &val);
	eint_wakeup_mask = val;

	for (i = 0, size = 8; i < 32; i += size) {

		ext_int_pend =
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, i));

		for_each_set_bit(bit, &ext_int_pend, size) {
			u32 gpio;
			int irq;

			if (eint_wakeup_mask & (1 << (i + bit)))
				continue;

			gpio = exynos_eint_to_pin_num(i + bit);
			irq = gpio_to_irq(gpio);

			log_wakeup_reason(irq);
			update_wakeup_reason_stats(irq, i + bit);
			found = 1;
		}
	}

	if (!found)
		pr_info("Resume caused by unknown EINT\n");
}

static void exynos_show_wakeup_registers(unsigned long wakeup_stat)
{
	pr_info("WAKEUP_STAT: 0x%08lx\n", wakeup_stat);
	pr_info("EINT_PEND: 0x%02x, 0x%02x 0x%02x, 0x%02x\n",
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 0)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 8)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 16)),
			__raw_readl(EXYNOS_EINT_PEND(exynos_eint_base, 24)));
}

static void exynos_show_wakeup_reason(bool sleep_abort)
{
	unsigned int wakeup_stat;

	if (sleep_abort)
		pr_info("PM: early wakeup!\n");

	exynos_pmu_read(EXYNOS_PMU_WAKEUP_STAT, &wakeup_stat);

	exynos_show_wakeup_registers(wakeup_stat);

	if (wakeup_stat & WAKEUP_STAT_RTC_ALARM)
		pr_info("Resume caused by RTC alarm\n");
	else if (wakeup_stat & WAKEUP_STAT_EINT)
		exynos_show_wakeup_reason_eint();
	else
		pr_info("Resume caused by wakeup_stat 0x%08x\n",
			wakeup_stat);
}

#ifdef CONFIG_CPU_IDLE
static DEFINE_RWLOCK(exynos_pm_notifier_lock);
static RAW_NOTIFIER_HEAD(exynos_pm_notifier_chain);

static int exynos_pm_notify(enum exynos_pm_event event, int nr_to_call, int *nr_calls)
{
	int ret;

	ret = __raw_notifier_call_chain(&exynos_pm_notifier_chain, event, NULL,
		nr_to_call, nr_calls);

	return notifier_to_errno(ret);
}

int exynos_pm_register_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_register(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_register_notifier);

int exynos_pm_unregister_notifier(struct notifier_block *nb)
{
	unsigned long flags;
	int ret;

	write_lock_irqsave(&exynos_pm_notifier_lock, flags);
	ret = raw_notifier_chain_unregister(&exynos_pm_notifier_chain, nb);
	write_unlock_irqrestore(&exynos_pm_notifier_lock, flags);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_unregister_notifier);

int exynos_pm_lpa_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_ENTER, -1, &nr_calls);
	if (ret)
		/*
		 * Inform listeners (nr_calls - 1) about failure of LPA
		 * entry who are notified earlier to prepare for it.
		 */
		exynos_pm_notify(LPA_ENTER_FAIL, nr_calls - 1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_lpa_enter);

int exynos_pm_lpa_exit(void)
{
	int ret;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(LPA_EXIT, -1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_lpa_exit);

int exynos_pm_sicd_enter(void)
{
	int nr_calls;
	int ret = 0;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(SICD_ENTER, -1, &nr_calls);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_sicd_enter);

int exynos_pm_sicd_exit(void)
{
	int ret;

	read_lock(&exynos_pm_notifier_lock);
	ret = exynos_pm_notify(SICD_EXIT, -1, NULL);
	read_unlock(&exynos_pm_notifier_lock);

	return ret;
}
EXPORT_SYMBOL_GPL(exynos_pm_sicd_exit);
#endif /* CONFIG_CPU_IDLE */

int early_wakeup;
int psci_index;
int is_cp_call;

static int exynos_pm_syscore_suspend(void)
{
	if (!exynos_check_cp_status()) {
		pr_info("%s: sleep canceled by CP reset \n",__func__);
		return -EINVAL;
	}

	psci_index = PSCI_SYSTEM_SLEEP;
	exynos_prepare_sys_powerdown(SYS_SLEEP);
	pr_info("%s: Enter sleep mode\n",__func__);

	return 0;
}

static void exynos_pm_syscore_resume(void)
{
	exynos_wakeup_sys_powerdown(SYS_SLEEP, (bool)early_wakeup);

	exynos_show_wakeup_reason((bool)early_wakeup);

	pr_debug("%s: post sleep, preparing to return\n", __func__);
}

static struct syscore_ops exynos_pm_syscore_ops = {
	.suspend	= exynos_pm_syscore_suspend,
	.resume		= exynos_pm_syscore_resume,
};

static int exynos_pm_enter(suspend_state_t state)
{
	/* This will also act as our return point when
	 * we resume as it saves its own register state and restores it
	 * during the resume. */
	early_wakeup = cpu_suspend(psci_index);
	if (early_wakeup)
		pr_info("%s: return to originator\n", __func__);

	return early_wakeup;
}

static const struct platform_suspend_ops exynos_pm_ops = {
	.enter		= exynos_pm_enter,
	.valid		= suspend_valid_only_mem,
};

static __init int exynos_pm_drvinit(void)
{
	suspend_set_ops(&exynos_pm_ops);
	register_syscore_ops(&exynos_pm_syscore_ops);

	exynos_eint_base = ioremap(EXYNOS8890_PA_GPIO_ALIVE, SZ_8K);

	if (exynos_eint_base == NULL) {
		pr_err("%s: unable to ioremap for EINT base address\n",
				__func__);
		BUG();
	}

	return 0;
}
arch_initcall(exynos_pm_drvinit);
