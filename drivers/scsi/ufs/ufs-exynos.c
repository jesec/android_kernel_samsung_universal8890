/*
 * UFS Host Controller driver for Exynos specific extensions
 *
 * Copyright (C) 2013-2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/clk.h>

#include <soc/samsung/exynos-pm.h>

#include "ufshcd.h"
#include "unipro.h"
#include "mphy.h"
#include "ufshcd-pltfrm.h"
#include "ufs-exynos.h"

/*
 * Unipro attribute value
 */
#define TXTRAILINGCLOCKS	0x10
#define TACTIVATE_10_USEC	400	/* unit: 10us */

/* Device ID */
#define DEV_ID	0x00
#define PEER_DEV_ID	0x01
#define PEER_CPORT_ID	0x00
#define TRAFFIC_CLASS	0x00

/*
 * Default M-PHY parameter
 */
#define TX_DIF_P_NSEC		3000000	/* unit: ns */
#define TX_DIF_N_NSEC		1000000	/* unit: ns */
#define RX_DIF_P_NSEC		1000000	/* unit: ns */
#define RX_HIBERN8_WAIT_NSEC	4000000	/* unit: ns */
#define HIBERN8_TIME		40	/* unit: 100us */
#define RX_BASE_UNIT_NSEC	100000	/* unit: ns */
#define RX_GRAN_UNIT_NSEC	4000	/* unit: ns */
#define RX_SLEEP_CNT		1280	/* unit: ns */
#define RX_STALL_CNT		320	/* unit: ns */

#define TX_HIGH_Z_CNT_NSEC	20000	/* unit: ns */
#define TX_BASE_UNIT_NSEC	100000	/* unit: ns */
#define TX_GRAN_UNIT_NSEC	4000	/* unit: ns */
#define TX_SLEEP_CNT		1000	/* unit: ns */
#define IATOVAL_NSEC		20000	/* unit: ns */

#define UNIPRO_PCLK_PERIOD(ufs) (NSEC_PER_SEC / ufs->pclk_rate)
#define UNIPRO_MCLK_PERIOD(ufs) (NSEC_PER_SEC / ufs->mclk_rate)
#define PHY_PMA_COMN_ADDR(reg)		((reg) << 2)
#define PHY_PMA_TRSV_ADDR(reg, lane)	(((reg) + (0x30 * (lane))) << 2)

const char *const phy_symb_clks[] = {
	"phyclk_ufs_tx0_symbol",
	"phyclk_ufs_tx1_symbol",
	"phyclk_ufs_rx0_symbol",
	"phyclk_ufs_rx1_symbol"
};

#define phy_pma_writel(ufs, val, reg)	\
	writel((val), (ufs)->phy.reg_pma + (reg))
#define phy_pma_readl(ufs, reg)	\
	readl((ufs)->phy.reg_pma + (reg))

