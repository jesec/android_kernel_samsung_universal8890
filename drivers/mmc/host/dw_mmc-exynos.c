/*
 * Exynos Specific Extensions for Synopsys DW Multimedia Card Interface driver
 *
 * Copyright (C) 2012, Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/mmc/host.h>
#include <linux/mmc/dw_mmc.h>
#include <linux/mmc/mmc.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>

#include "dw_mmc.h"
#include "dw_mmc-pltfm.h"
#include "dw_mmc-exynos.h"

void dw_mci_reg_dump(struct dw_mci *host)
{

	u32 reg;

	dev_err(host->dev, ": ============== REGISTER DUMP ==============\n");
	dev_err(host->dev, ": CTRL:	 0x%08x\n", mci_readl(host, CTRL));
	dev_err(host->dev, ": PWREN:	 0x%08x\n", mci_readl(host, PWREN));
	dev_err(host->dev, ": CLKDIV:	 0x%08x\n", mci_readl(host, CLKDIV));
	dev_err(host->dev, ": CLKSRC:	 0x%08x\n", mci_readl(host, CLKSRC));
	dev_err(host->dev, ": CLKENA:	 0x%08x\n", mci_readl(host, CLKENA));
	dev_err(host->dev, ": TMOUT:	 0x%08x\n", mci_readl(host, TMOUT));
	dev_err(host->dev, ": CTYPE:	 0x%08x\n", mci_readl(host, CTYPE));
	dev_err(host->dev, ": BLKSIZ:	 0x%08x\n", mci_readl(host, BLKSIZ));
	dev_err(host->dev, ": BYTCNT:	 0x%08x\n", mci_readl(host, BYTCNT));
	dev_err(host->dev, ": INTMSK:	 0x%08x\n", mci_readl(host, INTMASK));
	dev_err(host->dev, ": CMDARG:	 0x%08x\n", mci_readl(host, CMDARG));
	dev_err(host->dev, ": CMD:	 0x%08x\n", mci_readl(host, CMD));
	dev_err(host->dev, ": RESP0:	 0x%08x\n", mci_readl(host, RESP0));
	dev_err(host->dev, ": RESP1:	 0x%08x\n", mci_readl(host, RESP1));
	dev_err(host->dev, ": RESP2:	 0x%08x\n", mci_readl(host, RESP2));
	dev_err(host->dev, ": RESP3:	 0x%08x\n", mci_readl(host, RESP3));
	dev_err(host->dev, ": MINTSTS:	 0x%08x\n", mci_readl(host, MINTSTS));
	dev_err(host->dev, ": RINTSTS:	 0x%08x\n", mci_readl(host, RINTSTS));
	dev_err(host->dev, ": STATUS:	 0x%08x\n", mci_readl(host, STATUS));
	dev_err(host->dev, ": FIFOTH:	 0x%08x\n", mci_readl(host, FIFOTH));
	dev_err(host->dev, ": CDETECT:	 0x%08x\n", mci_readl(host, CDETECT));
	dev_err(host->dev, ": WRTPRT:	 0x%08x\n", mci_readl(host, WRTPRT));
	dev_err(host->dev, ": GPIO:	 0x%08x\n", mci_readl(host, GPIO));
	dev_err(host->dev, ": TCBCNT:	 0x%08x\n", mci_readl(host, TCBCNT));
	dev_err(host->dev, ": TBBCNT:	 0x%08x\n", mci_readl(host, TBBCNT));
	dev_err(host->dev, ": DEBNCE:	 0x%08x\n", mci_readl(host, DEBNCE));
	dev_err(host->dev, ": USRID:	 0x%08x\n", mci_readl(host, USRID));
	dev_err(host->dev, ": VERID:	 0x%08x\n", mci_readl(host, VERID));
	dev_err(host->dev, ": HCON:	 0x%08x\n", mci_readl(host, HCON));
	dev_err(host->dev, ": UHS_REG:	 0x%08x\n", mci_readl(host, UHS_REG));
	dev_err(host->dev, ": BMOD:	 0x%08x\n", mci_readl(host, BMOD));
	dev_err(host->dev, ": PLDMND:	 0x%08x\n", mci_readl(host, PLDMND));
	dev_err(host->dev, ": DBADDRL:	 0x%08x\n", mci_readl(host, DBADDRL));
	dev_err(host->dev, ": DBADDRU:	 0x%08x\n", mci_readl(host, DBADDRU));
	dev_err(host->dev, ": DSCADDRL:	 0x%08x\n", mci_readl(host, DSCADDRL));
	dev_err(host->dev, ": DSCADDRU:	 0x%08x\n", mci_readl(host, DSCADDRU));
	dev_err(host->dev, ": BUFADDR:	 0x%08x\n", mci_readl(host, BUFADDR));
	dev_err(host->dev, ": BUFADDRU:	 0x%08x\n", mci_readl(host, BUFADDRU));
	dev_err(host->dev, ": DBADDR:	 0x%08x\n", mci_readl(host, DBADDR));
	dev_err(host->dev, ": DSCADDR:	 0x%08x\n", mci_readl(host, DSCADDR));
	dev_err(host->dev, ": BUFADDR:	 0x%08x\n", mci_readl(host, BUFADDR));
	dev_err(host->dev, ": IDSTS:	 0x%08x\n", mci_readl(host, IDSTS));
	dev_err(host->dev, ": IDSTS64:	 0x%08x\n", mci_readl(host, IDSTS64));
	dev_err(host->dev, ": IDINTEN:	 0x%08x\n", mci_readl(host, IDINTEN));
	dev_err(host->dev, ": IDINTEN64: 0x%08x\n", mci_readl(host, IDINTEN64));
	dev_err(host->dev, ": EMMCP_BASE:0x%08x\n", mci_readl(host, EMMCP_BASE));
	dev_err(host->dev, ": MPSECURITY:0x%08x\n", mci_readl(host, MPSECURITY));
	dev_err(host->dev, ": MPSTAT:	 0x%08x\n", mci_readl(host, MPSTAT));
	dev_err(host->dev, ": DDR200_RDDQS_EN:	0x%08x\n",
		mci_readl(host, DDR200_RDDQS_EN));
	dev_err(host->dev, ": DDR200_ASYNC_FIFO_CTRL:	0x%08x\n",
		mci_readl(host, DDR200_ASYNC_FIFO_CTRL));
	dev_err(host->dev, ": DDR200_DLINE_CTRL:	0x%08x\n",
		mci_readl(host, DDR200_DLINE_CTRL));
	dev_err(host->dev, ": ============== STATUS DUMP ================\n");
	dev_err(host->dev, ": cmd_status:      0x%08x\n", host->cmd_status);
	dev_err(host->dev, ": data_status:     0x%08x\n", host->data_status);
	dev_err(host->dev, ": pending_events:  0x%08lx\n", host->pending_events);
	dev_err(host->dev, ": completed_events:0x%08lx\n", host->completed_events);
	reg = mci_readl(host, CMD);
	dev_err(host->dev, ": ================= CMD REG =================\n");
	dev_err(host->dev, ": read/write        : %s\n",
					(reg & (0x1 << 10)) ? "write" : "read");
	dev_err(host->dev, ": data expected     : %d\n", (reg >> 9) & 0x1);
	dev_err(host->dev, ": cmd index         : %d\n", (reg >> 0) & 0x3f);
	reg = mci_readl(host, STATUS);
	dev_err(host->dev, ": ================ STATUS REG ===============\n");
	dev_err(host->dev, ": fifocount         : %d\n", (reg >> 17) & 0x1fff);
	dev_err(host->dev, ": response index    : %d\n", (reg >> 11) & 0x3f);
	dev_err(host->dev, ": data state mc busy: %d\n", (reg >> 10) & 0x1);
	dev_err(host->dev, ": data busy         : %d\n", (reg >> 9) & 0x1);
	dev_err(host->dev, ": data 3 state      : %d\n", (reg >> 8) & 0x1);
	dev_err(host->dev, ": command fsm state : %d\n", (reg >> 4) & 0xf);
	dev_err(host->dev, ": fifo full         : %d\n", (reg >> 3) & 0x1);
	dev_err(host->dev, ": fifo empty        : %d\n", (reg >> 2) & 0x1);
	dev_err(host->dev, ": fifo tx watermark : %d\n", (reg >> 1) & 0x1);
	dev_err(host->dev, ": fifo rx watermark : %d\n", (reg >> 0) & 0x1);
	dev_err(host->dev, ": ===========================================\n");
}

/* Variations in Exynos specific dw-mshc controller */
enum dw_mci_exynos_type {
	DW_MCI_TYPE_EXYNOS4210,
	DW_MCI_TYPE_EXYNOS4412,
	DW_MCI_TYPE_EXYNOS5250,
	DW_MCI_TYPE_EXYNOS5420,
	DW_MCI_TYPE_EXYNOS5420_SMU,
	DW_MCI_TYPE_EXYNOS8890,
};

