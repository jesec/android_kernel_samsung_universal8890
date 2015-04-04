/* linux/drivers/video/exynos/decon/decon_reg_7420.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Jiun Yu <jiun.yu@samsung.com>
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* use this definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "decon_fw.h"
#define __iomem
#define DISP_SS_BASE_ADDR	0x13970000
#else
#include "decon.h"
#endif

/*
* Connection between IDMAs and WINx
* IDMA_G0 -> Channel7
* IDMA_G1 -> Channel0
* ...
* IDMA_VGR1 -> Channel6
*/
static int IDMA2CHMAP[MAX_DECON_WIN] = {
	0x7, 0x0, 0x3, 0x4, 0x1, 0x2, 0x5, 0x6
};

/******************* CAL raw functions implementation *************************/
/* ---------- SYSTEM REGISTER CONTROL ----------- */
void decon_reg_set_disp_ss_cfg(u32 id, void __iomem *disp_ss_regs,
	u32 dsi_idx, struct decon_mode_info *psr)
{
	u32 val;

	if (!disp_ss_regs) {
		decon_info("disp_ss regs is not mapped\n");
		return;
	}

	val = readl(disp_ss_regs + DISP_CFG);

	switch (id) {
	case 0:
		/* TODO: DUAL_DSI */
		val = ((val & (~DISP_CFG_SYNC_MODE0_MASK)) | DISP_CFG_SYNC_MODE0_TE_F);
		if ((psr->out_type == DECON_OUT_DSI) && (dsi_idx <= 2)) {
			val = (val & (~DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(dsi_idx)));
			val |= DISP_CFG_DSIM_PATH_CFG1_DISP_IF0(dsi_idx) |
				DISP_CFG_DSIM_PATH_CFG0_EN(dsi_idx);
			if (psr->dsi_mode == DSI_MODE_DUAL_DSI) {
				val = (val & (~DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(1)));
				val |= DISP_CFG_DSIM_PATH_CFG1_DISP_IF1(1) |
					DISP_CFG_DSIM_PATH_CFG0_EN(1);
			}
		} else if (psr->out_type == DECON_OUT_EDP) {
			/* TODO: need to disable IF0 -> DSIM path */
			val |= DISP_CFG_DP_PATH_CFG0_EN;
		}
		break;
	case 1:
		val = ((val & (~DISP_CFG_SYNC_MODE1_MASK)) | DISP_CFG_SYNC_MODE1_TE_S);
		if ((psr->out_type == DECON_OUT_DSI) && (dsi_idx <= 2)) {
			val = (val & (~DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(dsi_idx)));
			val |= DISP_CFG_DSIM_PATH_CFG1_DISP_IF2(dsi_idx) |
				DISP_CFG_DSIM_PATH_CFG0_EN(dsi_idx);
		}
		break;
	default:
		break;
	}
	writel(val, disp_ss_regs + DISP_CFG);
	decon_dbg("Display Configuration(DISP_CFG) value is 0x%x\n", val);
}

int decon_reg_reset(u32 id)
{
	int tries;

	decon_write(id, GLOBAL_CONTROL, GLOBAL_SWRESET);
	if (!id) {
		decon_write(id, DSC_CONTROL_0(0), DSC_ENC_SREST);
		decon_write(id, DSC_CONTROL_0(1), DSC_ENC_SREST);
	}
	for (tries = 2000; tries; --tries) {
		if (~decon_read(id, GLOBAL_CONTROL) & GLOBAL_SWRESET)
			break;
		udelay(10);
	}

	if (!tries) {
		decon_err("failed to reset Decon\n");
		return -EBUSY;
	}

	return 0;
}

/* TODO Auto clock gating is enable as default : CLOCK_CONTROL_0 */
void decon_reg_set_clkgate_mode(u32 id, u32 en)
{
	return;
/*
	u32 val = en ? ~0 : 0;

	decon_write_mask(id, DECON_CMU, val, DECON_CMU_ALL_CLKGATE_ENABLE);
*/
}

void decon_reg_set_vidout(u32 id, struct decon_mode_info *psr,
				u32 dsi_idx, u32 en)
{
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_write_mask(id, GLOBAL_CONTROL, GLOBAL_OPERATION_I80_F,
					GLOBAL_OPERATION_MODE_MASK);
	else
		decon_write_mask(id, GLOBAL_CONTROL, GLOBAL_OPERATION_RGBIF_F,
				GLOBAL_OPERATION_MODE_MASK);
	/* TODO Writeback : DECON_EXT not using DSI
	if (id && psr->out_type != DECON_OUT_DSI)
		decon_write_mask(id, VIDOUTCON0, VIDOUTCON0_TV_MODE,
			VIDOUTCON0_TV_MODE);
	*/

	/* TODO Should be determined
	decon_write_mask(id, VIDOUTCON0, en ? ~0 : 0, VIDOUTCON0_LCD_ON_F);
	if (dsi_mode == DSI_MODE_DUAL)
		decon_write_mask(id, VIDOUTCON0, en ? ~0 : 0,
				VIDOUTCON0_LCD_DUAL_ON_F);
	*/
}