#define EXYNOS_UFS_MMIO_FUNC(name)						\
static inline void name##_writel(struct exynos_ufs *ufs, u32 val, u32 reg)	\
{										\
	writel(val, ufs->reg_##name + reg);					\
}										\
										\
static inline u32 name##_readl(struct exynos_ufs *ufs, u32 reg)			\
{										\
	return readl(ufs->reg_##name + reg);					\
}

EXYNOS_UFS_MMIO_FUNC(hci);
EXYNOS_UFS_MMIO_FUNC(unipro);
EXYNOS_UFS_MMIO_FUNC(ufsp);
#undef EXYNOS_UFS_MMIO_FUNC

#define for_each_ufs_lane(ufs, i) \
	for (i = 0; i < (ufs)->avail_ln_rx; i++)
#define for_each_phy_cfg(cfg) \
	for (; (cfg)->flg != PHY_CFG_NONE; (cfg)++)

static const struct of_device_id exynos_ufs_match[];

static inline const struct exynos_ufs_soc *to_phy_soc(struct exynos_ufs *ufs)
{
	return ufs->phy.soc;
}

static inline struct exynos_ufs *to_exynos_ufs(struct ufs_hba *hba)
{
	return dev_get_platdata(hba->dev);
}

static inline void exynos_ufs_ctrl_phy_pwr(struct exynos_ufs *ufs, bool en)
{
	/* TODO: offset, mask */
	writel(!!en, ufs->phy.reg_pmu);
}

static inline const
struct exynos_ufs_soc *exynos_ufs_get_drv_data(struct device *dev)
{
	const struct of_device_id *match;

	match = of_match_node(exynos_ufs_match, dev->of_node);
	return ((struct ufs_hba_variant *)match->data)->vs_data;
}

static void exynos_ufs_ctrl_clk(struct exynos_ufs *ufs, bool en)
{
	u32 reg = hci_readl(ufs, HCI_FORCE_HCS);

	if (en)
		hci_writel(ufs, reg | CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
	else
		hci_writel(ufs, reg & ~CLK_STOP_CTRL_EN_ALL, HCI_FORCE_HCS);
}

static void exynos_ufs_gate_clk(struct exynos_ufs *ufs, bool en)
{

	u32 reg = hci_readl(ufs, HCI_CLKSTOP_CTRL);

	if (en)
		hci_writel(ufs, reg | CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
	else
		hci_writel(ufs, reg & ~CLK_STOP_ALL, HCI_CLKSTOP_CTRL);
}

static void exynos_ufs_set_unipro_pclk(struct exynos_ufs *ufs)
{
	u32 pclk_ctrl, pclk_rate;
	u32 f_min, f_max;
	u8 div = 0;

	f_min = ufs->pclk_avail_min;
	f_max = ufs->pclk_avail_max;
	pclk_rate = clk_get_rate(ufs->clk_hci);

	do {
		pclk_rate /= (div + 1);

		if (pclk_rate <= f_max)
			break;
		else
			div++;
	} while (pclk_rate >= f_min);

	WARN(pclk_rate < f_min, "not available pclk range %d\n", pclk_rate);

	pclk_ctrl = hci_readl(ufs, HCI_UNIPRO_APB_CLK_CTRL);
	pclk_ctrl = (pclk_ctrl & ~0xf) | (div & 0xf);
	hci_writel(ufs, pclk_ctrl, HCI_UNIPRO_APB_CLK_CTRL);
	ufs->pclk_rate = pclk_rate;
}

static void exynos_ufs_set_unipro_mclk(struct exynos_ufs *ufs)
{
	ufs->mclk_rate = clk_get_rate(ufs->clk_unipro);
}

static void exynos_ufs_set_unipro_clk(struct exynos_ufs *ufs)
{
	exynos_ufs_set_unipro_pclk(ufs);
	exynos_ufs_set_unipro_mclk(ufs);
}

static void exynos_ufs_set_pwm_clk_div(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	const int div = 30, mult = 20;
	const unsigned long pwm_min = 3 * 1000 * 1000;
	const unsigned long pwm_max = 9 * 1000 * 1000;
	long clk_period;
	const int divs[] = {32, 16, 8, 4};
	unsigned long _clk, clk = 0;
	int i = 0, clk_idx = -1;

	clk_period = UNIPRO_PCLK_PERIOD(ufs);

	for (i = 0; i < ARRAY_SIZE(divs); i++) {
		_clk = NSEC_PER_SEC * mult / (clk_period * divs[i] * div);
		if (_clk >= pwm_min && _clk <= pwm_max) {
			if (_clk > clk) {
				clk_idx = i;
				clk = _clk;
			}
		}
	}

	if (clk_idx >= 0)
		ufshcd_dme_set(hba, UIC_ARG_MIB(CMN_PWM_CMN_CTRL),
				clk_idx & PWM_CMN_CTRL_MASK);
	else
		dev_err(ufs->dev, "not dicided pwm clock divider\n");
}

static long exynos_ufs_calc_time_cntr(struct exynos_ufs *ufs, long period)
{
	const int precise = 10;
	long pclk_rate = ufs->pclk_rate;
	long clk_period, fraction;

	clk_period = UNIPRO_PCLK_PERIOD(ufs);
	fraction = ((NSEC_PER_SEC % pclk_rate) * precise) / pclk_rate;

	return (period * precise) / ((clk_period * precise) + fraction);
}

static void exynos_ufs_compute_phy_time_v(struct exynos_ufs *ufs,
					struct phy_tm_parm *tm_parm)
{
	tm_parm->tx_linereset_p =
		exynos_ufs_calc_time_cntr(ufs, TX_DIF_P_NSEC);
	tm_parm->tx_linereset_n =
		exynos_ufs_calc_time_cntr(ufs, TX_DIF_N_NSEC);
	tm_parm->tx_high_z_cnt =
		exynos_ufs_calc_time_cntr(ufs, TX_HIGH_Z_CNT_NSEC);
	tm_parm->tx_base_n_val =
		exynos_ufs_calc_time_cntr(ufs, TX_BASE_UNIT_NSEC);
	tm_parm->tx_gran_n_val =
		exynos_ufs_calc_time_cntr(ufs, TX_GRAN_UNIT_NSEC);
	tm_parm->tx_sleep_cnt =
		exynos_ufs_calc_time_cntr(ufs, TX_SLEEP_CNT);

	tm_parm->rx_linereset =
		exynos_ufs_calc_time_cntr(ufs, RX_DIF_P_NSEC);
	tm_parm->rx_hibern8_wait =
		exynos_ufs_calc_time_cntr(ufs, RX_HIBERN8_WAIT_NSEC);
	tm_parm->rx_base_n_val =
		exynos_ufs_calc_time_cntr(ufs, RX_BASE_UNIT_NSEC);
	tm_parm->rx_gran_n_val =
		exynos_ufs_calc_time_cntr(ufs, RX_GRAN_UNIT_NSEC);
	tm_parm->rx_sleep_cnt =
		exynos_ufs_calc_time_cntr(ufs, RX_SLEEP_CNT);
	tm_parm->rx_stall_cnt =
		exynos_ufs_calc_time_cntr(ufs, RX_STALL_CNT);
}

static void exynos_ufs_config_phy_time_v(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	struct phy_tm_parm tm_parm;
	int i;

	exynos_ufs_compute_phy_time_v(ufs, &tm_parm);

	exynos_ufs_set_pwm_clk_div(ufs);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);

	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_FILLER_ENABLE, i),
				RX_FILLER_ENABLE);
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_LINERESET_VAL, i),
				RX_LINERESET(tm_parm.rx_linereset));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_BASE_NVAL_07_00, i),
				RX_BASE_NVAL_L(tm_parm.rx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_BASE_NVAL_15_08, i),
				RX_BASE_NVAL_H(tm_parm.rx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_GRAN_NVAL_07_00, i),
				RX_GRAN_NVAL_L(tm_parm.rx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_GRAN_NVAL_10_08, i),
				RX_GRAN_NVAL_H(tm_parm.rx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_OV_SLEEP_CNT_TIMER, i),
				RX_OV_SLEEP_CNT(tm_parm.rx_sleep_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_OV_STALL_CNT_TIMER, i),
				RX_OV_STALL_CNT(tm_parm.rx_stall_cnt));
	}

	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_LINERESET_P_VAL, i),
				TX_LINERESET_P(tm_parm.tx_linereset_p));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_HIGH_Z_CNT_07_00, i),
				TX_HIGH_Z_CNT_L(tm_parm.tx_high_z_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_HIGH_Z_CNT_11_08, i),
				TX_HIGH_Z_CNT_H(tm_parm.tx_high_z_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_BASE_NVAL_07_00, i),
				TX_BASE_NVAL_L(tm_parm.tx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_BASE_NVAL_15_08, i),
				TX_BASE_NVAL_H(tm_parm.tx_base_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_GRAN_NVAL_07_00, i),
				TX_GRAN_NVAL_L(tm_parm.tx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_GRAN_NVAL_10_08, i),
				TX_GRAN_NVAL_H(tm_parm.tx_gran_n_val));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_OV_SLEEP_CNT_TIMER, i),
				TX_OV_H8_ENTER_EN |
				TX_OV_SLEEP_CNT(tm_parm.tx_sleep_cnt));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(TX_MIN_ACTIVATE_TIME, i),
				0xA);
	}

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}