static struct dw_mci_exynos_compatible {
	char				*compatible;
	enum dw_mci_exynos_type		ctrl_type;
} exynos_compat[] = {
	{
		.compatible	= "samsung,exynos4210-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS4210,
	}, {
		.compatible	= "samsung,exynos4412-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS4412,
	}, {
		.compatible	= "samsung,exynos5250-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS5250,
	}, {
		.compatible	= "samsung,exynos5420-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS5420,
	}, {
		.compatible	= "samsung,exynos5420-dw-mshc-smu",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS5420_SMU,
	}, {
		.compatible	= "samsung,exynos8890-dw-mshc",
		.ctrl_type	= DW_MCI_TYPE_EXYNOS8890,
	},
};

static int dw_mci_exynos_priv_init(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;

	priv->saved_strobe_ctrl = mci_readl(host, HS400_DLINE_CTRL);
	priv->saved_dqs_en = mci_readl(host, HS400_DQS_EN);
	priv->saved_dqs_en |= AXI_NON_BLOCKING_WR;
	mci_writel(host, HS400_DQS_EN, priv->saved_dqs_en);
	if (!priv->dqs_delay)
		priv->dqs_delay =
			DQS_CTRL_GET_RD_DELAY(priv->saved_strobe_ctrl);

	return 0;
}