void decon_reg_set_crc(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	decon_write_mask(id, CRC_CONTROL, val, CRC_START);
}

void decon_reg_set_fixvclk(u32 id, int disp_idx, enum decon_hold_scheme mode)
{
	u32 val = DISPIF_VCLK_HOLD;

	switch (mode) {
	case DECON_VCLK_HOLD:
		val = DISPIF_VCLK_HOLD;
		break;
	case DECON_VCLK_RUNNING:
		val = DISPIF_VCLK_RUN;
		break;
	case DECON_VCLK_RUN_VDEN_DISABLE:
		val = DISPIF_VCLK_RUN_VDEN_DISABLE;
		break;
	}

	decon_write_mask(id, DISPIF_CONTROL(disp_idx), val, DISPIF_OUT_CLOCK_UNDERRUN_SCHEME_F_MASK);
}

void decon_reg_clear_win(u32 id, int win_idx)
{
	decon_write(id, WIN_CONTROL(win_idx), WIN_CONTROL_RESET_VAL);
	decon_write(id, WIN_START_POSITION(win_idx), 0);
	decon_write(id, WIN_END_POSITION(win_idx), 0);
	decon_write(id, WIN_COLORMAP(win_idx), 0);
	decon_write(id, WIN_START_TIME_CONTROL(win_idx), 0);
	decon_write(id, WIN_PIXEL_COUNT(win_idx), 0x1);
}

void decon_reg_set_rgb_order(u32 id, int disp_idx, enum decon_rgb_order order)
{
	u32 val = DISPIF_RGB_ORDER_O_RGB;

	switch (order) {
	case DECON_RGB:
		val = DISPIF_RGB_ORDER_O_RGB;
		break;
	case DECON_GBR:
		val = DISPIF_RGB_ORDER_O_GBR;
		break;
	case DECON_BRG:
		val = DISPIF_RGB_ORDER_O_BRG;
		break;
	case DECON_BGR:
		val = DISPIF_RGB_ORDER_O_BGR;
		break;
	case DECON_RBG:
		val = DISPIF_RGB_ORDER_O_RBG;
		break;
	case DECON_GRB:
		val = DISPIF_RGB_ORDER_O_GRB;
		break;
	}

	decon_write_mask(id, DISPIF_CONTROL(disp_idx), val, DISPIF_OUT_RGB_ORDER_F_MASK);
}

void __get_mic_compressed_size(struct decon_lcd *info, u32 *width, u32 *height)
{
	switch (info->mic_ratio) {
	case 2:
		*height = info->yres;
		*width = (info->xres / info->mic_ratio);
		break;
	case 3:
		/* TODO */
		break;
	default:
		*height = info->yres;
		*width = info->xres;
		break;
	}
}

void decon_reg_set_porch(u32 id, int disp_idx, struct decon_lcd *info)
{
	u32 val = 0;

	/* CAUTION : Zero is not allowed*/
	val = DISPIF_VBPD0_F(info->vbp) | DISPIF_VFPD0_F(info->vfp);
	decon_write(id, DISPIF_TIMING_CONTROL_0(disp_idx), val);

	val = DISPIF_VSPD0_F(info->vsa);
	decon_write(id, DISPIF_TIMING_CONTROL_1(disp_idx), val);

	val = DISPIF_HBPD0_F(info->hbp) | DISPIF_HFPD0_F(info->hfp);
	decon_write(id, DISPIF_TIMING_CONTROL_2(disp_idx), val);

	val = DISPIF_HSPD0_F(info->hsa);
	decon_write(id, DISPIF_TIMING_CONTROL_3(disp_idx), val);
}

/* TODO : Check It needed to JF */
void decon_reg_set_linecnt_op_threshold(u32 id, int dsi_idx, u32 th)
{
	return;
#if 0
	decon_write(id, LINECNT_OP_THRESHOLD(dsi_idx), th);
#endif
}

/* TODO : Do JF need this? */
void decon_reg_set_clkval(u32 id, u32 clkdiv)
{
	return;
	/* decon_write_mask(id, VCLKCON0, ~0, VCLKCON0_CLKVALUP); */
}

void decon_reg_direct_on_off(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	decon_write_mask(id, GLOBAL_CONTROL, val, GLOBAL_DECON_EN_F | GLOBAL_DECON_EN);
}

void decon_reg_per_frame_off(u32 id)
{
	decon_write_mask(id, GLOBAL_CONTROL, 0, GLOBAL_DECON_EN_F);
}

/* TODO Check : Freerun mode is able to set per DISPIF at JF, currently Fix
 * as 0 */
void decon_reg_set_freerun_mode(u32 id, u32 en)
{
	decon_write_mask(id, DISPIF_CONTROL(0), en ? ~0 : 0, DISPIF_OUT_CLOCK_FREE_RUN_EN);
}