static void exynos_ufs_config_phy_cap_attr(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	int i;

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);

	for_each_ufs_lane(ufs, i) {
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G1_SYNC_LENGTH_CAP, i),
				SYNC_RANGE_COARSE | SYNC_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G2_SYNC_LENGTH_CAP, i),
				SYNC_RANGE_COARSE | SYNC_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G3_SYNC_LENGTH_CAP, i),
				SYNC_RANGE_COARSE | SYNC_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G1_PREP_LENGTH_CAP, i),
				PREP_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G2_PREP_LENGTH_CAP, i),
				PREP_LEN(0xf));
		ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(RX_HS_G3_PREP_LENGTH_CAP, i),
				PREP_LEN(0xf));
	}

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}

static void exynos_ufs_establish_connt(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;

	/* allow cport attributes to be set */
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CONNECTIONSTATE), CPORT_IDLE);

	/* local */
	ufshcd_dme_set(hba, UIC_ARG_MIB(N_DEVICEID), DEV_ID);
	ufshcd_dme_set(hba, UIC_ARG_MIB(N_DEVICEID_VALID), TRUE);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERDEVICEID), PEER_DEV_ID);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_PEERCPORTID), PEER_CPORT_ID);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CPORTFLAGS), CPORT_DEF_FLAGS);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_TRAFFICCLASS), TRAFFIC_CLASS);
	ufshcd_dme_set(hba, UIC_ARG_MIB(T_CONNECTIONSTATE), CPORT_CONNECTED);
}

static void exynos_ufs_config_smu(struct exynos_ufs *ufs)
{
	u32 reg;

	reg = ufsp_readl(ufs, UFSPRSECURITY);
	ufsp_writel(ufs, reg | NSSMU, UFSPRSECURITY);

	ufsp_writel(ufs, 0x0, UFSPSBEGIN0);
	ufsp_writel(ufs, 0xffffffff, UFSPSEND0);
	ufsp_writel(ufs, 0xff, UFSPSLUN0);
	ufsp_writel(ufs, 0xf1, UFSPSCTRL0);
}

static bool exynos_ufs_wait_pll_lock(struct exynos_ufs *ufs)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	u32 reg;

	do {
		reg = phy_pma_readl(ufs, PHY_PMA_COMN_ADDR(0x1E));
		if ((reg >> 5) & 0x1)
			return true;
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout mphy pll lock\n");

	return false;
}

static bool exynos_ufs_wait_cdr_lock(struct exynos_ufs *ufs)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);
	u32 reg;

	do {
		/* Need to check for all lane? */
		reg = phy_pma_readl(ufs, PHY_PMA_TRSV_ADDR(0x5E, 0));
		if ((reg >> 4) & 0x1)
			return true;
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout mphy cdr lock\n");

	return false;
}

static inline bool __match_mode_by_cfg(struct uic_pwr_mode *pmd, int mode)
{
	bool match = false;
	u8 _m, _l, _g;

	_m = pmd->mode;
	_g = pmd->gear;
	_l = pmd->lane;

	if (mode == PMD_ALL)
		match = true;
	else if (IS_PWR_MODE_HS(_m) && mode == PMD_HS)
		match = true;
	else if (IS_PWR_MODE_PWM(_m) && mode == PMD_PWM)
		match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 1
			&& mode & PMD_HS_G1_L1)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 1 && _l == 2
			&& mode & PMD_HS_G1_L2)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 1
			&& mode & PMD_HS_G2_L1)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 2 && _l == 2
			&& mode & PMD_HS_G2_L2)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 1
			&& mode & PMD_HS_G3_L1)
			match = true;
	else if (IS_PWR_MODE_HS(_m) && _g == 3 && _l == 2
			&& mode & PMD_HS_G3_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 1
			&& mode & PMD_PWM_G1_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 1 && _l == 2
			&& mode & PMD_PWM_G1_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 1
			&& mode & PMD_PWM_G2_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 2 && _l == 2
			&& mode & PMD_PWM_G2_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 1
			&& mode & PMD_PWM_G3_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 3 && _l == 2
			&& mode & PMD_PWM_G3_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 1
			&& mode & PMD_PWM_G4_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 4 && _l == 2
			&& mode & PMD_PWM_G4_L2)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 1
			&& mode & PMD_PWM_G5_L1)
			match = true;
	else if (IS_PWR_MODE_PWM(_m) && _g == 5 && _l == 2
			&& mode & PMD_PWM_G5_L2)
			match = true;

	return match;
}

static void exynos_ufs_config_phy(struct exynos_ufs *ufs,
				  const struct ufs_phy_cfg *cfg,
				  struct uic_pwr_mode *pmd)
{
	struct ufs_hba *hba = ufs->hba;
	int i;

	if (!cfg)
		return;

	for_each_phy_cfg(cfg) {
		for_each_ufs_lane(ufs, i) {
			if (pmd && !__match_mode_by_cfg(pmd, cfg->flg))
				continue;

			switch (cfg->lyr) {
			case PHY_PCS_COMN:
			case UNIPRO_STD_MIB:
			case UNIPRO_DBG_MIB:
				if (i == 0)
					ufshcd_dme_set(hba, UIC_ARG_MIB(cfg->addr), cfg->val);
				break;
			case PHY_PCS_RXTX:
				ufshcd_dme_set(hba, UIC_ARG_MIB_SEL(cfg->addr, i), cfg->val);
				break;
			case PHY_PMA_COMN:
				if (i == 0)
					phy_pma_writel(ufs, cfg->val, PHY_PMA_COMN_ADDR(cfg->addr));
				break;
			case PHY_PMA_TRSV:
				phy_pma_writel(ufs, cfg->val, PHY_PMA_TRSV_ADDR(cfg->addr, i));
				break;
			case UNIPRO_DBG_APB:
				unipro_writel(ufs, cfg->val, cfg->addr);
			}
		}
	}
}

static void exynos_ufs_config_sync_pattern_mask(struct exynos_ufs *ufs,
					struct uic_pwr_mode *pmd)
{
	struct ufs_hba *hba = ufs->hba;
	u8 g = pmd->gear;
	u32 mask, sync_len;
	int i;
#define SYNC_LEN_G1	(80 * 1000) /* 80 us */
#define SYNC_LEN_G2	(40 * 1000) /* 40 us */
#define SYNC_LEN_G3	(20 * 1000) /* 20 us */

