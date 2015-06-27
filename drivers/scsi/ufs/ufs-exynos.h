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

#ifndef _UFS_EXYNOS_H_
#define _UFS_EXYNOS_H_

/*
 * Exynos's Vendor specific registers for UFSHCI
 */
#define HCI_TXPRDT_ENTRY_SIZE		0x00
#define HCI_RXPRDT_ENTRY_SIZE		0x04
#define HCI_TO_CNT_DIV_VAL		0x08
#define HCI_40US_TO_CNT_VAL		0x0C
#define HCI_INVALID_UPIU_CTRL		0x10
#define HCI_INVALID_UPIU_BADDR		0x14
#define HCI_INVALID_UPIU_UBADDR		0x18
#define HCI_INVALID_UTMR_OFFSET_ADDR	0x1C
#define HCI_INVALID_UTR_OFFSET_ADDR	0x20
#define HCI_INVALID_DIN_OFFSET_ADDR	0x24
#define HCI_DBR_TIMER_CONFIG		0x28
#define HCI_DBR_TIMER_STATUS		0x2C
#define HCI_VENDOR_SPECIFIC_IS		0x38
#define HCI_VENDOR_SPECIFIC_IE		0x3C
#define HCI_UTRL_NEXUS_TYPE		0x40
#define HCI_UMTRL_NEXUS_TYPE		0x44
#define HCI_E2EFC_CTRL			0x48
#define HCI_SW_RST			0x50
#define HCI_LINK_VERSION		0x54
#define HCI_IDLE_TIMER_CONFIG		0x58
#define HCI_RX_UPIU_MATCH_ERROR_CODE	0x5C
#define HCI_DATA_REORDER		0x60
#define HCI_MAX_DOUT_DATA_SIZE		0x64
#define HCI_UNIPRO_APB_CLK_CTRL		0x68
#define HCI_AXIDMA_RWDATA_BURST_LEN	0x6C
#define HCI_GPIO_OUT			0x70
#define HCI_WRITE_DMA_CTRL		0x74
#define HCI_ERROR_EN_PA_LAYER		0x78
#define HCI_ERROR_EN_DL_LAYER		0x7C
#define HCI_ERROR_EN_N_LAYER		0x80
#define HCI_ERROR_EN_T_LAYER		0x84
#define HCI_ERROR_EN_DME_LAYER		0x88
#define HCI_REQ_HOLD_EN			0xAC
#define HCI_CLKSTOP_CTRL		0xB0
#define HCI_FORCE_HCS			0xB4

/* Device fatal error */
#define DFES_ERR_EN	BIT(31)
#define DFES_DEF_DL_ERRS	(UIC_DATA_LINK_LAYER_ERROR_TCX_REP_TIMER_EXP |\
				 UIC_DATA_LINK_LAYER_ERROR_RX_BUF_OF |\
				 UIC_DATA_LINK_LAYER_ERROR_PA_INIT)


/* TXPRDT defines */
#define PRDT_PREFECT_EN		BIT(31)
#define PRDT_SET_SIZE(x)	((x) & 0x1F)
enum {
	DFES_ERR_PA = 0,
	DEFS_ERR_DL,
	DEFS_ERR_N,
	DEFS_ERR_T,
	DEFS_ERR_DME,
};

/*
 * UNIPRO registers
 */
