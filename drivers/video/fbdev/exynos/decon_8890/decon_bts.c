 /*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vpp/vpp.h"
#include "decon.h"
#include "decon_helper.h"

#include <soc/samsung/bts.h>
#include <media/v4l2-subdev.h>

#define MULTI_FACTOR 		(1 << 10)
#define VPP_MAX 		9
#define HALF_MIC 		2
#define PIX_PER_CLK 		2
#define ROTATION 		4
#define NOT_ROT 		1
#if !defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
#define RGB_FACTOR 		4
#define YUV_FACTOR 		4
#endif
 /* If use float factor, DATA format factor should be multiflied by 2
 * And then, should be divided using FORMAT_FACTOR at the end of bw_eq
 */
#define FORMAT_FACTOR 		2
#define DISP_FACTOR 		1

#define ENUM_OFFSET 		2

#if defined(CONFIG_PM_DEVFREQ) && !defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
void bts_int_mif_bw(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	struct decon_device *decon = get_decon_drvdata(0);
	u32 vclk = (clk_get_rate(decon->res.vclk_leaf) * PIX_PER_CLK / MHZ);
	/* TODO, parse mic factor automatically at dt */
	u32 mic_factor = HALF_MIC; /* 1/2 MIC */
	u64 s_ratio_h, s_ratio_v = 0;
	/* bpl is multiplied 2, it should be divied at end of eq. */
	u8 bpl = 0;
	u8 rot_factor = is_rotation(config) ? ROTATION : NOT_ROT;
	bool is_rotated = is_rotation(config) ? true : false;

	if (is_rgb(config))
		bpl = RGB_FACTOR;
	else if (is_yuv(config))
		bpl = YUV_FACTOR;
	else
		bpl = RGB_FACTOR;

	if (is_rotated) {
		s_ratio_h = MULTI_FACTOR * src->h / dst->w;
		s_ratio_v = MULTI_FACTOR * src->w / dst->h;
	} else {
		s_ratio_h = MULTI_FACTOR * src->w / dst->w;
		s_ratio_v = MULTI_FACTOR * src->h / dst->h;
	}

	/* BW = (VCLK * MIC_factor * Data format * ScaleRatio_H * ScaleRatio_V * RotationFactor) */
	vpp->cur_bw = vclk * mic_factor * bpl * s_ratio_h * s_ratio_v
		* rot_factor * KHZ / (MULTI_FACTOR * MULTI_FACTOR);
}

void bts_disp_bw(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	struct decon_device *decon = get_decon_drvdata(0);
	u32 vclk = (clk_get_rate(decon->res.vclk_leaf) * PIX_PER_CLK / MHZ);
	/* TODO, parse mic factor automatically at dt */
	u32 mic_factor = HALF_MIC; /* 1/2 MIC */
	/* TODO, parse lcd width automatically at dt */
	u32 lcd_width = 1440;
	u64 s_ratio_h, s_ratio_v = 0;

	u32 src_w = is_rotation(config) ? src->h : src->w;
	u32 src_h = is_rotation(config) ? src->w : src->h;

	/* ACLK_DISP_400 [MHz] = (VCLK * MIC_factor * ScaleRatio_H * ScaleRatio_V * disp_factor) / 2 * (DST_width / LCD_width) */
	s_ratio_h = (src_w <= dst->w) ? MULTI_FACTOR : MULTI_FACTOR * src_w / dst->w;
	s_ratio_v = (src_h <= dst->h) ? MULTI_FACTOR : MULTI_FACTOR * src_h / dst->h;

	vpp->disp_cur_bw = vclk * mic_factor * s_ratio_h * s_ratio_v
		* DISP_FACTOR * KHZ * (MULTI_FACTOR * dst->w / lcd_width)
		/ (MULTI_FACTOR * MULTI_FACTOR * MULTI_FACTOR) / 2;
}