	if (g == 1)
		sync_len = SYNC_LEN_G1;
	else if (g == 2)
		sync_len = SYNC_LEN_G2;
	else if (g == 3)
		sync_len = SYNC_LEN_G3;
	else
		return;

	mask = exynos_ufs_calc_time_cntr(ufs, sync_len);
	mask = (mask >> 8) & 0xff;

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), TRUE);

	for_each_ufs_lane(ufs, i)
		ufshcd_dme_set(hba,
			UIC_ARG_MIB_SEL(RX_SYNC_MASK_LENGTH, i), mask);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_OV_TM), FALSE);
}

static void exynos_ufs_pre_prep_pmc(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;

	if (!soc)
		return;

	if (IS_PWR_MODE_HS(act_pmd->mode)) {
		exynos_ufs_config_sync_pattern_mask(ufs, act_pmd);

		switch (act_pmd->hs_series) {
		case PA_HS_MODE_A:
			exynos_ufs_config_phy(ufs,
				soc->tbl_calib_of_hs_rate_a, act_pmd);
			break;
		case PA_HS_MODE_B:
			exynos_ufs_config_phy(ufs,
				soc->tbl_calib_of_hs_rate_b, act_pmd);
			break;
		default:
			break;
		}
	} else if (IS_PWR_MODE_PWM(act_pmd->mode)) {
		exynos_ufs_config_phy(ufs, soc->tbl_calib_of_pwm, act_pmd);
	}
}

static void exynos_ufs_post_prep_pmc(struct ufs_hba *hba,
		struct ufs_pa_layer_attr *pwr_max,
		struct ufs_pa_layer_attr *pwr_req)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);
	struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;

	if (!soc)
		return;

	if (IS_PWR_MODE_HS(act_pmd->mode)) {
		switch (act_pmd->hs_series) {
		case PA_HS_MODE_A:
			exynos_ufs_config_phy(ufs,
				soc->tbl_post_calib_of_hs_rate_a, act_pmd);
			break;
		case PA_HS_MODE_B:
			exynos_ufs_config_phy(ufs,
				soc->tbl_post_calib_of_hs_rate_b, act_pmd);
			break;
		default:
			break;
		}
	} else if (IS_PWR_MODE_PWM(act_pmd->mode)) {
		exynos_ufs_config_phy(ufs, soc->tbl_post_calib_of_pwm, act_pmd);
	}

	exynos_ufs_wait_pll_lock(ufs);
	exynos_ufs_wait_cdr_lock(ufs);
}

static void exynos_ufs_phy_init(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;
	const struct exynos_ufs_soc *soc = to_phy_soc(ufs);

	if (ufs->avail_ln_rx == 0 || ufs->avail_ln_tx == 0) {
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_AVAILRXDATALANES),
			&ufs->avail_ln_rx);
		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_AVAILTXDATALANES),
			&ufs->avail_ln_tx);
		WARN(ufs->avail_ln_rx != ufs->avail_ln_tx,
			"available data lane is not equal(rx:%d, tx:%d)\n",
			ufs->avail_ln_rx, ufs->avail_ln_tx);
	}

	if (!soc)
		return;

	exynos_ufs_config_phy(ufs, soc->tbl_phy_init, NULL);
}

static void exynos_ufs_config_unipro(struct exynos_ufs *ufs)
{
	struct ufs_hba *hba = ufs->hba;

	unipro_writel(ufs, 0x1, UNIP_DME_PACP_CNFBIT);

	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_DBG_CLK_PERIOD),
		UNIPRO_MCLK_PERIOD(ufs));
	ufshcd_dme_set(hba, UIC_ARG_MIB(PA_TXTRAILINGCLOCKS), TXTRAILINGCLOCKS);
}

static void exynos_ufs_config_intr(struct exynos_ufs *ufs, u32 errs, u8 index)
{
	switch(index) {
	case UNIP_PA_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_PA_LAYER);
		break;
	case UNIP_DL_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_DL_LAYER);
		break;
	case UNIP_N_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_N_LAYER);
		break;
	case UNIP_T_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_T_LAYER);
		break;
	case UNIP_DME_LYR:
		hci_writel(ufs, DFES_ERR_EN | errs, HCI_ERROR_EN_DME_LAYER);
		break;
	}
}

static int exynos_ufs_pre_link(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* refer to hba */
	ufs->hba = hba;

	/* hci */
	exynos_ufs_config_intr(ufs, DFES_DEF_DL_ERRS, UNIP_DL_LYR);
	exynos_ufs_config_intr(ufs, DFES_DEF_N_ERRS, UNIP_N_LYR);
	exynos_ufs_config_intr(ufs, DFES_DEF_T_ERRS, UNIP_T_LYR);

	exynos_ufs_set_unipro_clk(ufs);
	exynos_ufs_ctrl_clk(ufs, true);

	/* mphy */
	exynos_ufs_phy_init(ufs);

	/* unipro */
	exynos_ufs_config_unipro(ufs);

	/* mphy */
	exynos_ufs_config_phy_time_v(ufs);
	exynos_ufs_config_phy_cap_attr(ufs);

	return 0;
}

static void exynos_ufs_fit_aggr_timeout(struct exynos_ufs *ufs)
{
	const u8 cnt_div_val = 40;
	u32 cnt_val;

	cnt_val = exynos_ufs_calc_time_cntr(ufs, IATOVAL_NSEC / cnt_div_val);
	hci_writel(ufs, cnt_val & CNT_VAL_1US_MASK, HCI_1US_TO_CNT_VAL);
}

static int exynos_ufs_post_link(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	exynos_ufs_establish_connt(ufs);
	exynos_ufs_fit_aggr_timeout(ufs);

	hci_writel(ufs, 0xA, HCI_DATA_REORDER);
	hci_writel(ufs, PRDT_PREFECT_EN | PRDT_SET_SIZE(12),
			HCI_TXPRDT_ENTRY_SIZE);
	hci_writel(ufs, PRDT_SET_SIZE(12), HCI_RXPRDT_ENTRY_SIZE);
	hci_writel(ufs, 0xFFFFFFFF, HCI_UTRL_NEXUS_TYPE);
	hci_writel(ufs, 0xFFFFFFFF, HCI_UTMRL_NEXUS_TYPE);
	hci_writel(ufs, 0x15, HCI_AXIDMA_RWDATA_BURST_LEN);

	if (ufs->opts & EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB)
		ufshcd_dme_set(hba,
			UIC_ARG_MIB(T_DBG_SKIP_INIT_HIBERN8_EXIT), TRUE);

	return 0;
}

