/*
 * drivers/video/exynos/decon/regs-decon.h
 *
 * Register definition file for Samsung DECON driver
 *
 * Copyright (c) 2014 Samsung Electronics
 * Sewoon Park <seuni.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef _REGS_DECON_H
#define _REGS_DECON_H

/* DISP INTERFACE OFFSET : IF0,1 = 0x0, IF2 = 0x5000, IF3 = 0x6000 */
#define IF_OFFSET(_x)	((((_x) < 2) ? 0 : 0x1000) * ((_x) + 3))
/* For TIMING VALUE : IF0,2,3 = 0x0, IF1 = 0x30 */
#define IF1_OFFSET(_x)	(((_x) == 1) ? 0x30 : 0x0)

#define DISP_CFG				0x0
#define DISP_CFG_DP_PATH_CFG0_EN		(0x1 << 20)
#define DISP_CFG_DSIM_PATH_CFG0_EN(_idx)	(0x1 << (8 + 4 * (_idx)))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF0(_idx)	(0x0 << (9 + 4 * (_idx)))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF1(_idx)	(0x1 << (9 + 4 * (_idx)))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF2(_idx)	(0x2 << (9 + 4 * (_idx)))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(_idx)	(0x3 << (9 + 4 * (_idx)))
#define DISP_CFG_SYNC_MODE1_MASK		(0x3 << 2)
#define DISP_CFG_SYNC_MODE1_TE_T                 (0x2 << 2)
#define DISP_CFG_SYNC_MODE1_TE_S                 (0x1 << 2)
#define DISP_CFG_SYNC_MODE1_TE_F                 (0x0 << 2)
#define DISP_CFG_SYNC_MODE0_MASK		(0x3 << 0)
#define DISP_CFG_SYNC_MODE0_TE_T                 (0x2 << 0)
#define DISP_CFG_SYNC_MODE0_TE_S                 (0x1 << 0)
#define DISP_CFG_SYNC_MODE0_TE_F                 (0x0 << 0)

/* VIDCON0 at previous SoC */
#define GLOBAL_CONTROL			0x0
#define GLOBAL_SWRESET			(0x1 << 28)
#define GLOBAL_OPERATION_MODE_MASK	(0x1 << 8)
#define GLOBAL_OPERATION_RGBIF_F	(0x0 << 8)
#define GLOBAL_OPERATION_I80_F		(0x1 << 8)
#define GLOBAL_IDLE_STATUS_MASK		(0x1 << 5)
#define GLOBAL_RUN_STATUS		(0x1 << 4)
#define GLOBAL_DECON_EN			(0x1 << 1)
#define GLOBAL_DECON_EN_F		(0x1 << 0)

#define	RESOURCE_OCCUPANCY_INFO_0	0x0010
#define RESOURCE_OCCUPANCY_INFO_1	0x0014
#define RESOURCE_SEL_SEL_0		0x0018
#define RESOURCE_SEL_SEL_1		0x001C
#define RESOURCE_CONFLICTION_INDUCER	0x0020
#define SRAM_SHARE_ENABLE		0x0030
#define INTERRUPT_ENABLE		0x0040
#define INTR_INT_EN			(0x1 << 0)
#define INTR_FIFO_INT_EN		(0x1 << 8)
#define INTR_FRAME_DONE_INT_EN		(0x1 << 12)
#define INTR_RES_CONFLICT_INT_EN	(0x1 << 18)
#define INTR_DISPIF_VSTATUS_SEL_MASK	(0x3 << 20)
#define INTR_DISPIF_VSTATUS_SEL_VBP	(0x0 << 20)
#define INTR_DISPIF_VSTATUS_SEL_VSYNC	(0x1 << 20)
#define INTR_DISPIF_VSTATUS_SEL_ACT	(0x2 << 20)
#define INTR_DISPIF_VSTATUS_SEL_VFP	(0x3 << 20)
#define INTR_DISPIF_SEL_F		(0x1 << 22)
#define INTR_DISPIF_VSTATUS_INT_EN	(0x1 << 24)


#define UNDER_RUN_CYCLE_THRESHOLD	0x0044
#define INTERRUPT_PENDING		0x0048
#define INT_DISPIF_VSTATUS_INT_PEND	(0x1 << 24) /*[24]*/
#define INT_RESOURCE_CONFLICT_INT_PEND	(0x1 << 18) /*[18]*/
#define INT_FRAME_DONE_INT_PEND		(0x1 << 12) /*[12]*/
#define INT_FIFO_LEVEL_INT_PEND		(0x1 << 8)  /*[8]*/