#define UNIP_COMP_VERSION			0x000
#define UNIP_COMP_INFO				0x004
#define UNIP_COMP_RESET				0x010
#define UNIP_DME_POWERON_REQ			0x040
#define UNIP_DME_POWERON_CNF_RESULT		0x044
#define UNIP_DME_POWEROFF_REQ			0x048
#define UNIP_DME_POWEROFF_CNF_RESULT		0x04C
#define UNIP_DME_RESET_REQ			0x050
#define UNIP_DME_RESET_REQ_LEVEL		0x054
#define UNIP_DME_ENABLE_REQ			0x058
#define UNIP_DME_ENABLE_CNF_RESULT		0x05C
#define UNIP_DME_ENDPOINTRESET_REQ		0x060
#define UNIP_DME_ENDPOINTRESET_CNF_RESULT	0x064
#define UNIP_DME_LINKSTARTUP_REQ		0x068
#define UNIP_DME_LINKSTARTUP_CNF_RESULT		0x06C
#define UNIP_DME_HIBERN8_ENTER_REQ		0x070
#define UNIP_DME_HIBERN8_ENTER_CNF_RESULT	0x074
#define UNIP_DME_HIBERN8_ENTER_IND_RESULT	0x078
#define UNIP_DME_HIBERN8_EXIT_REQ		0x080
#define UNIP_DME_HIBERN8_EXIT_CNF_RESULT	0x084
#define UNIP_DME_HIBERN8_EXIT_IND_RESULT	0x088
#define UNIP_DME_PWR_REQ			0x090
#define UNIP_DME_PWR_REQ_POWERMODE		0x094
#define UNIP_DME_PWR_REQ_LOCALL2TIMER0		0x098
#define UNIP_DME_PWR_REQ_LOCALL2TIMER1		0x09C
#define UNIP_DME_PWR_REQ_LOCALL2TIMER2		0x0A0
#define UNIP_DME_PWR_REQ_REMOTEL2TIMER0		0x0A4
#define UNIP_DME_PWR_REQ_REMOTEL2TIMER1		0x0A8
#define UNIP_DME_PWR_REQ_REMOTEL2TIMER2		0x0AC
#define UNIP_DME_PWR_CNF_RESULT			0x0B0
#define UNIP_DME_PWR_IND_RESULT			0x0B4
#define UNIP_DME_TEST_MODE_REQ			0x0B8
#define UNIP_DME_TEST_MODE_CNF_RESULT		0x0BC
#define UNIP_DME_ERROR_IND_LAYER		0x0C0
#define UNIP_DME_ERROR_IND_ERRCODE		0x0C4
#define UNIP_DME_PACP_CNFBIT			0x0C8
#define UNIP_DME_DL_FRAME_IND			0x0D0
#define UNIP_DME_INTR_STATUS			0x0E0
#define UNIP_DME_INTR_ENABLE			0x0E4
#define UNIP_DME_GETSET_ADDR			0x100
#define UNIP_DME_GETSET_WDATA			0x104
#define UNIP_DME_GETSET_RDATA			0x108
#define UNIP_DME_GETSET_CONTROL			0x10C
#define UNIP_DME_GETSET_RESULT			0x110
#define UNIP_DME_PEER_GETSET_ADDR		0x120
#define UNIP_DME_PEER_GETSET_WDATA		0x124
#define UNIP_DME_PEER_GETSET_RDATA		0x128
#define UNIP_DME_PEER_GETSET_CONTROL		0x12C
#define UNIP_DME_PEER_GETSET_RESULT		0x130

/*
 * UFS Protector registers
 */
