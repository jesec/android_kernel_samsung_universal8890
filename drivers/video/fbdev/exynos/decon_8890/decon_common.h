/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_DECON_COMMON_H__
#define ___SAMSUNG_DECON_COMMON_H__

#include "./panels/decon_lcd.h"

#define MAX_DECON_WIN		(8)
#define MAX_DECON_S_WIN		(4)
#define MAX_DECON_T_WIN		(4)
#define MAX_VPP_SUBDEV		9
#define SHADOW_UPDATE_TIMEOUT	300 * 1000 /* 300ms */

enum decon_dsi_mode {
	DSI_MODE_SINGLE = 0,
	DSI_MODE_DUAL_DISPLAY,
	DSI_MODE_DUAL_DSI,
	DSI_MODE_NONE,
};

enum decon_trig_mode {
	DECON_HW_TRIG = 0,
	DECON_SW_TRIG
};

enum decon_hold_scheme {
	DECON_VCLK_HOLD = 0x00,
	DECON_VCLK_RUNNING = 0x01,
	DECON_VCLK_RUN_VDEN_DISABLE = 0x3,
};

enum decon_rgb_order {
	DECON_RGB = 0x0,
	DECON_GBR = 0x1,
	DECON_BRG = 0x2,
	DECON_BGR = 0x4,
	DECON_RBG = 0x5,
	DECON_GRB = 0x6,
};

enum decon_set_trig {
	DECON_TRIG_DISABLE = 0,
	DECON_TRIG_ENABLE
};

enum decon_idma_type {
	IDMA_G0 = 0,	/* Dedicated to WIN7 */
	IDMA_G1,
	IDMA_VG0,
	IDMA_VG1,
	IDMA_G2,
	IDMA_G3,
	IDMA_VGR0,
	IDMA_VGR1,
};

enum decon_output_type {
	DECON_OUT_DSI = 0,
	DECON_OUT_EDP,
	DECON_OUT_HDMI,
	DECON_OUT_WB,
	DECON_OUT_TUI
};

struct decon_mode_info {
	enum decon_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
	enum decon_output_type out_type;
	enum decon_dsi_mode dsi_mode;
};

struct decon_param {
	struct decon_mode_info psr;
	struct decon_lcd *lcd_info;
	u32 nr_windows;
	void __iomem *disp_ss_regs;
};

struct decon_window_regs {
	u32 wincon;
	u32 win_start_pos;
	u32 win_end_pos;
	u32 win_pixel_cnt;
	u32 winmap_color;
	u32 whole_w;
	u32 whole_h;
	u32 offset_x;
	u32 offset_y;

	enum decon_idma_type type;
};

enum decon_clk_id {
	CLK_ID_VCLK = 0,
	CLK_ID_ECLK,
	CLK_ID_ACLK,
	CLK_ID_PCLK,
	CLK_ID_DPLL, /* DISP_PLL */
	CLK_ID_RESOLUTION,
	CLK_ID_MIC_RATIO,
	CLK_ID_DSC_RATIO,
	CLK_ID_MAX,
};

struct decon_clocks {
	unsigned long decon[CLK_ID_DPLL + 1];
};

/* CAL APIs list */
u32 decon_reg_init(u32 id, u32 idx, struct decon_param *p);
void decon_reg_init_probe(u32 id, u32 idx, struct decon_param *p);
void decon_reg_start(u32 id, struct decon_mode_info *psr);
int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr);
void decon_reg_set_window_control(u32 id, int win_idx, struct decon_window_regs *regs, u32 winmap_en);
void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en);
void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
			enum decon_set_trig en);
int decon_reg_wait_for_update_timeout(u32 id, unsigned long timeout);
void decon_reg_shadow_protect_win(u32 id, u32 win_idx, u32 protect);
void decon_reg_activate_window(u32 id, u32 index);
u32 decon_reg_get_interrupt_and_clear(u32 id);
int decon_reg_wait_update_done_and_mask(u32 id, struct decon_mode_info *psr,
		unsigned long timeout);
void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr);
void decon_reg_get_clock_ratio(struct decon_clocks *clks, struct decon_param *p);

/* CAL raw functions list */
int decon_reg_reset(u32 id);
void decon_reg_set_default_win_channel(u32 id);
void decon_reg_set_clkgate_mode(u32 id, u32 en);
void decon_reg_blend_alpha_bits(u32 id, u32 alpha_bits);
void decon_reg_set_vidout(u32 id, struct decon_mode_info *psr, u32 idx, u32 en);
void decon_reg_set_crc(u32 id, u32 en);
void decon_reg_set_fixvclk(u32 id, int dsi_idx, enum decon_hold_scheme mode);
void decon_reg_clear_win(u32 id, int win_idx);
void decon_reg_set_rgb_order(u32 id, int dsi_idx, enum decon_rgb_order order);
void decon_reg_set_porch(u32 id, int dsi_idx, struct decon_lcd *info);
void decon_reg_set_linecnt_op_threshold(u32 id, int dsi_idx, u32 th);
void decon_reg_set_clkval(u32 id, u32 clkdiv);
void decon_reg_direct_on_off(u32 id, u32 en);
void decon_reg_per_frame_off(u32 id);
void decon_reg_set_freerun_mode(u32 id, u32 en);
void decon_reg_shadow_update_req(u32 id);
void decon_reg_set_wb_frame(u32 id, u32 width, u32 height, dma_addr_t addr);
void decon_reg_wb_swtrigger(u32 id);
void decon_reg_configure_lcd(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info, u32 dsi_idx);
void decon_reg_configure_trigger(u32 id, enum decon_trig_mode mode);
void decon_reg_set_winmap(u32 id, u32 idx, u32 color, u32 en);
u32 decon_reg_get_linecnt(u32 id, int dsi_idx);
u32 decon_reg_get_vstatus(u32 id, int dsi_idx);
int decon_reg_wait_linecnt_is_zero_timeout(u32 id, int dsi_idx, unsigned long timeout);
u32 decon_reg_get_idle_status(u32 id);
int decon_reg_wait_stop_status_timeout(u32 id, unsigned long timeout);
int decon_reg_is_win_enabled(u32 id, int win_idx);
int decon_reg_is_shadow_updated(u32 id);
void decon_reg_config_mic(u32 id, int dsi_idx, struct decon_lcd *lcd_info);
void decon_reg_clear_int(u32 id);
void decon_reg_config_win_channel(u32 id, u32 win_idx, enum decon_idma_type type);
void decon_reg_set_mdnie_pclk(u32 id, u32 en);
void decon_reg_enable_mdnie(u32 id, u32 en);
void decon_reg_set_mdnie_blank(u32 id, u32 front, u32 sync, u32 back, u32 line);
u32 decon_reg_get_width(u32 id, int dsi_mode);
u32 decon_reg_get_height(u32 id, int dsi_mode);
void decon_reg_win_shadow_update_clear_wait(u32 id, u32 win_idx, unsigned long timeout);
void decon_reg_win_shadow_update_req(u32 id, u32 win_idx);
void decon_reg_all_win_shadow_update_req(u32 id, u32 max_win);
#endif /* ___SAMSUNG_DECON_COMMON_H__ */