void bts_mif_lock(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;

	if (is_rotation(config)) {
		pm_qos_update_request(&vpp->vpp_mif_qos, 1144000);
	} else {
		pm_qos_update_request(&vpp->vpp_mif_qos, 0);
	}
}

void bts_send_bw(struct vpp_dev *vpp, bool enable)
{
	struct decon_win_config *config = vpp->config;
	u8 vpp_type;
	enum vpp_bw_type bw_type;

	if (is_rotation(config)) {
		if (config->src.w * config->src.h >= FULLHD_SRC)
			bw_type = BW_FULLHD_ROT;
		else
			bw_type = BW_ROT;
	} else {
		bw_type = BW_DEFAULT;
	}

	vpp_type = vpp->id + ENUM_OFFSET;

	if (enable) {
		exynos_update_media_scenario(vpp_type, vpp->cur_bw, bw_type);
	} else {
		exynos_update_media_scenario(vpp_type, 0, BW_DEFAULT);
		vpp->prev_bw = vpp->cur_bw = 0;
	}
}

void bts_add_request(struct decon_device *decon)
{
	pm_qos_add_request(&decon->mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&decon->disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 0);
	pm_qos_add_request(&decon->int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
}

void bts_send_init(struct decon_device *decon)
{
	pm_qos_update_request(&decon->mif_qos, 546000);
}

void bts_send_release(struct decon_device *decon)
{
	pm_qos_update_request(&decon->mif_qos, 0);
	pm_qos_update_request(&decon->disp_qos, 0);
	pm_qos_update_request(&decon->int_qos, 0);
	decon->disp_cur = decon->disp_prev = 0;
}

void bts_remove_request(struct decon_device *decon)
{
	pm_qos_remove_request(&decon->mif_qos);
	pm_qos_remove_request(&decon->disp_qos);
	pm_qos_remove_request(&decon->int_qos);
}

void bts_calc_bw(struct vpp_dev *vpp)
{
	bts_disp_bw(vpp);
	bts_int_mif_bw(vpp);
}

void bts_send_zero_bw(struct vpp_dev *vpp)
{
	bts_send_bw(vpp, false);
}

void bts_send_calc_bw(struct vpp_dev *vpp)
{
	bts_send_bw(vpp, true);
}
#else
void bts_add_request(struct decon_device *decon){ return; }
void bts_send_init(struct decon_device *decon){ return; }
void bts_send_release(struct decon_device *decon){ return; }
void bts_remove_request(struct decon_device *decon){ return; }

void bts_calc_bw(struct vpp_dev *vpp){ return; }
void bts_send_calc_bw(struct vpp_dev *vpp){ return; }
void bts_send_zero_bw(struct vpp_dev *vpp){ return; }
void bts_mif_lock(struct vpp_dev *vpp){ return; }
#endif

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
void bts2_calc_bw(struct decon_device *decon, struct decon_reg_data *regs)
{
	int i;
	struct decon_win *win;
	struct vpp_dev *vpp;
	struct v4l2_subdev *sd;
	struct decon_win_config *config;

	decon->total_bw = 0;
	decon->max_peak_bw = 0;
	for (i = 0; i < decon->pdata->max_win; i++) {
		win = decon->windows[i];
		if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
			sd = decon->mdev->vpp_sd[win->vpp_id];
			vpp = v4l2_get_subdevdata(sd);
			config = &regs->vpp_config[i];

			vpp->bts_info.src_w = config->src.w;
			vpp->bts_info.src_h = config->src.h;
			vpp->bts_info.dst_w = config->dst.w;
			vpp->bts_info.dst_h = config->dst.h;
			vpp->bts_info.bpp = decon_get_bpp(config->format);
			vpp->bts_info.is_rotation = is_rotation(config);
			vpp->bts_info.mic = decon->mic_factor;
			vpp->bts_info.pix_per_clk = DECON_PIX_PER_CLK;
			vpp->bts_info.vclk = decon->vclk_factor;

			decon_dbg("%s(%d): src_w(%d), src_h(%d), dst_w(%d), dst_h(%d)\n",
					__func__, win->vpp_id,
					vpp->bts_info.src_w, vpp->bts_info.src_h,
					vpp->bts_info.dst_w, vpp->bts_info.dst_h);
			decon_dbg("bpp(%d), roration(%d)\n",
					vpp->bts_info.bpp, vpp->bts_info.is_rotation);

			exynos_bw_calc(win->vpp_id, &vpp->bts_info.hw);

			decon_dbg("cur_bw(%llu), peak_bw(%llu)\n",
					vpp->bts_info.cur_bw, vpp->bts_info.peak_bw);

			/* sum of each VPP's bandwidth */
			decon->total_bw += vpp->bts_info.cur_bw;
			/* max of each VPP's peak bandwidth */
			if (decon->max_peak_bw < vpp->bts_info.peak_bw)
				decon->max_peak_bw = vpp->bts_info.peak_bw;
		}
	}

	decon_dbg("total cur bw(%d), max peak bw(%d)\n",
			decon->total_bw, decon->max_peak_bw);
}