#define UFSPRCTRL	0x000
#define UFSPRSTAT	0x008
#define UFSPRSECURITY	0x010
#define UFSPRENCKEY0	0x020
#define UFSPRENCKEY1	0x024
#define UFSPRENCKEY2	0x028
#define UFSPRENCKEY3	0x02C
#define UFSPRENCKEY4	0x030
#define UFSPRENCKEY5	0x034
#define UFSPRENCKEY6	0x038
#define UFSPRENCKEY7	0x03C
#define UFSPRTWKEY0	0x040
#define UFSPRTWKEY1	0x044
#define UFSPRTWKEY2	0x048
#define UFSPRTWKEY3	0x04C
#define UFSPRTWKEY4	0x050
#define UFSPRTWKEY5	0x054
#define UFSPRTWKEY6	0x058
#define UFSPRTWKEY7	0x05C
#define UFSPWCTRL	0x100
#define UFSPWSTAT	0x108
#define UFSPWSECURITY	0x110
#define UFSPWENCKEY0	0x120
#define UFSPWENCKEY1	0x124
#define UFSPWENCKEY2	0x128
#define UFSPWENCKEY3	0x12C
#define UFSPWENCKEY4	0x130
#define UFSPWENCKEY5	0x134
#define UFSPWENCKEY6	0x138
#define UFSPWENCKEY7	0x13C
#define UFSPWTWKEY0	0x140
#define UFSPWTWKEY1	0x144
#define UFSPWTWKEY2	0x148
#define UFSPWTWKEY3	0x14C
#define UFSPWTWKEY4	0x150
#define UFSPWTWKEY5	0x154
#define UFSPWTWKEY6	0x158
#define UFSPWTWKEY7	0x15C
#define UFSPSBEGIN0	0x200
#define UFSPSEND0	0x204
#define UFSPSCTRL0	0x20C
#define UFSPSBEGIN1	0x210
#define UFSPSEND1	0x214
#define UFSPSCTRL1	0x21C
#define UFSPSBEGIN2	0x220
#define UFSPSEND2	0x224
#define UFSPSCTRL2	0x22C
#define UFSPSBEGIN3	0x230
#define UFSPSEND3	0x234
#define UFSPSCTRL3	0x23C
#define UFSPSBEGIN4	0x240
#define UFSPSEND4	0x244
#define UFSPSCTRL4	0x24C
#define UFSPSBEGIN5	0x250
#define UFSPSEND5	0x254
#define UFSPSCTRL5	0x25C
#define UFSPSBEGIN6	0x260
#define UFSPSEND6	0x264
#define UFSPSCTRL6	0x26C
#define UFSPSBEGIN7	0x270
#define UFSPSEND7	0x274
#define UFSPSCTRL7	0x27C

/*
 * MIBs for PA debug registers
 */
#define PA_DBG_CLK_PERIOD		0x1614
#define PA_DBG_TXPHY_CFGUPDT		0x1618
#define PA_DBG_RXPHY_CFGUPDT		0x1619
#define PA_DBG_TX_PHY_LATENCY		0x162C
#define PA_DBG_OV_TM			0x1640
#define PA_DBG_MIB_CAP_SEL		0x1646
#define PA_DBG_RESUME_HIBERN8		0x1650
#define PA_DBG_TX_PHY_LATENCY_EN	0x1652
#define PA_DBG_IGNORE_INCOMING		0x1659
#define PA_DBG_LINE_RESET_THLD		0x1661

/*
 * Exynos MPHY attributes
 */
#define TX_LINERESETNVALUE		0x0277
#define TX_LINERESETPVALUE		0x027D
#define TX_OV_SLEEP_CNT_TIMER		0x028E

#define RX_FILLER_ENABLE		0x0316
#define RX_LCC_IGNORE			0x0318
#define RX_LINERESETVALUE		0x0317
#define RX_HIBERN8_WAIT_VAL_BIT_20_16	0x0331
#define RX_HIBERN8_WAIT_VAL_BIT_15_08	0x0332
#define RX_HIBERN8_WAIT_VAL_BIT_07_00	0x0333

#define CMN_REFCLK_PLL_LOCK		0x0406
#define CMN_REFCLK_STREN		0x044C
#define CMN_REFCLK_OUT			0x044E
#define CMN_REFCLK_SEL_PLL		0x044F

/*
 * Driver specific definitions
 */

enum {
	PHY_CFG_NONE = 0,
	PHY_PCS_COMN,
	PHY_PCS_RXTX,
	PHY_PMA_COMN,
	PHY_PMA_TRSV,
};