static void exynos_ufs_host_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	unsigned long timeout = jiffies + msecs_to_jiffies(1);

	hci_writel(ufs, UFS_SW_RST_MASK, HCI_SW_RST);

	do {
		if (!(hci_readl(ufs, HCI_SW_RST) & UFS_SW_RST_MASK))
			return;
	} while (time_before(jiffies, timeout));

	dev_err(ufs->dev, "timeout host sw-reset\n");
}

static void exynos_ufs_dev_hw_reset(struct ufs_hba *hba)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	/* bit[1] for resetn */
	hci_writel(ufs, 0 << 0, HCI_GPIO_OUT);
	udelay(5);
	hci_writel(ufs, 1 << 0, HCI_GPIO_OUT);
}

static void exynos_ufs_pre_hibern8(struct ufs_hba *hba, u8 enter)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (!enter)
		exynos_ufs_gate_clk(ufs, false);
}

static void exynos_ufs_post_hibern8(struct ufs_hba *hba, u8 enter)
{
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	if (!enter) {
		struct uic_pwr_mode *act_pmd = &ufs->act_pmd_parm;
		u32 mode;

		ufshcd_dme_get(hba, UIC_ARG_MIB(PA_PWRMODE), &mode);
		if (mode != (act_pmd->mode << 4 | act_pmd->mode)) {
			dev_warn(hba->dev, "%s: power mode change\n", __func__);
			ufshcd_config_pwr_mode(hba, &hba->max_pwr_info.info);
		}

		if (!(ufs->opts & EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB))
			exynos_ufs_establish_connt(ufs);
	} else {
		exynos_ufs_gate_clk(ufs, true);
	}
}

static int exynos_ufs_link_startup_notify(struct ufs_hba *hba, bool notify)
{
	int ret = 0;

	switch (notify) {
	case PRE_CHANGE:
		exynos_ufs_dev_hw_reset(hba);
		ret = exynos_ufs_pre_link(hba);
		break;
	case POST_CHANGE:
		ret = exynos_ufs_post_link(hba);
		break;
	default:
		break;
	}

	return ret;
}

static int exynos_ufs_pwr_mode_change_notify(struct ufs_hba *hba, bool notify,
					struct ufs_pa_layer_attr *pwr_max,
					struct ufs_pa_layer_attr *pwr_req)
{
	switch (notify) {
	case PRE_CHANGE:
		exynos_ufs_pre_prep_pmc(hba, pwr_max, pwr_req);
		break;
	case POST_CHANGE:
		exynos_ufs_post_prep_pmc(hba, pwr_max, pwr_req);
		break;
	default:
		break;
	}

	return 0;
}

static void exynos_ufs_hibern8_notify(struct ufs_hba *hba,
				u8 enter, bool notify)
{
	switch (notify) {
	case PRE_CHANGE:
		exynos_ufs_pre_hibern8(hba, enter);
		break;
	case POST_CHANGE:
		exynos_ufs_post_hibern8(hba, enter);
		break;
	default:
		break;
	}
}

static int __exynos_ufs_clk_set_parent(struct device *dev, const char *c, const char *p)
{
	struct clk *_c, *_p;

	_c = devm_clk_get(dev, c);
	if (IS_ERR(_c)) {
		dev_err(dev, "failed to get clock %s\n", c);
		return -EINVAL;
	}

	_p = devm_clk_get(dev, p);
	if (IS_ERR(_p)) {
		dev_err(dev, "failed to get clock %s\n", p);
		return -EINVAL;
	}

	return clk_set_parent(_c, _p);
}

static int exynos_ufs_clk_init(struct device *dev, struct exynos_ufs *ufs)
{
	int i, ret = 0;

	const char *const clks[] = {
		"mout_sclk_combo_phy_embedded", "top_sclk_phy_fsys1_26m",
	};

	ret = __exynos_ufs_clk_set_parent(dev, clks[0], clks[1]);
	if (ret) {
		dev_err(dev, "failed to set parent %s of clock %s\n",
				clks[1], clks[0]);
		return ret;
	}

	ufs->clk_hci = devm_clk_get(dev, "aclk_ufs");
	if (IS_ERR(ufs->clk_hci)) {
		dev_err(dev, "failed to get ufs clock\n");
	} else {
		ret = clk_prepare_enable(ufs->clk_hci);
		if (ret) {
			dev_err(dev, "failed to enable ufs clock\n");
			return ret;
		}

		dev_info(dev, "ufshci clock: %ld Hz \n", clk_get_rate(ufs->clk_hci));
	}

	ufs->clk_unipro = devm_clk_get(dev, "sclk_ufsunipro");
	if (IS_ERR(ufs->clk_unipro)) {
		dev_err(dev, "failed to get sclk_unipro clock\n");
	} else {
		ret = clk_prepare_enable(ufs->clk_unipro);
		if (ret) {
			dev_err(dev, "failed to enable unipro clock\n");
			goto err_aclk_ufs;
		}

		dev_info(dev, "unipro clock: %ld Hz \n", clk_get_rate(ufs->clk_unipro));
	}

	ufs->clk_refclk_1 = devm_clk_get(dev, "sclk_refclk_1");
	if (IS_ERR(ufs->clk_refclk_1)) {
		dev_err(dev, "failed to get sclk_refclk_1 clock\n");
	} else {
		ret = clk_prepare_enable(ufs->clk_refclk_1);
		if (ret) {
			dev_err(dev, "failed to enable refclk_1 clock\n");
			goto err_clk_unipro;
		}

		dev_info(dev, "refclk_1 clock: %ld Hz \n", clk_get_rate(ufs->clk_refclk_1));
	}

	ufs->clk_refclk_2 = devm_clk_get(dev, "sclk_refclk_2");
	if (IS_ERR(ufs->clk_refclk_2)) {
		dev_err(dev, "failed to get sclk_refclk_2 clock\n");
	} else {
		ret = clk_prepare_enable(ufs->clk_refclk_2);
		if (ret) {
			dev_err(dev, "failed to enable refclk_2 clock\n");
			goto err_clk_unipro;
		}

		dev_info(dev, "refclk_2 clock: %ld Hz \n", clk_get_rate(ufs->clk_refclk_2));
	}


	for (i = 0; i < ARRAY_SIZE(phy_symb_clks); i++) {
		ufs->clk_phy_symb[i] = devm_clk_get(dev, phy_symb_clks[i]);
		if (IS_ERR(ufs->clk_phy_symb[i])) {
			dev_err(dev, "failed to get %s clock\n",
				phy_symb_clks[i]);
		} else {
			ret = clk_prepare_enable(ufs->clk_phy_symb[i]);
			if (ret) {
				dev_err(dev, "failed to enable %s clock\n",
					phy_symb_clks[i]);
				goto err_clk_refclk;
			}
		}
	}

	return 0;

err_clk_refclk:
	for (i = 0; i < ARRAY_SIZE(phy_symb_clks); i++) {
		if (!IS_ERR_OR_NULL(ufs->clk_phy_symb[i]))
			clk_disable_unprepare(ufs->clk_phy_symb[i]);
	}

	if (!IS_ERR(ufs->clk_refclk_1))
		clk_disable_unprepare(ufs->clk_refclk_1);

err_clk_unipro:
	if (!IS_ERR(ufs->clk_unipro))
		clk_disable_unprepare(ufs->clk_unipro);

err_aclk_ufs:
	if (!IS_ERR(ufs->clk_hci))
		clk_disable_unprepare(ufs->clk_hci);

	return ret;
}