#define SHADOW_REG_UPDATE_REQ		0x0060
#define SHADOW_REG_UPDATE_REQ_DECON	(0x1 << 31)
#define SHADOW_REG_UPDATE_REQ_MDNIE	(0x1 << 24)
#define SHADOW_REG_UPDATE_REQ_WIN(_idx) (0x1 << (_idx))
/* SHADOW_REG Mapping
SHADOW_REG_UPDATE_REQ	[31]
SHADOW_REG_UPDATE_REQ_mDNIe	[24]
WIN7_SHADOW_REG_UPDATE_REQ	[7]
WIN6_SHADOW_REG_UPDATE_REQ	[6]
WIN5_SHADOW_REG_UPDATE_REQ	[5]
WIN4_SHADOW_REG_UPDATE_REQ	[4]
WIN3_SHADOW_REG_UPDATE_REQ	[3]
WIN2_SHADOW_REG_UPDATE_REQ	[2]
WIN1_SHADOW_REG_UPDATE_REQ	[1]
WIN0_SHADOW_REG_UPDATE_REQ	[0]
*/

#define HW_SW_TRIG_CONTROL		0x0070
#define TRIG_CON_TRIG_AUTO_MASK_EN	(1 << 12) /*[12]*/
#define TRIG_CON_HW_TRIG_ACTIVE_VALUE	(1 << 9)  /*[9]*/
#define TRIG_CON_SW_TRIG		(1 << 8)  /*[8]*/
#define TRIG_CON_HW_TRIG_EDGE_POLARITY	(1 << 7)  /*[7]*/
#define TRIG_CON_HW_TRIG_MASK		(1 << 5)  /*[5]*/
#define TRIG_CON_HW_TRIG_EN		(1 << 4)  /*[4]*/

/* DISPIF0_DISPIF1_CONTROL = 0x0080,
   DISPIF2_CONTROL = 0x5080, IF3 = 0x6080 */
#define DISPIF_CONTROL(_if_idx)			(0x0080 + IF_OFFSET(_if_idx))
#define DISPIF_2PIX_EN_F	(24)
#define DISPIF_OUT_CLOCK_UNDERRUN_SCHEME_F_MASK	(0x3 << 12) /*[13:12]*/
#define DISPIF_VCLK_HOLD			(0x0 << 12)
#define DISPIF_VCLK_RUN				(0x1 << 12)
#define DISPIF_VCLK_RUN_VDEN_DISABLE		(0x3 << 12)
#define DISPIF_OUT_RGB_ORDER_F_MASK		(0x7 << 8)
#define DISPIF_RGB_ORDER_O_RGB			(0x0 << 4)
#define DISPIF_RGB_ORDER_O_GBR			(0x1 << 4)
#define DISPIF_RGB_ORDER_O_BRG			(0x2 << 4)
#define DISPIF_RGB_ORDER_O_BGR			(0x4 << 4)
#define DISPIF_RGB_ORDER_O_RBG			(0x5 << 4)
#define DISPIF_RGB_ORDER_O_GRB			(0x6 << 4)
#define DISPIF_OUT_CLOCK_FREE_RUN_EN		(1 << 4) /*[4]*/
#define DISPIF_VSYNC_ACTIVE_VALUE	(2)
#define DISPIF_HSYNC_ACTIVE_VALUE	(1)
#define DISPIF_VDEN_ACTIVE_VALUE	(0)

/* IF0,1 = 0x008C, IF2 = 0x508C, IF3 = 0x608C */
#define DISPIF_LINE_COUNT(_if_idx)	(0x008C + IF_OFFSET(_if_idx))
#define DISPIF1_LINECNT_GET(_v)		(((_v) >> 16) & 0xffff)
#define DISPIF_LINECNT_GET(_v)		(((_v) >> 0) & 0xffff)

