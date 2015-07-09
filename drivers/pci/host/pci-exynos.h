/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __PCIE_EXYNOS_H
#define __PCIE_EXYNOS_H

#define MAX_TIMEOUT		2000
#define ID_MASK			0xffff
#define MAX_RC_NUM		2

#define to_exynos_pcie(x)	container_of(x, struct exynos_pcie, pp)

struct exynos_pcie_clks {
	struct clk	*pcie_clks[10];
	struct clk	*phy_clks[3];
};

enum exynos_pcie_state {
	STATE_LINK_DOWN = 0,
	STATE_LINK_UP_TRY,
	STATE_LINK_DOWN_TRY,
	STATE_LINK_UP,
};

struct exynos_pcie {
	void __iomem		*elbi_base;
	void __iomem		*phy_base;
	void __iomem		*block_base;
	void __iomem		*rc_dbi_base;
	void __iomem		*phy_pcs_base;
	struct regmap		*pmureg;
	int			perst_gpio;
	int			eint_flag;
	int			pcie_tpoweron_max;
	int			ch_num;
	int			pcie_clk_num;
	int			phy_clk_num;
	enum exynos_pcie_state	state;
	int			probe_ok;
	int			l1ss_enable;
	int			d0uninit_cnt;
	int			idle_ip_index;
	bool			use_msi;
	bool			lpc_checking;
	struct workqueue_struct	*pcie_wq;
	struct exynos_pcie_clks	clks;
	struct pcie_port	pp;
	struct pci_dev		*pci_dev;
	struct pci_saved_state	*pci_saved_configs;
	struct notifier_block	lpa_nb;
	struct mutex		lock;
	struct delayed_work	work;
#ifdef CONFIG_PCI_EXYNOS_TEST
	int			wlan_gpio;
	int			bt_gpio;
#endif
};

/* PCIe ELBI registers */
#define PCIE_IRQ_PULSE			0x000
#define IRQ_INTA_ASSERT			(0x1 << 0)
#define IRQ_INTB_ASSERT			(0x1 << 2)
#define IRQ_INTC_ASSERT			(0x1 << 4)
#define IRQ_INTD_ASSERT			(0x1 << 6)
#define IRQ_RADM_PM_TO_ACK		(0x1 << 18)
#define IRQ_L1_EXIT			(0x1 << 24)
#define PCIE_IRQ_LEVEL			0x004
#define IRQ_MSI_CTRL			(0x1 << 1)
#define PCIE_IRQ_SPECIAL		0x008
#define PCIE_IRQ_EN_PULSE		0x00c
#define PCIE_IRQ_EN_LEVEL		0x010
#define IRQ_MSI_ENABLE			(0x1 << 1)
#define IRQ_LINK_DOWN			(0x1 << 30)
#define IRQ_LINKDOWN_ENABLE		(0x1 << 30)
#define PCIE_IRQ_EN_SPECIAL		0x014
#define PCIE_SW_WAKE			0x018
#define PCIE_APP_LTSSM_ENABLE		0x02c
#define PCIE_L1_BUG_FIX_ENABLE		0x038
#define PCIE_APP_REQ_EXIT_L1		0x040
#define PCIE_ELBI_RDLH_LINKUP		0x074
#define PCIE_ELBI_LTSSM_DISABLE		0x0
#define PCIE_ELBI_LTSSM_ENABLE		0x1
#define PCIE_PM_DSTATE			0x88
#define PCIE_D0_UNINIT_STATE		0x4
#define PCIE_APP_REQ_EXIT_L1_MODE	0xF4
#define PCIE_HISTORY_REG(x)		(0x138 + ((x) * 0x4))
#define LTSSM_STATE(x)			(((x) >> 16) & 0x3f)
#define PM_DSTATE(x)			(((x) >> 8) & 0x7)
#define L1SUB_STATE(x)			(((x) >> 0) & 0x7)
#define PCIE_LINKDOWN_RST_CTRL_SEL	0x1B8
#define PCIE_LINKDOWN_RST_MANUAL	(0x1 << 1)
#define PCIE_LINKDOWN_RST_FSM		(0x1 << 0)
#define PCIE_SOFT_AUXCLK_SEL_CTRL	0x1C4
#define CORE_CLK_GATING			(0x1 << 0)
#define PCIE_SOFT_CORE_RESET		0x1D0
#define PCIE_STATE_HISTORY_CHECK	0x274
#define HISTORY_BUFFER_ENABLE		(0x1 << 0)
#define HISTORY_BUFFER_CLEAR		(0x1 << 1)

/* PCIe PMU registers */
#define PCIE_PHY_CONTROL		0x071C
#define PCIE_PHY_CONTROL_MASK		0x1

#endif