static int exynos_ufs_populate_dt_phy(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *ufs_phy, *phy_sys;
	struct exynos_ufs_phy *phy = &ufs->phy;
	struct resource io_res;
	int ret;

	ufs_phy = of_get_child_by_name(dev->of_node, "ufs-phy");
	if (!ufs_phy) {
		dev_err(dev, "failed to get ufs-phy node\n");
		return -ENODEV;
	}

	ret = of_address_to_resource(ufs_phy, 0, &io_res);
	if (ret) {
		dev_err(dev, "failed to get i/o address phy pma\n");
		goto err_0;
	}

	phy->reg_pma = devm_ioremap_resource(dev, &io_res);
	if (!phy->reg_pma) {
		dev_err(dev, "failed to ioremap for phy pma\n");
		ret = -ENOMEM;
		goto err_0;
	}

	phy_sys = of_get_child_by_name(ufs_phy, "ufs-phy-sys");
	if (!phy_sys) {
		dev_err(dev, "failed to get ufs-phy-sys node\n");
		ret = -ENODEV;
		goto err_0;
	}

	ret = of_address_to_resource(phy_sys, 0, &io_res);
	if (ret) {
		dev_err(dev, "failed to get i/o address ufs-phy pmu\n");
		goto err_1;
	}

	phy->reg_pmu = devm_ioremap_resource(dev, &io_res);
	if (!phy->reg_pmu) {
		dev_err(dev, "failed to ioremap for ufs-phy pmu\n");
		ret = -ENOMEM;
	}

	phy->soc = exynos_ufs_get_drv_data(dev);

err_1:
	of_node_put(phy_sys);
err_0:
	of_node_put(ufs_phy);

	return ret;
}

static int exynos_ufs_populate_dt(struct device *dev, struct exynos_ufs *ufs)
{
	struct device_node *np = dev->of_node;
	u32 freq[2];
	int ret;

	ret = exynos_ufs_populate_dt_phy(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to populate dt-phy\n");
		goto out;
	}

	ret = of_property_read_u32_array(np,
			"pclk-freq-avail-range",freq, ARRAY_SIZE(freq));
	if (!ret) {
		ufs->pclk_avail_min = freq[0];
		ufs->pclk_avail_max = freq[1];
	} else {
		dev_err(dev, "faild to get available pclk range\n");
		goto out;
	}

	if (of_find_property(np, "ufs-opts-skip-connection-estab", NULL))
		ufs->opts |= EXYNOS_UFS_OPTS_SKIP_CONNECTION_ESTAB;

out:
	return ret;
}

#ifdef CONFIG_CPU_IDLE
static int exynos_ufs_lp_event(struct notifier_block *nb, unsigned long event, void *data)
{
	struct exynos_ufs *ufs =
		container_of(nb, struct exynos_ufs, lpa_nb);
	int ret = NOTIFY_OK;

	switch (event) {
	case LPA_ENTER:
		exynos_ufs_ctrl_phy_pwr(ufs, false);
		break;
	case LPA_EXIT:
		exynos_ufs_ctrl_phy_pwr(ufs, true);
		break;
	default:
		ret = NOTIFY_DONE;
	}

	return notifier_from_errno(ret);
}
#endif

static u64 exynos_ufs_dma_mask = DMA_BIT_MASK(32);

static int exynos_ufs_probe(struct platform_device *pdev)
{
	const struct of_device_id *match;
	struct device *dev = &pdev->dev;
	struct exynos_ufs *ufs;
	struct resource *res;
	int ret;

	match = of_match_node(exynos_ufs_match, dev->of_node);
	if (!match)
		return -ENODEV;

	ufs = devm_kzalloc(dev, sizeof(*ufs), GFP_KERNEL);
	if (!ufs) {
		dev_err(dev, "cannot allocate mem for exynos-ufs\n");
		return -ENOMEM;
	}

	/* exynos-specific hci */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 1);
	ufs->reg_hci = devm_ioremap_resource(dev, res);
	if (!ufs->reg_hci) {
		dev_err(dev, "cannot ioremap for hci vendor register\n");
		return -ENOMEM;
	}

	/* unipro */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 2);
	ufs->reg_unipro = devm_ioremap_resource(dev, res);
	if (!ufs->reg_unipro) {
		dev_err(dev, "cannot ioremap for unipro register\n");
		return -ENOMEM;
	}

	/* ufs protector */
	res = platform_get_resource(pdev, IORESOURCE_MEM, 3);
	ufs->reg_ufsp = devm_ioremap_resource(dev, res);
	if (!ufs->reg_ufsp) {
		dev_err(dev, "cannot ioremap for ufs protector register\n");
		return -ENOMEM;
	}

	ret = exynos_ufs_populate_dt(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to get dt info.\n");
		return ret;
	}

	/* PHY power contorl */
	exynos_ufs_ctrl_phy_pwr(ufs, true);

	ret = exynos_ufs_clk_init(dev, ufs);
	if (ret) {
		dev_err(dev, "failed to clock initialization\n");
		return ret;
	}