/* IF0 = 0x0090, IF1 = 0x00C0, IF2 = 0x5090, IF3 = 0x6090 */
#define DISPIF_TIMING_CONTROL_0(_if_idx) \
	(0x0090 + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_VBPD0_F(_v)		((_v) << 16) /*[31:16]*/
#define DISPIF_VFPD0_F(_v)		((_v) << 0)  /*[15:0]*/

#define DISPIF_TIMING_CONTROL_1(_if_idx) \
	(0x0094 + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_VSPD0_F(_v)		((_v) << 0)  /*[15:0]*/

#define DISPIF_TIMING_CONTROL_2(_if_idx) \
	(0x0098 + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_HBPD0_F(_v)		((_v) << 16) /*[31:16]*/
#define DISPIF_HFPD0_F(_v)		((_v) << 0)  /*[15:0]*/

#define DISPIF_TIMING_CONTROL_3(_if_idx) \
	(0x009C + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_HSPD0_F(_v)		((_v) << 0)  /*[15:0]*/

#define DISPIF_SIZE_CONTROL_0(_if_idx) \
	(0x00A0 + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_HEIGHT_F(_v)		((_v & 0x3FFF) << 16) /*[29:16]*/
#define DISPIF_WIDTH_F(_v)		((_v & 0x3FFF) << 0)  /*[13:0]*/
#define DISPIF_W_H_MASK			(0x3FFF)
#define DISPIF_WIDTH_START_POS		(0)
#define DISPIF_HEIGHT_START_POS		(16)

#define DISPIF_SIZE_CONTROL_1(_if_idx) \
	(0x00A4 + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_PIXEL_COUNT_F(_v)	((_v) << 0) /*[31:0]*/

/* Auto clock gating is enable as default */
#define CLOCK_CONTROL_0			0x00F0
#define AUTO_CLOCK_GATE_ENABLE_OF_ROOT_VCLK_3	(31)
#define AUTO_CLOCK_GATE_ENABLE_OF_ROOT_VCLK_2	(30)
#define AUTO_CLOCK_GATE_ENABLE_OF_ROOT_VCLK	(29)
#define AUTO_CLOCK_GATE_ENABLE_OF_ROOT_ECLK	(28)
#define AUTO_CLOCK_GATE_ENABLE_OF_ECLK	(25)
#define AUTO_CLOCK_GATE_ENABLE_OF_PCLK	(24)
#define AUTO_CLOCK_GATE_ENABLE_OF_DSCC	(20)
#define QACTIVE_VALUE	(16)
#define W7_AUTO_CG_EN	(7)
#define W6_AUTO_CG_EN	(6)
#define W5_AUTO_CG_EN	(5)
#define W4_AUTO_CG_EN	(4)
#define W3_AUTO_CG_EN	(3)
#define W2_AUTO_CG_EN	(2)
#define W1_AUTO_CG_EN	(1)
#define W0_AUTO_CG_EN	(0)

#define VCLK_DIVIDER_CONTROL		0x00F4
#define ECLK_DIVIDER_CONTROL		0x00F8
#define BLENDER_BG_IMAGE_SIZE_0		0x0110
#define BLND_BG_WIDTH_F(_x)	((_x & 0x3FFF) << 0)
#define BLND_BG_HEIGHT_F(_x)	((_x & 0x3FFF) << 16)

#define BLENDER_BG_IMAGE_SIZE_1		0x0114
#define BLENDER_BG_IMAGE_COLOR		0x0118
#define LRMERGER_MODE_CONTROL		0x011C

#define WIN_CONTROL(_win)		(0x0130 + ((_win) * 32))
#define WIN_CONTROL_RESET_VAL		(0x00000300)
#define WIN_ALPHA1_F(_x)	(((_x) & 0xff) << 24) /*[31:24]*/
#define WIN_ALPHA0_F(_x)	(((_x) & 0xff) << 16) /*[23:16]*/
#define WIN_CHMAP_F(_x)		(((_x) & 0x7) << 12)  /*[14:12]*/
#define WIN_CHMAP_F_MASK	(0x7 << 12)  /*[14:12]*/
#define WIN_BLEND_FUNC_F(_x)	(((_x) & 0xF) << 8)   /*[11:8]*/
#define WIN_ALPHA_MUL_F			(0x1 << 6)   /*[6]*/
#define WIN_ALPHA_SEL_F			(0x3 << 4)   /*[5:4]*/
#define WIN_MAPCOLOR_EN_F		(0x1 << 1)   /*[1]*/
#define WIN_MAPCOLOR_EN_MASK		(0x1 << 1)   /*[1]*/
#define WIN_EN_F			(0x1 << 0)   /*[0]*/

#define WIN_BLEND_FUNC_CLEAR		(0 << 8)
#define WIN_BLEND_FUNC_COPY		(1 << 8)
#define WIN_BLEND_FUNC_DST		(2 << 8)
#define WIN_BLEND_FUNC_SRC_OVER		(3 << 8)
#define WIN_BLEND_FUNC_DST_OVER		(4 << 8)
#define WIN_BLEND_FUNC_SRC_IN		(5 << 8)
#define WIN_BLEND_FUNC_DST_IN		(6 << 8)
#define WIN_BLEND_FUNC_SRC_OUT		(7 << 8)
#define WIN_BLEND_FUNC_DST_OUT		(8 << 8)
#define WIN_BLEND_FUNC_SRC_TOP		(9 << 8)
#define WIN_BLEND_FUNC_DST_TOP		(10 << 8)
#define WIN_BLEND_FUNC_XOR		(11 << 8)
#define WIN_BLEND_FUNC_PLUS		(12 << 8)
#define WIN_BLEND_FUNC_LEGACY		(13 << 8)

/*
 * WIN_BLEND_FUNC_CLEAR => Cd = Cs * 0 +  Cd * 0
 * WIN_BLEND_FUNC_COPY =>  Cd = Cs;
 * WIN_BLEND_FUNC_DST =>  Cd =  Cd;
 * WIN_BLEND_FUNC_SRC_OVER => Cd = Cs + Cd * (1 - As);
 * WIN_BLEND_FUNC_DST_OVER => Cd = Cd + Cs * (1 - Ad);
 * WIN_BLEND_FUNC_SRC_IN => Cd =  Cd * 0 + Cs * Ad;
 * WIN_BLEND_FUNC_DST_IN => Cd = Cd * As + Cs * 0;
 * WIN_BLEND_FUNC_SRC_OUT => Cd =  Cd * 0 + Cs * (1 - Ad);
 * WIN_BLEND_FUNC_DST_OUT => Cd = Cd * (1 - As) + Cs * 0;
 * WIN_BLEND_FUNC_SRC_TOP => Cd = Cd * (1 - As) + Cs * Ad;
 * WIN_BLEND_FUNC_DST_TOP => Cd = Cd * As + Cs * (1 - Ad);
 * WIN_BLEND_FUNC_XOR => Cd = Cd * (1 - As) + Cs * (1 - Ad);
 * WIN_BLEND_FUNC_PLUS => Cd = Cd + Cs;
 * WIN_BLEND_FUNC_LEGACY => Cd = Cd * (1 - As) + Cs * As;
*/

#define WIN_START_POSITION(_win)	((0x0130 + ((_win) * 32) + 4))
#define WIN_STRPTR_Y_F_MASK		(0x1fff << 16) /*[28:16]*/
#define WIN_STRPTR_X_F_MASK		(0x1fff << 0)  /*12:0]*/
#define WIN_STRPTR_Y_F_POS		(16)
#define WIN_STRPTR_X_F_POS              (0)

#define WIN_END_POSITION(_win)		((0x0130 + ((_win) * 32) + 8))
#define WIN_ENDPTR_Y_F_MASK		(0x1fff << 16) /*[28:16]*/
#define WIN_ENDPTR_X_F_MASK		(0x1fff << 0)  /*[12:0]*/
#define WIN_ENDPTR_Y_F_POS		(16)
#define WIN_ENDPTR_X_F_POS              (0)

#define WIN_COLORMAP(_win)		((0x0130 + ((_win) * 32) + 12))
#define WIN_MAPCOLOR_F(_v)		((_v) << 0)
#define WIN_MAPCOLOUR_MASK		(0xffffff << 0)
#define WIN_MAPCOLOR_R_F		(0xff << 16) /*[23:16]*/
#define WIN_MAPCOLOR_G_F		(0xff << 8)  /*[15:8]*/
#define WIN_MAPCOLOR_B_F		(0xff << 0)  /*[7:0]*/

#define WIN_START_TIME_CONTROL(_win)	((0x0130 + ((_win) * 32) + 16))
#define WIN_PIXEL_COUNT(_win)		((0x0130 + ((_win) * 32) + 20))

#define DATA_PATH_CONTROL		0x0230
#define DATA_PATH_CTRL_ENHANCE_PATH_MASK	(0x7 << 8)
#define DATA_PATH_CTRL_ENHANCE_OFF		(0x0 << 8)
#define DATA_PATH_CTRL_DITHER_ON		(0x1 << 8)
#define DATA_PATH_CTRL_MDNIE_ON_DITHER_OFF	(0x4 << 8)
#define DATA_PATH_CTRL_MDNIE_ON_DITHER_ON	(0x5 << 8)
#define COMP_DISPIF_WB_PATH_MASK		(0xFF << 0)
#define COMP_DISPIF_NOCOMP_SPLITBYP_U0FF_U0DISP (0x1 << 0)
#define COMP_DISPIF_NOCOMP_SPLITBYP_U1FF_U1DISP (0x2 << 0)
#define COMP_DISPIF_NOCOMP_SPLITBYP_U0U1FF_U0U1DISP (0x3 << 0)
#define COMP_DISPIF_MIC_SPLITBYP_U0FF_U0DISP (0x9 << 0)
#define COMP_DISPIF_MIC_SPLITBYP_U1FF_U1DISP (0xA << 0)
#define COMP_DISPIF_MIC_SPLITBYP_U0U1FF_U0U1DISP (0xB << 0)
#define COMP_MIC_DECON0_SHIFT			(0x1 << 3)
#define COMP_MIC_DECON1_SHIFT			(0x1 << 5)
#define COMP_DISPIF_NOCOMP_U2FF_U2DISP		(0x1 << 0)

#define SPLITTER_CONTROL_0		0x0240
#define SPLIT_CON_STARTPTR_MASK		0x3fff
#define SPLITTER_SIZE_CONTROL_0		0x0244
#define SPLITTER_HEIGHT_F(_v)		((_v & 0x3fff) << 16) /*[29:16]*/
#define SPLITTER_WIDTH_F(_v)		((_v & 0x3fff) << 0)  /*[13:0]*/
#define SPLITTER_SIZE_CONTROL_1		0x0248
#define FRAME_FIFO_CONTROL		0x024C
#define FRAME_FIFO_0_SIZE_CONTROL_0	0x0250
#define FRAME_FIFO_0_SIZE_CONTROL_1	0x0254
#define FRAME_FIFO_1_SIZE_CONTROL_0	0x0258
#define FRAME_FIFO_1_SIZE_CONTROL_1	0x025C
#define FRAME_FIFO_HEIGHT_F(_v)		((_v & 0x3fff) << 16) /*[29:16]*/
#define FRAME_FIFO_WIDTH_F(_v)		((_v & 0x3fff) << 0)  /*[13:0]*/
#define SRAM_SHARE_COMP_INFO		0x0270
#define SRAM_SHARE_ENH_INFO		0x0274
#define CRC_DATA_0			0x0280
#define CRC_DATA_2			0x0284

#define CRC_CONTROL			0x0288
#define CRC_SRC_SEL_V			(0x1 << 28) /*[28]*/
#define CRC_SRC_SEL_E			(0x7 << 24) /*[26:24]*/
#define CRC_MUX_SEL			(0x1f << 16) /*[20:16]*/
#define CRC_PIXEL_SEL			(0x1 << 12) /*[12]*/
#define CRC_START			(0x1 << 0)  /*[0]*/

#define FRAME_ID			0x02A0
#define DEBUG_CLOCK_OUT_SEL		0x02AC
#define DITHER_CONTROL			0x0300
#define	MIC_CONTROL			0x1004
#define MIC_DUMMY_F(_v)			((_v) << 20) /*[28:20]*/
#define MIC_DUMMY_MASK			(0x1FF << 20)
#define MIC_AUTO_CLOCK_GATE_EN_F	(16)
#define MIC_MODULE_BYPASS_F	(12)
#define MIC_SLICE_NUM_F		(8)
#define MIC_PIXEL_0_1_ORDER	(0 << 4)
#define MIC_PIXEL_1_0_ORDER	(1 << 4)
#define MIC_PIXEL_ORDER_F	(1 << 4)
#define MIC_4x1_VC_OLD		(1 << 2)
#define MIC_4x1_VC_NEW		(0 << 2)
#define MIC_4x1_VC_F		(1 << 2)
#define MIC_PARA_CR_CTRL_1_BY_3	(1 << 1)
#define MIC_PARA_CR_CTRL_1_BY_2	(0 << 1)
#define MIC_PARA_CR_CTRL_F	(1 << 1)
#define MIC_PARA_P_UPD_DIS	(0 << 0)
#define MIC_PARA_P_UPD_EN	(1 << 0)
#define MIC_PARA_P_UPD_EN_F	(1 << 0)
#define MIC_PIX_IN_BYTES	(3)

#define	MIC_ENC_PARAM0			0x1008
#define	MIC_ENC_PARAM1			0x100C
#define	MIC_ENC_PARAM2			0x1010
#define	MIC_ENC_PARAM3			0x1014
#define	MIC_BG_COLOR			0x1018
#define	MIC_SIZE_CONTROL		0x101C
#define MIC_WIDTH_C_F			(0)

#define DSC_CONTROL_0(_id)	(0x2000 + 0x1000 * (_id))
#define DSC_ENC_SREST		(0x1 << 28)
#define DSC_CONTROL_1(_id)	(0x2004 + 0x1000 * (_id))
#define DSC_CONTROL_2(_id)	(0x2008 + 0x1000 * (_id))
#define DSC_IN_PIXEL_CNT(_id)	(0x200C + 0x1000 * (_id))
#define DSC_COMP_PIXEL_CNT(_id)	(0x2010 + 0x1000 * (_id))

/*TODO: set proper shadow offset */
#define SHADOW_OFFSET	0x0
#endif /* _REGS_DECON_H */