enum {
	__PMD_PWM_G1_L1,
	__PMD_PWM_G1_L2,
	__PMD_PWM_G2_L1,
	__PMD_PWM_G2_L2,
	__PMD_PWM_G3_L1,
	__PMD_PWM_G3_L2,
	__PMD_PWM_G4_L1,
	__PMD_PWM_G4_L2,
	__PMD_PWM_G5_L1,
	__PMD_PWM_G5_L2,
	__PMD_HS_G1_L1,
	__PMD_HS_G1_L2,
	__PMD_HS_G2_L1,
	__PMD_HS_G2_L2,
	__PMD_HS_G3_L1,
	__PMD_HS_G3_L2,
};

#define PMD_PWM_G1_L1	(1U << __PMD_PWM_G1_L1)
#define PMD_PWM_G1_L2	(1U << __PMD_PWM_G1_L2)
#define PMD_PWM_G2_L1	(1U << __PMD_PWM_G2_L1)
#define PMD_PWM_G2_L2	(1U << __PMD_PWM_G2_L2)
#define PMD_PWM_G3_L1	(1U << __PMD_PWM_G3_L1)
#define PMD_PWM_G3_L2	(1U << __PMD_PWM_G3_L2)
#define PMD_PWM_G4_L1	(1U << __PMD_PWM_G4_L1)
#define PMD_PWM_G4_L2	(1U << __PMD_PWM_G4_L2)
#define PMD_PWM_G5_L1	(1U << __PMD_PWM_G5_L1)
#define PMD_PWM_G5_L2	(1U << __PMD_PWM_G5_L2)
#define PMD_HS_G1_L1	(1U << __PMD_HS_G1_L1)
#define PMD_HS_G1_L2	(1U << __PMD_HS_G1_L2)
#define PMD_HS_G2_L1	(1U << __PMD_HS_G2_L1)
#define PMD_HS_G2_L2	(1U << __PMD_HS_G2_L2)
#define PMD_HS_G3_L1	(1U << __PMD_HS_G3_L1)
#define PMD_HS_G3_L2	(1U << __PMD_HS_G3_L2)

#define PMD_ALL		(PMD_HS_G3_L2 - 1)
#define PMD_PWM		(PMD_PWM_G4_L2 - 1)
#define PMD_HS		(PMD_ALL ^ PMD_PWM)

struct ufs_phy_cfg {
	u32 addr;
	u32 val;
	u32 flg;
	u32 lyr;
};

struct exynos_ufs_soc {
	const struct ufs_phy_cfg *tbl_phy_init;
	const struct ufs_phy_cfg *tbl_calib_of_pwm;
	const struct ufs_phy_cfg *tbl_calib_of_hs_rate_a;
	const struct ufs_phy_cfg *tbl_calib_of_hs_rate_b;
	const struct ufs_phy_cfg *tbl_post_calib_of_pwm;
	const struct ufs_phy_cfg *tbl_post_calib_of_hs_rate_a;
	const struct ufs_phy_cfg *tbl_post_calib_of_hs_rate_b;
};

struct exynos_ufs_phy {
	void __iomem *reg_pma;
	void __iomem *reg_pmu;
	const struct exynos_ufs_soc *soc;
};

struct uic_pwr_mode {
	u8 lane;
	u8 gear;
	u8 mode;
	u8 hs_series;
};

struct exynos_ufs {
	struct device *dev;
	struct ufs_hba *hba;

	void __iomem *reg_hci;
	void __iomem *reg_unipro;
	void __iomem *reg_ufsp;

	struct clk *clk_hci;
	struct clk *clk_unipro;
	struct clk *clk_refclk;
	struct clk *clk_phy_symb[4];
	long pclk_rate;

	int avail_ln_rx;
	int avail_ln_tx;

	struct exynos_ufs_phy phy;
	struct uic_pwr_mode req_pmd_parm;
	struct uic_pwr_mode act_pmd_parm;

};

struct phy_tm_parm {
	u32 tx_linereset_p;
	u32 tx_linereset_n;
	u32 rx_linereset;
	u32 rx_hibern8_wait;
};

#endif /* _UFS_EXYNOS_H_ */
