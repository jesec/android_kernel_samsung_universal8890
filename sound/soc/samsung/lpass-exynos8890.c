/* sound/soc/samsung/lpass-exynos7420.c
 *
 * Low Power Audio SubSystem driver for Samsung Exynos
 *
 * Copyright (c) 2013 Samsung Electronics Co. Ltd.
 *	Yeongman Seo <yman.seo@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/platform_device.h>

#include <sound/exynos.h>
#include <soc/samsung/exynos-powermode.h>

#if 0
#include <mach/map.h>
#include <mach/regs-pmu.h>
#endif

#include "lpass.h"
#include "i2s.h"

/* Default ACLK gate for
   aclk_dmac, aclk_sramc */
#define INIT_ACLK_GATE_MASK	(1 << 31 | 1 << 30)

/* Default PCLK gate for
   pclk_wdt0, pclk_wdt1, pclk_slimbus,
   pclk_pcm, pclk_i2s, pclk_timer */
#define INIT_PCLK_GATE_MASK	(1 << 22 | 1 << 23 | 1 << 24 | \
				 1 << 26 | 1 << 27 | 1 << 28)

/* Default SCLK gate for
   sclk_ca5, sclk_slimbus, sclk_uart,
   sclk_i2s, sclk_pcm, sclk_slimbus_clkin */
#define INIT_SCLK_GATE_MASK	(1 << 31 | 1 << 30 | 1 << 29 | \
				 1 << 28 | 1 << 27 | 1 << 26)
static struct lpass_cmu_info {
	struct clk		*aud_lpass;
	struct clk		*aud_pll;
} lpass_cmu;

static int g_init_sicd_index;
static int g_init_sicd_aud_index;
static int g_sicd_index;
static int g_sicd_aud_index;
static int g_current_power_mode;

void __iomem *lpass_cmu_save[] = {
	NULL,	/* endmark */
};

extern int check_adma_status(void);

void lpass_init_clk_gate(void)
{
	return;
}

void lpass_reset_clk_default(void)
{
	return;
}

int lpass_get_clk(struct device *dev, struct lpass_info *lpass)
{
	return 0;
}

void lpass_put_clk(struct lpass_info *lpass)
{
	return;
}

void lpass_set_mux_osc(void)
{
	return;
}

void lpass_set_mux_pll(void)
{
	return;
}


int lpass_set_clk_heirachy(struct device *dev)
{
	lpass_cmu.aud_lpass = clk_get(dev, "gate_aud_lpass");
	if (IS_ERR(lpass_cmu.aud_lpass)) {
		dev_err(dev, "aud_lpass clk not found\n");
		return -1;
	}

	lpass_cmu.aud_pll = clk_get(dev, "sclk_aud_pll");
	if (IS_ERR(lpass_cmu.aud_pll)) {
		dev_err(dev, "sclk_aud_pll not found\n");
		goto err;
	}

	return 0;
err:
	clk_put(lpass_cmu.aud_lpass);
	return -1;
}

void lpass_enable_pll(bool on)
{
	if (on) {
		clk_prepare_enable(lpass_cmu.aud_pll);
		clk_set_rate(lpass_cmu.aud_pll, 492000000);
#if 0
		if (lpass_i2s_master_mode()) {
			void *sfr;
			u32 cfg;

			clk_set_rate(lpass_cmu.aud_pll, 491520000);

			/* AUDIO DIVIDER sfrs */
			sfr = ioremap(0x114C0000, SZ_4K);
			clk_set_rate(lpass_cmu.aud_pll, 491520000);
			cfg = readl(sfr + 0x400);
			cfg |= 0x9; /* Divider for AUD_CA5 = 10 */
			writel(cfg, sfr + 0x400);

			cfg = readl(sfr + 0x404);
			cfg |= 0xf; /* Divider for AUD_ACLK = 16 */
			writel(cfg, sfr + 0x404);
			iounmap(sfr);
		} else {
			clk_set_rate(lpass_cmu.aud_pll, 492000000);
		}
#endif

		clk_prepare_enable(lpass_cmu.aud_lpass);
	} else {
		clk_disable_unprepare(lpass_cmu.aud_lpass);
		clk_disable_unprepare(lpass_cmu.aud_pll);
	}
}

void lpass_update_lpclock_impl(struct device *dev, u32 ctrlid, bool active)
{
#ifdef CONFIG_CPU_IDLE
	int dram_used;

	dram_used = check_adma_status();

	if (g_init_sicd_index == 0) {
		g_sicd_index = exynos_get_idle_ip_index(dev_name(dev));
		g_init_sicd_index = 1;
	}
	if (g_init_sicd_aud_index == 0) {
		g_sicd_aud_index = exynos_get_idle_ip_index("11400000.lpass.sicd_aud");
		g_init_sicd_aud_index = 1;
	}
	if (g_sicd_index < 0 || g_sicd_aud_index < 0) {
		dev_err(dev, "ERROR : Can't get SICD index for 'audio'.\n");
		return;
	}

	if (ctrlid & LPCLK_CTRLID_LEGACY) {
		if (active && dram_used == 0)
			g_current_power_mode |= LPCLK_CTRLID_LEGACY;
		else
			g_current_power_mode &= (~LPCLK_CTRLID_LEGACY);
	}
	if (ctrlid & LPCLK_CTRLID_OFFLOAD) {
		if (active)
			g_current_power_mode |= LPCLK_CTRLID_OFFLOAD;
		else
			g_current_power_mode &= (~LPCLK_CTRLID_OFFLOAD);
	}
	switch (g_current_power_mode) {
	case 0x00:
		exynos_update_ip_idle_status(g_sicd_index, 1);
		exynos_update_ip_idle_status(g_sicd_aud_index, 0);
		break;
	case 0x01:
		exynos_update_ip_idle_status(g_sicd_index, 0);
		exynos_update_ip_idle_status(g_sicd_aud_index, 1);
		break;
	case 0x02:
	case 0x03:
		exynos_update_ip_idle_status(g_sicd_index, 0);
		exynos_update_ip_idle_status(g_sicd_aud_index, 0);
		break;
	default:
		pr_err("[ERROR] Invalid audio power mode: 0x%04X\n",
			g_current_power_mode);
	}
#endif
}



/* Module information */
MODULE_AUTHOR("Hyunwoong Kim, <khw0178.kim@samsung.com>");
MODULE_LICENSE("GPL");