/* TODO Writeback */
void decon_reg_set_wb_frame(u32 id, u32 width, u32 height, dma_addr_t addr)
{
#if 0
	decon_write(id, VIDWB_ADD0, addr);
	decon_write(id, VIDWB_WHOLE_X, info->width);
	decon_write(id, VIDWB_WHOLE_Y, info->height);
	decon_write_mask(id, FRAMEFIFO_REG10, ~0, FRAMEFIFO_WB_MODE_F);
	decon_write_mask(id, VIDOUTCON0, 0, VIDOUTCON0_LCD_ON_F);
	decon_write_mask(id, VIDOUTCON0, ~0,
			VIDOUTCON0_WB_F | VIDOUTCON0_WB_SRC_SEL_F);
#endif
}


/* TODO Writeback */
void decon_reg_wb_swtrigger(u32 id)
{
#if 0
	decon_write_mask(id, TRIGCON, ~0, TRIGCON_SWTRIGCMD_WB);
#endif
}

void decon_reg_configure_lcd(u32 id, enum decon_dsi_mode dsi_mode,
		struct decon_lcd *lcd_info, u32 dsi_idx)
{
	int disp_idx = (!id) ? id : (id + 1);

	decon_reg_set_rgb_order(id, disp_idx, DECON_RGB);
	decon_reg_set_porch(id, disp_idx, lcd_info);
	decon_reg_config_mic(id, lcd_info);

	if (lcd_info->mode == DECON_VIDEO_MODE)
		decon_reg_set_linecnt_op_threshold(id, dsi_idx, lcd_info->yres - 1);

	if (!id && (dsi_mode == DSI_MODE_DUAL_DSI)) {
		decon_reg_set_rgb_order(id, 1, DECON_RGB);
		decon_reg_set_porch(id, 1, lcd_info);

		if (lcd_info->mode == DECON_VIDEO_MODE)
			decon_reg_set_linecnt_op_threshold(id, 1, lcd_info->yres - 1);
	}

	decon_reg_set_clkval(id, 0);

	decon_reg_set_freerun_mode(id, 1);
	decon_reg_direct_on_off(id, 0);
}

void decon_reg_configure_trigger(u32 id, enum decon_trig_mode mode)
{
	u32 mask, val = 0;

	mask =  TRIG_CON_HW_TRIG_EN;

	/* SW_TRIG case, just disable HW_TRIG */
	if (mode == DECON_HW_TRIG) {
		mask |= TRIG_CON_HW_TRIG_MASK;
		val = TRIG_CON_HW_TRIG_EN | TRIG_CON_HW_TRIG_MASK;
	}

	decon_write_mask(id, HW_SW_TRIG_CONTROL, val, mask);
}

void decon_reg_set_winmap(u32 id, u32 idx, u32 color, u32 en)
{
	u32 val = en ? WIN_MAPCOLOR_EN_F : 0;

	/* Enable */
	decon_write_mask(id, WIN_CONTROL(idx), val, WIN_MAPCOLOR_EN_MASK);

	/* Color Set */
	val = WIN_MAPCOLOR_F(color);
	decon_write_mask(id, WIN_COLORMAP(idx), val, WIN_MAPCOLOUR_MASK);
}

/* TODO */
void decon_reg_set_disp_if_colmap(u32 id, u32 idx, u32 color)
{
	/* If Decon and DISPIF is ON when all wins are disabled, DISP generates mapcolor */
}

u32 decon_reg_get_linecnt(u32 id, int disp_idx)
{
	if (disp_idx == 1)
		return DISPIF1_LINECNT_GET(decon_read(id, DISPIF_LINE_COUNT(disp_idx)));
	else
		return DISPIF_LINECNT_GET(decon_read(id, DISPIF_LINE_COUNT(disp_idx)));
}

/* TODO Move to driver */
u32 decon_reg_get_vstatus(u32 id, int dsi_idx)
{
	return 0;
	/* return decon_read(id, VIDCON1(dsi_idx)) & VIDCON1_VSTATUS_MASK; */
}

/* timeout : usec */
/* TODO: VSTATUS related */
int decon_reg_wait_linecnt_is_zero_timeout(u32 id, int dsi_idx, unsigned long timeout)
{
	unsigned long delay_time = 10;
	int disp_idx = (!id) ? id : (id + 1);
	unsigned long cnt = timeout / delay_time;
	u32 linecnt;

	do {
		linecnt = decon_reg_get_linecnt(id, disp_idx);
		if (!linecnt)
			break;
		cnt--;
		udelay(delay_time);
	} while (cnt);

	if (!cnt) {
		decon_err("wait timeout linecount is zero(%u)\n", linecnt);
		return -EBUSY;
	}

	return 0;
}

/* change from.. u32 decon_reg_get_stop_status(u32 id) */
/* TODO Check just Idle or not? Move to driver */
u32 decon_reg_get_idle_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CONTROL);
	if (val & GLOBAL_RUN_STATUS)
		return 1;

	return 0;
}

/* TODO Move to driver */
int decon_reg_wait_stop_status_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_idle_status(id);
		cnt--;
		udelay(delay_time);
	} while (status && cnt);

	if (!cnt) {
		decon_err("wait timeout decon stop status(%u)\n", status);
		return -EBUSY;
	}

	return 0;
}