static void dw_mci_exynos_set_clksel_timing(struct dw_mci *host, u32 timing)
{
	u32 clksel;

	clksel = mci_readl(host, CLKSEL);
	clksel = (clksel & ~SDMMC_CLKSEL_TIMING_MASK) | timing;
	mci_writel(host, CLKSEL, clksel);
}

static inline u8 dw_mci_exynos_get_ciu_div(struct dw_mci *host)
{
	return SDMMC_CLKSEL_GET_DIV(mci_readl(host, CLKSEL)) + 1;
}

static int dw_mci_exynos_setup_clock(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	unsigned long rate = clk_get_rate(host->ciu_clk);

	host->bus_hz = rate / (priv->ciu_div + 1);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int dw_mci_exynos_suspend(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);

	return dw_mci_suspend(host);
}

static int dw_mci_exynos_resume(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);

	dw_mci_exynos_priv_init(host);
	return dw_mci_resume(host);
}

/**
 * dw_mci_exynos_resume_noirq - Exynos-specific resume code
 *
 * On exynos5420 there is a silicon errata that will sometimes leave the
 * WAKEUP_INT bit in the CLKSEL register asserted.  This bit is 1 to indicate
 * that it fired and we can clear it by writing a 1 back.  Clear it to prevent
 * interrupts from going off constantly.
 *
 * We run this code on all exynos variants because it doesn't hurt.
 */

static int dw_mci_exynos_resume_noirq(struct device *dev)
{
	struct dw_mci *host = dev_get_drvdata(dev);
	u32 clksel;

	clksel = mci_readl(host, CLKSEL);
	if (clksel & SDMMC_CLKSEL_WAKEUP_INT)
		mci_writel(host, CLKSEL, clksel);

	return 0;
}
#else
#define dw_mci_exynos_suspend		NULL
#define dw_mci_exynos_resume		NULL
#define dw_mci_exynos_resume_noirq	NULL
#endif /* CONFIG_PM_SLEEP */

static void dw_mci_exynos_prepare_command(struct dw_mci *host, u32 *cmdr)
{
	/*
	 * Exynos4412 and Exynos5250 extends the use of CMD register with the
	 * use of bit 29 (which is reserved on standard MSHC controllers) for
	 * optionally bypassing the HOLD register for command and data. The
	 * HOLD register should be bypassed in case there is no phase shift
	 * applied on CMD/DATA that is sent to the card.
	 */
	if (SDMMC_CLKSEL_GET_DRV_WD3(mci_readl(host, CLKSEL)))
		*cmdr |= SDMMC_CMD_USE_HOLD_REG;
}