#ifdef CONFIG_CPU_IDLE
	ufs->lpa_nb.notifier_call = exynos_ufs_lp_event;
	ufs->lpa_nb.next = NULL;
	ufs->lpa_nb.priority = 0;

	ret = exynos_pm_register_notifier(&ufs->lpa_nb);
	if (ret) {
		dev_err(dev, "failed to register low power mode notifier\n");
		return ret;
	}
#endif

	ufs->dev = dev;
	dev->platform_data = ufs;
	dev->dma_mask = &exynos_ufs_dma_mask;

	/* ufsp */
	exynos_ufs_config_smu(ufs);

	return ufshcd_pltfrm_init(pdev, match->data);
}

static int exynos_ufs_remove(struct platform_device *pdev)
{
	struct exynos_ufs *ufs = dev_get_platdata(&pdev->dev);
	int i;

	ufshcd_pltfrm_exit(pdev);

#ifdef CONFIG_CPU_IDLE
	exynos_pm_unregister_notifier(&ufs->lpa_nb);
#endif

	if (!IS_ERR(ufs->clk_refclk_1))
		clk_disable_unprepare(ufs->clk_refclk_1);


	for (i = 0; i < ARRAY_SIZE(phy_symb_clks); i++) {
		if (!IS_ERR(ufs->clk_phy_symb[i]))
			clk_disable_unprepare(ufs->clk_phy_symb[i]);
	}

	if (!IS_ERR(ufs->clk_unipro))
		clk_disable_unprepare(ufs->clk_unipro);

	if (!IS_ERR(ufs->clk_hci))
		clk_disable_unprepare(ufs->clk_hci);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int exynos_ufs_suspend(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);
	int ret;

	ret = ufshcd_system_suspend(hba);
	if (ret)
		return ret;

	disable_irq(hba->irq);

	exynos_ufs_ctrl_phy_pwr(ufs, false);

	return 0;
}

static int exynos_ufs_resume(struct device *dev)
{
	struct ufs_hba *hba = dev_get_drvdata(dev);
	struct exynos_ufs *ufs = to_exynos_ufs(hba);

	exynos_ufs_ctrl_phy_pwr(ufs, true);

	exynos_ufs_config_smu(ufs);

	enable_irq(hba->irq);

	return ufshcd_system_resume(hba);
}
#endif /* CONFIG_PM_SLEEP */

#ifdef CONFIG_PM
static const struct dev_pm_ops exynos_ufs_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(exynos_ufs_suspend, exynos_ufs_resume)
};

#define EXYNOS_UFS_PMOPS (&exynos_ufs_pmops)
#else
#define EXYNOS_UFS_PMOPS NULL
#endif

static const struct ufs_hba_variant_ops exynos_ufs_ops = {
	.host_reset = exynos_ufs_host_reset,
	.link_startup_notify = exynos_ufs_link_startup_notify,
	.pwr_change_notify = exynos_ufs_pwr_mode_change_notify,
	.hibern8_notify = exynos_ufs_hibern8_notify,
};

static const struct ufs_phy_cfg init_cfg[] = {
	{PA_DBG_OPTION_SUITE, 0x20103, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_DBG_AUTOMODE_THLD, 0x222E, PMD_ALL, UNIPRO_DBG_MIB},
	{0x00f, 0xfa, PMD_ALL, PHY_PMA_COMN},
	{0x010, 0x82, PMD_ALL, PHY_PMA_COMN},
	{0x011, 0x1e, PMD_ALL, PHY_PMA_COMN},
	{0x017, 0x84, PMD_ALL, PHY_PMA_COMN},
	{0x035, 0x58, PMD_ALL, PHY_PMA_TRSV},
	{0x036, 0x32, PMD_ALL, PHY_PMA_TRSV},
	{0x037, 0x40, PMD_ALL, PHY_PMA_TRSV},
	{0x03b, 0x83, PMD_ALL, PHY_PMA_TRSV},
	{0x042, 0x88, PMD_ALL, PHY_PMA_TRSV},
	{0x043, 0xa6, PMD_ALL, PHY_PMA_TRSV},
	{0x048, 0x74, PMD_ALL, PHY_PMA_TRSV},
	{0x04c, 0x5b, PMD_ALL, PHY_PMA_TRSV},
	{0x04d, 0x83, PMD_ALL, PHY_PMA_TRSV},
	{0x05c, 0x14, PMD_ALL, PHY_PMA_TRSV},
	{},
};

/* Calibration for PWM mode */
static const struct ufs_phy_cfg calib_of_pwm[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x376, 0x00, PMD_PWM, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{0x04d, 0x03, PMD_PWM, PHY_PMA_COMN},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_SAVECONFIGTIME, 0xbb8, PMD_ALL, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{UNIP_DBG_FORCE_DME_CTRL_STATE, 0x22, PMD_ALL, UNIPRO_DBG_APB},
	{},
};