/* DONE */
int decon_reg_is_win_enabled(u32 id, int win_idx)
{
	if (decon_read(id, WIN_CONTROL(win_idx)) & WIN_EN_F)
		return 1;

	return 0;
}

/* DONE */
int decon_reg_is_shadow_updated(u32 id)
{
	if (decon_read(id, SHADOW_REG_UPDATE_REQ) & SHADOW_REG_UPDATE_REQ_DECON)
		return 0;

	return 1;
}

/* TODO Check the maching for JF */
void decon_reg_config_size(u32 id, int dsi_idx, int dsi_mode, struct decon_lcd *lcd_info)
{
	u32 mic_w;
	u32 split_w, frame_fifo_w, dispif_w, dispif_h;
	u32 xfact, yfact;
	u32 slice = 1;
	u32 val;
	int disp_idx = (!id) ? id : (id + 1);

	/* BG_IMG Size */
	val = BLND_BG_WIDTH_F(lcd_info->xres) | BLND_BG_HEIGHT_F(lcd_info->yres);
	decon_write(id, BLENDER_BG_IMAGE_SIZE_0, val);
	decon_write(id, BLENDER_BG_IMAGE_SIZE_1, lcd_info->xres * lcd_info->yres);

	/* MIC SIZE */
	if (lcd_info->mic_ratio == 2) {
		mic_w = (lcd_info->xres >> 1) * MIC_PIX_IN_BYTES;
		decon_write(id, MIC_SIZE_CONTROL, mic_w);
		yfact = lcd_info->yres;
	} else if (lcd_info->mic_ratio == 3) {
		/* TODO : Slice Mode */
		if ((lcd_info->xres % 12) == 0) {
			xfact = lcd_info->xres / 12;
			yfact = lcd_info->yres >> 1;
			mic_w = 24 * xfact * MIC_PIX_IN_BYTES;
		} else if (((lcd_info->xres - 4) % 12) == 0) {
			xfact = (lcd_info->xres - 4) / 12;
			yfact = lcd_info->yres >> 1;
			mic_w = 24 * xfact * MIC_PIX_IN_BYTES + 24;
		} else if (((lcd_info->xres - 8) % 12) == 0) {
			xfact = (lcd_info->xres - 8) / 12;
			yfact = lcd_info->yres >> 1;
			mic_w = 24 * xfact * MIC_PIX_IN_BYTES + 24;
		}
		decon_write(id, MIC_SIZE_CONTROL, mic_w);
	} else {
		mic_w = lcd_info->xres * MIC_PIX_IN_BYTES;
		yfact = lcd_info->yres;
	}

	/* TODO : Dual Slice Mode */
	if (slice == 1)
		split_w = (mic_w / 3);
	else
		split_w = (mic_w / 3) << 1;

	/* SPLITTER_SIZE */
	if (!id) {
		val = SPLITTER_WIDTH_F(split_w) | SPLITTER_HEIGHT_F(yfact);
		decon_write(id, SPLITTER_SIZE_CONTROL_0, val);
		val = split_w * yfact;
		decon_write(id, SPLITTER_SIZE_CONTROL_1, val);
		if (slice == 1)
			decon_write_mask(id, SPLITTER_CONTROL_0, 0, SPLIT_CON_STARTPTR_MASK);
		else
			decon_write_mask(id, SPLITTER_CONTROL_0, (split_w >> 1), SPLIT_CON_STARTPTR_MASK);
	}

	/* FRAME_FIFO_SIZE */
	if (dsi_mode == DSI_MODE_DUAL_DSI)
		frame_fifo_w = split_w >> 1;
	else
		frame_fifo_w = split_w;

	val = FRAME_FIFO_WIDTH_F(frame_fifo_w) | FRAME_FIFO_HEIGHT_F(yfact);
	decon_write(id, FRAME_FIFO_0_SIZE_CONTROL_0, val);
	decon_write(id, FRAME_FIFO_0_SIZE_CONTROL_1, frame_fifo_w * yfact);
	if (!id) {
		decon_write(id, FRAME_FIFO_1_SIZE_CONTROL_0, val);
		decon_write(id, FRAME_FIFO_1_SIZE_CONTROL_1, frame_fifo_w * yfact);
	}

	/* DISP_IF SIZE */
	if (lcd_info->mic_ratio == 3) {
		dispif_w = frame_fifo_w >> 1;
		dispif_h = yfact << 1;
	} else {
		dispif_w = frame_fifo_w;
		dispif_h = yfact;
	}
	val = DISPIF_HEIGHT_F(dispif_h) | DISPIF_WIDTH_F(dispif_w);
	decon_write(id, DISPIF_SIZE_CONTROL_0(disp_idx), val);

	val = DISPIF_PIXEL_COUNT_F(dispif_w * dispif_h);
	decon_write(id, DISPIF_SIZE_CONTROL_1(disp_idx), val);
	if (!id && (dsi_mode == DSI_MODE_DUAL_DSI)) {
		val = DISPIF_HEIGHT_F(dispif_h) | DISPIF_WIDTH_F(dispif_w);
		decon_write(id, DISPIF_SIZE_CONTROL_0(1), val);

		val = DISPIF_PIXEL_COUNT_F(dispif_w * dispif_h);
		decon_write(id, DISPIF_SIZE_CONTROL_1(1), val);
	}

	return;
}