static void dw_mci_exynos_config_hs400(struct dw_mci *host, u32 timing)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	u32 dqs, strobe;

	/*
	 * Not supported to configure register
	 * related to HS400
	 */
	if (priv->ctrl_type < DW_MCI_TYPE_EXYNOS5420)
		return;

	dqs = priv->saved_dqs_en;
	strobe = priv->saved_strobe_ctrl;

	if (timing == MMC_TIMING_MMC_HS400) {
		dqs |= DATA_STROBE_EN;
		strobe = DQS_CTRL_RD_DELAY(strobe, priv->dqs_delay);
	} else {
		dqs &= ~DATA_STROBE_EN;
	}

	mci_writel(host, HS400_DQS_EN, dqs);
	mci_writel(host, HS400_DLINE_CTRL, strobe);
}

static void dw_mci_exynos_adjust_clock(struct dw_mci *host, unsigned int wanted)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	unsigned long actual;
	u8 div;
	int ret;
	/*
	 * Don't care if wanted clock is zero or
	 * ciu clock is unavailable
	 */
	if (!wanted || IS_ERR(host->ciu_clk))
		return;

	/* Guaranteed minimum frequency for cclkin */
	if (wanted < EXYNOS_CCLKIN_MIN)
		wanted = EXYNOS_CCLKIN_MIN;

	if (wanted == priv->cur_speed)
		return;

	div = dw_mci_exynos_get_ciu_div(host);
	ret = clk_set_rate(host->ciu_clk, wanted * div);
	if (ret)
		dev_warn(host->dev,
			"failed to set clk-rate %u error: %d\n",
			wanted * div, ret);
	actual = clk_get_rate(host->ciu_clk);
	host->bus_hz = actual / div;
	priv->cur_speed = wanted;
	host->current_speed = 0;
}

static void dw_mci_exynos_set_ios(struct dw_mci *host, struct mmc_ios *ios)
{
	struct dw_mci_exynos_priv_data *priv = host->priv;
	unsigned int wanted = ios->clock;
	u32 *clk_tbl = priv->ref_clk;
	u32 timing = ios->timing, clksel;
	u32 cclkin;

	cclkin = clk_tbl[timing];

	if(host->bus_hz != cclkin)
		wanted = cclkin;

	switch (timing) {
	case MMC_TIMING_MMC_HS400:
		/* Update tuned sample timing */
		clksel = SDMMC_CLKSEL_UP_SAMPLE(
				priv->hs400_timing, priv->tuned_sample);
		wanted <<= 1;
		break;
	case MMC_TIMING_MMC_DDR52:
		clksel = priv->ddr_timing;
		/* Should be double rate for DDR mode */
		if (ios->bus_width == MMC_BUS_WIDTH_8)
			wanted <<= 1;
		break;
	default:
		clksel = priv->sdr_timing;
	}

	/* Set clock timing for the requested speed mode*/
	dw_mci_exynos_set_clksel_timing(host, clksel);

	/* Configure setting for HS400 */
	dw_mci_exynos_config_hs400(host, timing);

	/* Configure clock rate */
	dw_mci_exynos_adjust_clock(host, wanted);
}