void bts2_update_bw(struct decon_device *decon, struct decon_reg_data *regs, u32 is_after)
{
	int i;
	struct decon_win *win;
	struct vpp_dev *vpp;
	struct v4l2_subdev *sd;
	struct decon_win_config *config;

	if (is_after) { /* after DECON h/w configuration */
		if (decon->total_bw < decon->prev_total_bw) {
			/* don't care 1st param */
			exynos_update_bw(0, decon->total_bw,
					decon->max_peak_bw);
		}

		for (i = 0; i < decon->pdata->max_win; i++) {
			win = decon->windows[i];
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				sd = decon->mdev->vpp_sd[win->vpp_id];
				vpp = v4l2_get_subdevdata(sd);
				config = &regs->vpp_config[i];

				if (!is_rotation(config))
					bts_ext_scenario_set(win->vpp_id,
							TYPE_ROTATION,
							is_rotation(config));
			}
		}

		decon->prev_total_bw = decon->total_bw;
		decon->prev_max_peak_bw = decon->max_peak_bw;
	} else {
		if (decon->total_bw > decon->prev_total_bw) {
			/* don't care 1st param */
			exynos_update_bw(0, decon->total_bw,
					decon->max_peak_bw);
		}

		for (i = 0; i < decon->pdata->max_win; i++) {
			win = decon->windows[i];
			if (decon->vpp_usage_bitmask & (1 << win->vpp_id)) {
				sd = decon->mdev->vpp_sd[win->vpp_id];
				vpp = v4l2_get_subdevdata(sd);
				config = &regs->vpp_config[i];

				if (is_rotation(config))
					bts_ext_scenario_set(win->vpp_id,
							TYPE_ROTATION,
							is_rotation(config));
			}
		}
	}
}

void bts2_release_bw(struct decon_device *decon)
{
	exynos_update_bw(0, 0, 0);

	decon->prev_total_bw = 0;
	decon->max_peak_bw = 0;
}

void bts2_release_rot_bw(enum decon_idma_type type)
{
	bts_ext_scenario_set(type, TYPE_ROTATION, 0);
}
#endif

struct decon_init_bts decon_init_bts_control = {
	.bts_add		= bts_add_request,
	.bts_set_init		= bts_send_init,
	.bts_release_init	= bts_send_release,
	.bts_remove		= bts_remove_request,
};

struct decon_bts decon_bts_control = {
	.bts_get_bw		= bts_calc_bw,
	.bts_set_calc_bw	= bts_send_calc_bw,
	.bts_set_zero_bw	= bts_send_zero_bw,
	.bts_set_rot_mif	= bts_mif_lock,
};

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
struct decon_bts2 decon_bts2_control = {
	.bts_calc_bw		= bts2_calc_bw,
	.bts_update_bw		= bts2_update_bw,
	.bts_release_bw		= bts2_release_bw,
	.bts_release_rot_bw	= bts2_release_rot_bw,
};
#endif