void decon_reg_config_mic(u32 id, struct decon_lcd *lcd_info)
{
	u32 dummy;
	u32 val, mask;

	if (id)
		return;

	if (lcd_info->mic_ratio == 2) {
		val = MIC_PARA_CR_CTRL_1_BY_2 | MIC_PIXEL_0_1_ORDER;
		mask = MIC_PARA_CR_CTRL_F | MIC_PIXEL_ORDER_F;
		decon_write_mask(id, MIC_CONTROL, val, mask);
	} else if (lcd_info->mic_ratio == 3) {
		val = MIC_PARA_CR_CTRL_1_BY_3 | MIC_PIXEL_0_1_ORDER;
		mask = MIC_PARA_CR_CTRL_F | MIC_PIXEL_ORDER_F;
		decon_write_mask(id, MIC_CONTROL, val, mask);
		/* TODO : Slice Mode */
		if ((lcd_info->xres % 12) == 0)
			dummy = 0;
		else if (((lcd_info->xres - 4) % 12) == 0)
			dummy = 16;
		else if (((lcd_info->xres - 8) % 12) == 0)
			dummy = 8;
		decon_write_mask(id, MIC_CONTROL, MIC_DUMMY_F(dummy), MIC_DUMMY_MASK);
	}
}

/* TODO Select Interrupts! */
void decon_reg_clear_int(u32 id)
{
	u32 mask;

	mask = INTERRUPT_PENDING |
		INT_DISPIF_VSTATUS_INT_PEND |
		INT_RESOURCE_CONFLICT_INT_PEND |
		INT_FRAME_DONE_INT_PEND |
		INT_FIFO_LEVEL_INT_PEND;

	decon_write_mask(id, INTERRUPT_PENDING, mask, mask);
}

void decon_reg_config_win_channel(u32 id, u32 win_idx, enum decon_idma_type type)
{
	u32 data;

	if (type >= MAX_DECON_WIN) {
		decon_err("%s:invalid index of Channel = %d\n", __func__, type);
		type = type & (MAX_DECON_WIN - 1);
	}

	data = WIN_CHMAP_F(IDMA2CHMAP[type]);
	decon_write_mask(id, WIN_CONTROL(win_idx), data, WIN_CHMAP_F_MASK);
}

/* TODO Implement later */
void decon_reg_set_mdnie_pclk(u32 id, u32 en)
{
	return;

}

/* TODO Implement later */
void decon_reg_enable_mdnie(u32 id, u32 en)
{
	return;
}

/* TODO Implement later */
void decon_reg_set_mdnie_blank(u32 id, u32 front, u32 sync, u32 back, u32 line)
{
	return;
}

static void decon_reg_set_data_path(u32 id, int dsi_mode, u32 mic_ratio)
{
	u32 mask, val = 0;

	if (id == 0) {
		mask = DATA_PATH_CTRL_ENHANCE_PATH_MASK | COMP_DISPIF_WB_PATH_MASK;
		if (mic_ratio > 1)
			val |= COMP_MIC_DECON0_SHIFT;

		/* TODO: MDNIE */
		if (dsi_mode == DSI_MODE_SINGLE)
			val |= COMP_DISPIF_NOCOMP_SPLITBYP_U0FF_U0DISP;
		else if (dsi_mode == DSI_MODE_DUAL_DSI)
			val |= COMP_DISPIF_NOCOMP_SPLITBYP_U0U1FF_U0U1DISP;
	} else {
		mask = COMP_DISPIF_WB_PATH_MASK;
		if (mic_ratio > 1)
			val |= COMP_MIC_DECON1_SHIFT;
		val |= COMP_DISPIF_NOCOMP_U2FF_U2DISP;
	}
	decon_write_mask(id, DATA_PATH_CONTROL, val, mask);

}