#ifndef MHZ
#define MHZ (1000*1000)
#endif
static int dw_mci_exynos_parse_dt(struct dw_mci *host)
{
	struct dw_mci_exynos_priv_data *priv;
	struct device_node *np = host->dev->of_node;
	u32 timing[4];
	u32 div = 0;
	int idx;
	int ref_clk_size;
	u32 *ref_clk;
	u32 *ciu_clkin_values = NULL;
	int idx_ref;
	int ret = 0;
	int id = 0;

	priv = devm_kzalloc(host->dev, sizeof(*priv), GFP_KERNEL);
	if (!priv) {
		dev_err(host->dev, "mem alloc failed for private data\n");
		return -ENOMEM;
	}

	for (idx = 0; idx < ARRAY_SIZE(exynos_compat); idx++) {
		if (of_device_is_compatible(np, exynos_compat[idx].compatible))
			priv->ctrl_type = exynos_compat[idx].ctrl_type;
	}

	if (of_property_read_u32(np, "num-ref-clks", &ref_clk_size)) {
		dev_err(host->dev, "Getting a number of referece clock failed\n");
		ret = -ENODEV;
		goto err_ref_clk;
	}

	ref_clk = devm_kzalloc(host->dev, ref_clk_size * sizeof(*ref_clk),
					GFP_KERNEL);
	if (!ref_clk) {
		dev_err(host->dev, "Mem alloc failed for reference clock table\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}

	ciu_clkin_values = devm_kzalloc(host->dev,
			ref_clk_size * sizeof(*ciu_clkin_values), GFP_KERNEL);

	if (!ciu_clkin_values) {
		dev_err(host->dev, "Mem alloc failed for temporary clock values\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}
	if (of_property_read_u32_array(np, "ciu_clkin", ciu_clkin_values, ref_clk_size)) {
		dev_err(host->dev, "Getting ciu_clkin values faild\n");
		ret = -ENOMEM;
		goto err_ref_clk;
	}

	for (idx_ref = 0; idx_ref < ref_clk_size; idx_ref++, ref_clk++, ciu_clkin_values++) {
		if (*ciu_clkin_values > MHZ)
			*(ref_clk) = (*ciu_clkin_values);
		else
			*(ref_clk) = (*ciu_clkin_values) * MHZ;
	}

	ref_clk -= ref_clk_size;
	ciu_clkin_values -= ref_clk_size;
	priv->ref_clk = ref_clk;

	of_property_read_u32(np, "samsung,dw-mshc-ciu-div", &div);
	priv->ciu_div = div;

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-sdr-timing", timing, 4);
	if (ret)
		return ret;

	priv->sdr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);

	ret = of_property_read_u32_array(np,
			"samsung,dw-mshc-ddr-timing", timing, 4);
	if (ret)
		return ret;

	priv->ddr_timing = SDMMC_CLKSEL_TIMING(timing[0], timing[1], timing[2], timing[3]);
	host->priv = priv;

err_ref_clk:
	return ret;
}

static inline u8 dw_mci_exynos_get_clksmpl(struct dw_mci *host)
{
	return SDMMC_CLKSEL_CCLK_SAMPLE(mci_readl(host, CLKSEL));
}

static inline void dw_mci_exynos_set_clksmpl(struct dw_mci *host, u8 sample)
{
	u32 clksel;
	clksel = mci_readl(host, CLKSEL);
	clksel = (clksel & ~0x7) | SDMMC_CLKSEL_CCLK_SAMPLE(sample);
	mci_writel(host, CLKSEL, clksel);
}

static inline u8 dw_mci_exynos_move_next_clksmpl(struct dw_mci *host)
{
	u32 clksel;
	u8 sample;

	clksel = mci_readl(host, CLKSEL);
	sample = (clksel + 1) & 0x7;
	clksel = (clksel & ~0x7) | sample;
	mci_writel(host, CLKSEL, clksel);
	return sample;
}

static s8 dw_mci_exynos_get_best_clksmpl(u8 candiates)
{
	const u8 iter = 8;
	u8 __c;
	s8 i, loc = -1;

	for (i = 0; i < iter; i++) {
		__c = ror8(candiates, i);
		if ((__c & 0xc7) == 0xc7) {
			loc = i;
			goto out;
		}
	}

	for (i = 0; i < iter; i++) {
		__c = ror8(candiates, i);
		if ((__c & 0x83) == 0x83) {
			loc = i;
			goto out;
		}
	}

out:
	return loc;
}

static int dw_mci_exynos_execute_tuning(struct dw_mci_slot *slot, u32 opcode,
					struct dw_mci_tuning_data *tuning_data)
{
	struct dw_mci *host = slot->host;
	struct mmc_host *mmc = slot->mmc;
	const u8 *blk_pattern = tuning_data->blk_pattern;
	u8 *blk_test;
	unsigned int blksz = tuning_data->blksz;
	u8 start_smpl, smpl, candiates = 0;
	s8 found = -1;
	int ret = 0;

	blk_test = kmalloc(blksz, GFP_KERNEL);
	if (!blk_test)
		return -ENOMEM;

	start_smpl = dw_mci_exynos_get_clksmpl(host);

	do {
		struct mmc_request mrq = {NULL};
		struct mmc_command cmd = {0};
		struct mmc_command stop = {0};
		struct mmc_data data = {0};
		struct scatterlist sg;

		cmd.opcode = opcode;
		cmd.arg = 0;
		cmd.flags = MMC_RSP_R1 | MMC_CMD_ADTC;

		stop.opcode = MMC_STOP_TRANSMISSION;
		stop.arg = 0;
		stop.flags = MMC_RSP_R1B | MMC_CMD_AC;

		data.blksz = blksz;
		data.blocks = 1;
		data.flags = MMC_DATA_READ;
		data.sg = &sg;
		data.sg_len = 1;

		sg_init_one(&sg, blk_test, blksz);
		mrq.cmd = &cmd;
		mrq.stop = &stop;
		mrq.data = &data;
		host->mrq = &mrq;

		mci_writel(host, TMOUT, ~0);
		smpl = dw_mci_exynos_move_next_clksmpl(host);

		mmc_wait_for_req(mmc, &mrq);

		if (!cmd.error && !data.error) {
			if (!memcmp(blk_pattern, blk_test, blksz))
				candiates |= (1 << smpl);
		} else {
			dev_dbg(host->dev,
				"Tuning error: cmd.error:%d, data.error:%d\n",
				cmd.error, data.error);
		}
	} while (start_smpl != smpl);

	found = dw_mci_exynos_get_best_clksmpl(candiates);
	if (found >= 0)
		dw_mci_exynos_set_clksmpl(host, found);
	else
		ret = -EIO;

	kfree(blk_test);
	return ret;
}

/* Common capabilities of Exynos4/Exynos5 SoC */
static unsigned long exynos_dwmmc_caps[4] = {
	MMC_CAP_1_8V_DDR | MMC_CAP_8_BIT_DATA | MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
	MMC_CAP_CMD23,
};

static const struct dw_mci_drv_data exynos_drv_data = {
	.caps			= exynos_dwmmc_caps,
	.init			= dw_mci_exynos_priv_init,
	.setup_clock		= dw_mci_exynos_setup_clock,
	.prepare_command	= dw_mci_exynos_prepare_command,
	.set_ios		= dw_mci_exynos_set_ios,
	.parse_dt		= dw_mci_exynos_parse_dt,
	.execute_tuning		= dw_mci_exynos_execute_tuning,
};

static const struct of_device_id dw_mci_exynos_match[] = {
	{ .compatible = "samsung,exynos4412-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5250-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5420-dw-mshc",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos5420-dw-mshc-smu",
			.data = &exynos_drv_data, },
	{ .compatible = "samsung,exynos8890-dw-mshc",
			.data = &exynos_drv_data, },
	{},
};
MODULE_DEVICE_TABLE(of, dw_mci_exynos_match);

static int dw_mci_exynos_probe(struct platform_device *pdev)
{
	const struct dw_mci_drv_data *drv_data;
	const struct of_device_id *match;

	match = of_match_node(dw_mci_exynos_match, pdev->dev.of_node);
	drv_data = match->data;
	return dw_mci_pltfm_register(pdev, drv_data);
}

static const struct dev_pm_ops dw_mci_exynos_pmops = {
	SET_SYSTEM_SLEEP_PM_OPS(dw_mci_exynos_suspend, dw_mci_exynos_resume)
	.resume_noirq = dw_mci_exynos_resume_noirq,
	.thaw_noirq = dw_mci_exynos_resume_noirq,
	.restore_noirq = dw_mci_exynos_resume_noirq,
};

static struct platform_driver dw_mci_exynos_pltfm_driver = {
	.probe		= dw_mci_exynos_probe,
	.remove		= __exit_p(dw_mci_pltfm_remove),
	.driver		= {
		.name		= "dwmmc_exynos",
		.of_match_table	= dw_mci_exynos_match,
		.pm		= &dw_mci_exynos_pmops,
	},
};

module_platform_driver(dw_mci_exynos_pltfm_driver);

MODULE_DESCRIPTION("Samsung Specific DW-MSHC Driver Extension");
MODULE_AUTHOR("Thomas Abraham <thomas.ab@samsung.com");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:dwmmc-exynos");