/* Calibration for HS mode series A */
static const struct ufs_phy_cfg calib_of_hs_rate_a[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x362, 0xff, PMD_HS, PHY_PCS_RXTX},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{0x321, 0x40, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{0x00f, 0xfa, PMD_HS, PHY_PMA_COMN},
	{0x010, 0x82, PMD_HS, PHY_PMA_COMN},
	{0x011, 0x1e, PMD_HS, PHY_PMA_COMN},
	/* Setting order: 1st(0x16), 2nd(0x15) */
	{0x016, 0xff, PMD_HS, PHY_PMA_COMN},
	{0x015, 0x80, PMD_HS, PHY_PMA_COMN},
	{0x017, 0x84, PMD_HS, PHY_PMA_COMN},
	{0x036, 0x32, PMD_HS, PHY_PMA_TRSV},
	{0x037, 0x40, PMD_HS, PHY_PMA_TRSV},
	{0x038, 0x3f, PMD_HS, PHY_PMA_TRSV},
	{0x042, 0x88, PMD_HS, PHY_PMA_TRSV},
	{0x043, 0xa6, PMD_HS, PHY_PMA_TRSV},
	{0x048, 0x74, PMD_HS, PHY_PMA_TRSV},
	{0x034, 0x35, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{0x035, 0x5b, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_SAVECONFIGTIME, 0xbb8, PMD_ALL, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{UNIP_DBG_FORCE_DME_CTRL_STATE, 0x22, PMD_ALL, UNIPRO_DBG_APB},
	{},
};

/* Calibration for HS mode series B */
static const struct ufs_phy_cfg calib_of_hs_rate_b[] = {
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x362, 0xff, PMD_HS, PHY_PCS_RXTX},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{0x321, 0x35, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{0x00f, 0xfa, PMD_HS, PHY_PMA_COMN},
	{0x010, 0x82, PMD_HS, PHY_PMA_COMN},
	{0x011, 0x1e, PMD_HS, PHY_PMA_COMN},
	/* Setting order: 1st(0x16), 2nd(0x15) */
	{0x016, 0xff, PMD_HS, PHY_PMA_COMN},
	{0x015, 0x80, PMD_HS, PHY_PMA_COMN},
	{0x017, 0x84, PMD_HS, PHY_PMA_COMN},
	{0x036, 0x32, PMD_HS, PHY_PMA_TRSV},
	{0x037, 0x40, PMD_HS, PHY_PMA_TRSV},
	{0x038, 0x3f, PMD_HS, PHY_PMA_TRSV},
	{0x042, 0xbb, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{0x043, 0xa6, PMD_HS, PHY_PMA_TRSV},
	{0x048, 0x74, PMD_HS, PHY_PMA_TRSV},
	{0x034, 0x36, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{0x035, 0x5c, PMD_HS_G2_L1 | PMD_HS_G2_L2, PHY_PMA_TRSV},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_SAVECONFIGTIME, 0xbb8, PMD_ALL, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{UNIP_DBG_FORCE_DME_CTRL_STATE, 0x22, PMD_ALL, UNIPRO_DBG_APB},
	{},
};

/* Calibration for PWM mode atfer PMC */
static const struct ufs_phy_cfg post_calib_of_pwm[] = {
	{PA_DBG_RXPHY_CFGUPDT, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x363, 0x00, PMD_PWM, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{},
};

/* Calibration for HS mode series A atfer PMC */
static const struct ufs_phy_cfg post_calib_of_hs_rate_a[] = {
	{PA_DBG_RXPHY_CFGUPDT, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{0x015, 0x00, PMD_HS, PHY_PMA_COMN},
	{0x04d, 0x83, PMD_HS, PHY_PMA_TRSV},
	{0x41a, 0x00, PMD_HS_G3_L1 | PMD_HS_G3_L2, PHY_PCS_COMN},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_CONNECTEDTXDATALANES, 1, PMD_HS_G1_L1 | PMD_HS_G2_L1 | PMD_HS_G3_L1, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},
	{},
};

/* Calibration for HS mode series B after PMC*/
static const struct ufs_phy_cfg post_calib_of_hs_rate_b[] = {
	{PA_DBG_RXPHY_CFGUPDT, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{0x015, 0x00, PMD_HS, PHY_PMA_COMN},
	{0x04d, 0x83, PMD_HS, PHY_PMA_TRSV},
	{0x41a, 0x00, PMD_HS_G3_L1 | PMD_HS_G3_L2, PHY_PCS_COMN},
	{PA_DBG_OV_TM, true, PMD_ALL, PHY_PCS_COMN},
	{0x363, 0x00, PMD_HS, PHY_PCS_RXTX},
	{PA_DBG_OV_TM, false, PMD_ALL, PHY_PCS_COMN},
	{PA_DBG_MODE, 0x1, PMD_ALL, UNIPRO_DBG_MIB},
	{PA_CONNECTEDTXDATALANES, 1, PMD_HS_G1_L1 | PMD_HS_G2_L1 | PMD_HS_G3_L1, UNIPRO_STD_MIB},
	{PA_DBG_MODE, 0x0, PMD_ALL, UNIPRO_DBG_MIB},

	{},
};

static const struct exynos_ufs_soc exynos_ufs_soc_data = {
	.tbl_phy_init			= init_cfg,
	.tbl_calib_of_pwm		= calib_of_pwm,
	.tbl_calib_of_hs_rate_a		= calib_of_hs_rate_a,
	.tbl_calib_of_hs_rate_b		= calib_of_hs_rate_b,
	.tbl_post_calib_of_pwm		= post_calib_of_pwm,
	.tbl_post_calib_of_hs_rate_a	= post_calib_of_hs_rate_a,
	.tbl_post_calib_of_hs_rate_b	= post_calib_of_hs_rate_b,
};

static const struct ufs_hba_variant exynos_ufs_drv_data = {
	.ops		= &exynos_ufs_ops,
	.quirks		= UFSHCI_QUIRK_BROKEN_DWORD_UTRD |
			  UFSHCI_QUIRK_BROKEN_REQ_LIST_CLR |
			  UFSHCI_QUIRK_USE_OF_HCE,
	.vs_data	= &exynos_ufs_soc_data,
};

static const struct of_device_id exynos_ufs_match[] = {
	{ .compatible = "samsung,exynos-ufs",
			.data = &exynos_ufs_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, exynos_ufs_match);

static struct platform_driver exynos_ufs_driver = {
	.driver = {
		.name = "exynos-ufs",
		.owner = THIS_MODULE,
		.pm = EXYNOS_UFS_PMOPS,
		.of_match_table = exynos_ufs_match,
	},
	.probe = exynos_ufs_probe,
	.remove = exynos_ufs_remove,
};

module_platform_driver(exynos_ufs_driver);
MODULE_DESCRIPTION("Exynos Specific UFSHCI driver");
MODULE_AUTHOR("Seungwon Jeon <tgih.jun@samsung.com>");
MODULE_LICENSE("GPL");