/***************** CAL APIs implementation *******************/
u32 decon_reg_init(u32 id, u32 dsi_idx, struct decon_param *p)
{
	int win_idx;
	struct decon_lcd *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	int disp_idx = (!id) ? id : (id + 1);

	/* DECON does not need to start, if DECON is already
	 * running(enabled in LCD_ON_UBOOT) */
	if (decon_reg_get_idle_status(id)) {
		decon_reg_init_probe(id, dsi_idx, p);
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
		return -EBUSY;
	}

	/* Configure a DISP_SS */
	decon_reg_set_disp_ss_cfg(id, p->disp_ss_regs, dsi_idx, psr);

	decon_reg_reset(id);
	decon_reg_set_clkgate_mode(id, 0);
	decon_reg_set_vidout(id, psr, dsi_idx, 1);
	decon_reg_set_crc(id, 0);

	decon_reg_set_data_path(id, psr->dsi_mode, lcd_info->mic_ratio);
	decon_reg_config_mic(id, lcd_info);
	decon_reg_config_size(id, dsi_idx, psr->dsi_mode, lcd_info);
	/* TODO: Config the splitter with x_start_pos of right side image */

#if 0 /* TODO Need it? */
	if (id) {
		decon_reg_set_default_win_channel(id);
		/*
		 * Interrupt of DECON-EXT should be set to video mode,
		 * because of malfunction of I80 frame done interrupt.
		 */
		psr->psr_mode = DECON_VIDEO_MODE;
		decon_reg_set_int(id, psr, dsi_mode, 1);
		psr->psr_mode = DECON_MIPI_COMMAND_MODE;
	}
#endif

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_fixvclk(id, disp_idx, DECON_VCLK_RUN_VDEN_DISABLE);
	else
		decon_reg_set_fixvclk(id, disp_idx, DECON_VCLK_HOLD);

	if (!id && (psr->dsi_mode == DSI_MODE_DUAL_DSI)) {
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			decon_reg_set_fixvclk(id, 1, DECON_VCLK_RUN_VDEN_DISABLE);
		else
			decon_reg_set_fixvclk(id, 1, DECON_VCLK_HOLD);
	}

	for (win_idx = 0; win_idx < p->nr_windows; win_idx++)
		decon_reg_clear_win(id, win_idx);

	/* RGB order -> porch values -> LINECNT_OP_THRESHOLD -> clock divider
	 * -> freerun mode --> stop DECON */
	decon_reg_configure_lcd(id, psr->dsi_mode, lcd_info, dsi_idx);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_configure_trigger(id, psr->trig_mode);

	/* asserted interrupt should be cleared before initializing decon hw */
	decon_reg_clear_int(id);

	return 0;
}

void decon_reg_init_probe(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct decon_lcd *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	int disp_idx = (!id) ? id : (id + 1);


	decon_reg_set_clkgate_mode(id, dsi_idx);
	decon_reg_set_vidout(id, psr, dsi_idx, 1);
	decon_reg_set_crc(id, 0);

	decon_reg_set_data_path(id, psr->dsi_mode, lcd_info->mic_ratio);
	decon_reg_config_mic(id, lcd_info);
	decon_reg_config_size(id, dsi_idx, psr->dsi_mode, lcd_info);
	/* Does exynos7420 decon always use DECON_VCLK_HOLD ? */
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_fixvclk(id, disp_idx, DECON_VCLK_RUN_VDEN_DISABLE);
	else
		decon_reg_set_fixvclk(id, disp_idx, DECON_VCLK_HOLD);

	if (!id && (psr->dsi_mode == DSI_MODE_DUAL_DSI)) {
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			decon_reg_set_fixvclk(id, 1, DECON_VCLK_RUN_VDEN_DISABLE);
		else
			decon_reg_set_fixvclk(id, 1, DECON_VCLK_HOLD);
	}

	decon_reg_set_rgb_order(id, dsi_idx, DECON_RGB);
	decon_reg_set_porch(id, dsi_idx, lcd_info);
	if (lcd_info->mode == DECON_VIDEO_MODE)
		decon_reg_set_linecnt_op_threshold(id, dsi_idx, lcd_info->yres - 1);

	if (!id && (psr->dsi_mode == DSI_MODE_DUAL_DSI)) {
		decon_reg_set_rgb_order(id, 1, DECON_RGB);
		decon_reg_set_porch(id, 1, lcd_info);
		if (lcd_info->mode == DECON_VIDEO_MODE)
			decon_reg_set_linecnt_op_threshold(id, 1, lcd_info->yres - 1);
	}

	decon_reg_set_clkval(id, 0);
	decon_reg_set_freerun_mode(id, 0);
	decon_reg_shadow_update_req(id);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_configure_trigger(id, psr->trig_mode);
}

void decon_reg_start(u32 id, struct decon_mode_info *psr)
{
	decon_reg_direct_on_off(id, 1);
	decon_reg_shadow_update_req(id);
	decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}

int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr)
{
	int ret = 0;

	decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);

	if (decon_reg_get_idle_status(id) == 1) {
		/* timeout : 50ms */
		/* TODO: dual DSI scenario */
		ret = decon_reg_wait_linecnt_is_zero_timeout(id, dsi_idx, 50 * 1000);
		if (ret)
			goto err;

		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
			decon_reg_direct_on_off(id, 0);
			decon_reg_shadow_update_req(id);
		} else {
			decon_reg_per_frame_off(id);
			decon_reg_shadow_update_req(id);
			decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
		}

		/* timeout : 20ms */
		ret = decon_reg_wait_stop_status_timeout(id, 20 * 1000);
		if (ret)
			goto err;
	}
err:
	ret = decon_reg_reset(id);

	return ret;
}

/* TODO shoud be modified for JF */
void decon_reg_set_window_control(u32 id, int win_idx,
		struct decon_window_regs *regs, u32 winmap_en)
{
	decon_write(id, WIN_CONTROL(win_idx), regs->wincon);
	decon_write(id, WIN_START_POSITION(win_idx), regs->win_start_pos);
	decon_write(id, WIN_END_POSITION(win_idx), regs->win_end_pos);
	decon_write(id, WIN_PIXEL_COUNT(win_idx), regs->win_pixel_cnt);

	decon_reg_set_winmap(id, win_idx, regs->winmap_color, winmap_en);
	/* WindowN Enabled */
	decon_write_mask(id, WIN_CONTROL(win_idx), ~0, WIN_EN_F);

	decon_reg_config_win_channel(id, win_idx, regs->type);
}

void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en)
{
	u32 val;

	decon_reg_clear_int(id);
	if (en) {
		val = INTR_INT_EN | INTR_FIFO_INT_EN | INTR_RES_CONFLICT_INT_EN;
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
			decon_write_mask(id, INTERRUPT_PENDING, ~0, INT_FRAME_DONE_INT_PEND);
			val |= INTR_FRAME_DONE_INT_EN;
		}
		val |= INTR_DISPIF_VSTATUS_SEL_VSYNC | INTR_DISPIF_VSTATUS_INT_EN |
			INTR_DISPIF_VSTATUS_INT_EN;
		/* TODO: write back */
		decon_write_mask(id, INTERRUPT_ENABLE, val, ~0);
	} else {
		decon_write_mask(id, INTERRUPT_ENABLE, 0, INTR_INT_EN);
	}
}

/* Is it need to do hw trigger unmask and mask asynchronously in case of dual DSI */
void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
			enum decon_set_trig en)
{
	u32 val = (en == DECON_TRIG_ENABLE) ? 0 : ~0;
	u32 mask;

	if (psr->psr_mode == DECON_VIDEO_MODE)
		return;

	if (psr->trig_mode == DECON_SW_TRIG) {
		mask = TRIG_CON_SW_TRIG;
		val = (en == DECON_TRIG_ENABLE) ? ~0 : 0;
	} else {
		mask = TRIG_CON_HW_TRIG_MASK;
		val = (en == DECON_TRIG_ENABLE) ? 0 : ~0;
	}

	decon_write_mask(id, HW_SW_TRIG_CONTROL, val, mask);
}

/* wait until shadow update is finished */
int decon_reg_wait_for_update_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;

	while (decon_read(id, SHADOW_REG_UPDATE_REQ) && --cnt)
		udelay(delay_time);

	if (!cnt) {
		decon_err("timeout of updating decon registers\n");
		return -EBUSY;
	}

	return 0;
}

/* wait until shadow update is finished */
int decon_reg_wait_update_done_and_mask(u32 id, struct decon_mode_info *psr,
		unsigned long timeout)
{
	int ret;
	ret = decon_reg_wait_for_update_timeout(id, timeout);
	decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	return ret;
}

void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr)
{
	decon_reg_shadow_update_req(id);
	decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}

void decon_reg_win_shadow_update_clear_wait(u32 id, u32 win_idx, unsigned long timeout)
{
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, 0, SHADOW_REG_UPDATE_REQ_WIN(win_idx));
	decon_reg_wait_for_update_timeout(id, timeout);
}

/* request window shadow update */
void decon_reg_win_shadow_update_req(u32 id, u32 win_idx)
{
	u32 val = ~0;

	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, val, SHADOW_REG_UPDATE_REQ_WIN(win_idx));
}

void decon_reg_all_win_shadow_update_req(u32 id, u32 max_win)
{
	u32 val = ~(1 << max_win);
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, val, val);
}

void decon_reg_shadow_update_req(u32 id)
{
	u32 val = ~0;

	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, val, SHADOW_REG_UPDATE_REQ_DECON);
}

void decon_reg_activate_window(u32 id, u32 index)
{
	decon_write_mask(id, WIN_CONTROL(index), ~0, WIN_EN_F);
	decon_reg_win_shadow_update_req(id, index);
}

u32 decon_reg_get_width(u32 id, int dsi_mode)
{
	int disp_idx = (!id) ? id : (id + 1);
	u32 val = 0;

	val = decon_read(id, DISPIF_SIZE_CONTROL_0(disp_idx));
	return (val >> DISPIF_WIDTH_START_POS) & DISPIF_W_H_MASK;
}

u32 decon_reg_get_height(u32 id, int dsi_mode)
{
	int disp_idx = (!id) ? id : (id + 1);
	u32 val = 0;

	val = decon_read(id, DISPIF_SIZE_CONTROL_0(disp_idx));
	return (val >> DISPIF_HEIGHT_START_POS) & DISPIF_W_H_MASK;
}

const unsigned long decon_clocks_table[][CLK_ID_MAX] = {
	/* VCLK,  ECLK,  ACLK,  PCLK,  DISP_PLL,  resolution, MIC_ratio, DSC ratio */
	{   125, 137.5,   400,    66,       125, 1440 * 2560,         1,         1},
	{  62.5, 137.5,   400,    66,      62.5, 1440 * 2560,         1,         2},
	{   141, 137.5,   400,    66,       141, 1440 * 2560,         1,         3},
	{    71,   168,   400,    66,        71, 1440 * 2560,         2,         1},
	{  41.7, 137.5,   400,    66,      62.5, 1440 * 2560,         3,         1},
};

void decon_reg_get_clock_ratio(struct decon_clocks *clks, struct decon_param *p)
{
	int i = sizeof(decon_clocks_table) / sizeof(decon_clocks_table[0]) - 1;

	/* set reset value */
	clks->decon[CLK_ID_VCLK] = decon_clocks_table[i][CLK_ID_VCLK];
	clks->decon[CLK_ID_ECLK] = decon_clocks_table[i][CLK_ID_ECLK];
	clks->decon[CLK_ID_ACLK] = decon_clocks_table[i][CLK_ID_ACLK];
	clks->decon[CLK_ID_PCLK] = decon_clocks_table[i][CLK_ID_PCLK];
	clks->decon[CLK_ID_DPLL] = decon_clocks_table[i][CLK_ID_DPLL];

	while (i--) {
		if (decon_clocks_table[i][CLK_ID_RESOLUTION]
				!= p->lcd_info->xres * p->lcd_info->yres) {
			i--;
			continue;
		}

		if (decon_clocks_table[i][CLK_ID_MIC_RATIO]
				!= p->lcd_info->mic_ratio) {
			i--;
			continue;
		}

		clks->decon[CLK_ID_VCLK] = decon_clocks_table[i][CLK_ID_VCLK];
		clks->decon[CLK_ID_ECLK] = decon_clocks_table[i][CLK_ID_ECLK];
		clks->decon[CLK_ID_ACLK] = decon_clocks_table[i][CLK_ID_ACLK];
		clks->decon[CLK_ID_PCLK] = decon_clocks_table[i][CLK_ID_PCLK];
		clks->decon[CLK_ID_DPLL] = decon_clocks_table[i][CLK_ID_DPLL];
		break;
	}

	decon_dbg("%s: VCLK %ld ECLK %ld ACLK %ld PCLK %ld DPLL %ld\n",
		__func__,
		clks->decon[CLK_ID_VCLK],
		clks->decon[CLK_ID_ECLK],
		clks->decon[CLK_ID_ACLK],
		clks->decon[CLK_ID_PCLK],
		clks->decon[CLK_ID_DPLL]);
}

u32 decon_reg_get_interrupt_and_clear(u32 id)
{
	u32 irq_status;
	irq_status = decon_read(id, INTERRUPT_PENDING);

	if (irq_status & INT_DISPIF_VSTATUS_INT_PEND)
		decon_write_mask(id, INTERRUPT_PENDING, ~0, INT_DISPIF_VSTATUS_INT_PEND);

	if (irq_status & INT_FIFO_LEVEL_INT_PEND)
		decon_write_mask(id, INTERRUPT_PENDING, ~0, INT_FIFO_LEVEL_INT_PEND);

	if (irq_status & INT_FRAME_DONE_INT_PEND)
		decon_write_mask(id, INTERRUPT_PENDING, ~0, INT_FRAME_DONE_INT_PEND);

	if (irq_status & INT_RESOURCE_CONFLICT_INT_PEND) {
		decon_write_mask(id, INTERRUPT_PENDING, ~0, INT_RESOURCE_CONFLICT_INT_PEND);
		decon_warn("RESOURCE_OCCUPANCY_INFO_0: 0x%x, RESOURCE_OCCUPANCY_INFO_1: 0x%x\n",
			decon_read(id, RESOURCE_OCCUPANCY_INFO_0),
			decon_read(id, RESOURCE_OCCUPANCY_INFO_1));
		decon_info("RESOURCE_SEL_SEL_0: 0x%x, RESOURCE_SEL_SEL_1: 0x%x, RESOURCE_CONFLICTION_INDUCER: 0x%x\n",
			decon_read(id, RESOURCE_OCCUPANCY_INFO_0),
			decon_read(id, RESOURCE_OCCUPANCY_INFO_1),
			decon_read(id, RESOURCE_CONFLICTION_INDUCER));
	}

	return irq_status;
}

/* TODO implement later
void decon_reg_set_block_mode(u32 id, u32 win_idx, u32 x, u32 y, u32 w, u32 h, u32 en)
{
	u32 val = en ? ~0 : 0;
	u32 blk_offset = 0, blk_size = 0;

	blk_offset = VIDW_BLKOFFSET_Y_F(y) | VIDW_BLKOFFSET_X_F(x);
	blk_size = VIDW_BLKSIZE_W_F(w) | VIDW_BLKSIZE_H_F(h);

	decon_write_mask(id, VIDW_BLKOFFSET(win_idx), blk_offset, VIDW_BLKOFFSET_MASK);
	decon_write_mask(id, VIDW_BLKSIZE(win_idx), blk_size, VIDW_BLKSIZE_MASK);
	decon_write_mask(id, WINCON(win_idx), val, WINCON_BLK_EN_F);
}

void decon_reg_set_tui_va(u32 id, u32 va)
{
	decon_write(id, VIDW_ADD2(6), va);
}
*/
