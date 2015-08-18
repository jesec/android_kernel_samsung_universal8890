/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_enc_param.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "s5p_mfc_enc_param.h"

int s5p_mfc_set_slice_mode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	/* multi-slice control */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES)
		MFC_WRITEL((enc->slice_mode + 0x4), S5P_FIMV_E_MSLICE_MODE);
	else if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW)
		MFC_WRITEL((enc->slice_mode - 0x2), S5P_FIMV_E_MSLICE_MODE);
	else
		MFC_WRITEL(enc->slice_mode, S5P_FIMV_E_MSLICE_MODE);

	/* multi-slice MB number or bit size */
	if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB || \
			enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW) {
		MFC_WRITEL(enc->slice_size.mb, S5P_FIMV_E_MSLICE_SIZE_MB);
	} else if (enc->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
		MFC_WRITEL(enc->slice_size.bits, S5P_FIMV_E_MSLICE_SIZE_BITS);
	} else {
		MFC_WRITEL(0x0, S5P_FIMV_E_MSLICE_SIZE_MB);
		MFC_WRITEL(0x0, S5P_FIMV_E_MSLICE_SIZE_BITS);
	}

	return 0;

}

static int s5p_mfc_set_enc_params(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0, pix_val;

	mfc_debug_enter();

	/* width */
	MFC_WRITEL(ctx->img_width, S5P_FIMV_E_FRAME_WIDTH); /* 16 align */
	/* height */
	MFC_WRITEL(ctx->img_height, S5P_FIMV_E_FRAME_HEIGHT); /* 16 align */

	/** cropped width */
	MFC_WRITEL(ctx->img_width, S5P_FIMV_E_CROPPED_FRAME_WIDTH);
	/** cropped height */
	MFC_WRITEL(ctx->img_height, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);
	/** cropped offset */
	MFC_WRITEL(0x0, S5P_FIMV_E_FRAME_CROP_OFFSET);

	/* pictype : IDR period */
	reg = 0;
	reg |= p->gop_size & 0xFFFF;
	MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);

	if(FW_HAS_GOP2(dev)) {
		reg = 0;
		reg |= (p->gop_size >> 16) & 0x3FFF;
		MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG2);
	}

	/* multi-slice control */
	/* multi-slice MB number or bit size */
	enc->slice_mode = p->slice_mode;

	MFC_WRITEL(0, S5P_FIMV_E_ENC_OPTIONS);

	if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_MB) {
		enc->slice_size.mb = p->slice_mb;
	} else if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SICE_MODE_MAX_BYTES) {
		enc->slice_size.bits = p->slice_bit;
	} else if (p->slice_mode == V4L2_MPEG_VIDEO_MULTI_SLICE_MODE_MAX_MB_ROW) {
		enc->slice_size.mb = p->slice_mb_row * ((ctx->img_width + 15) / 16);
	} else {
		enc->slice_size.mb = 0;
		enc->slice_size.bits = 0;
	}

	s5p_mfc_set_slice_mode(ctx);

	/* cyclic intra refresh */
	MFC_WRITEL(p->intra_refresh_mb, S5P_FIMV_E_IR_SIZE);
	reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
	if (p->intra_refresh_mb == 0)
		reg &= ~(0x1 << 4);
	else
		reg |= (0x1 << 4);
	MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* 'NON_REFERENCE_STORE_ENABLE' for debugging */
	reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
	reg &= ~(0x1 << 9);
	MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* memory structure cur. frame */
	if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) {
		/* 0: Linear, 1: 2D tiled*/
		reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
		reg |= (0x1 << 7);
		MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);
	} else {
		/* 0: Linear, 1: 2D tiled*/
		reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
		reg &= ~(0x1 << 7);
		MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);
	}

	switch (ctx->src_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV12MT_16X16:
		pix_val = 0;
		break;
	case V4L2_PIX_FMT_NV21M:
		pix_val = 1;
		break;
	case V4L2_PIX_FMT_YVU420M:
		pix_val = 2;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV420N:
		pix_val = 3;
		break;
	case V4L2_PIX_FMT_ARGB32:
		pix_val = 8;
		break;
	case V4L2_PIX_FMT_RGB24:
		pix_val = 9;
		break;
	case V4L2_PIX_FMT_RGB565:
		pix_val = 10;
		break;
	case V4L2_PIX_FMT_RGB32X:
		pix_val = 12;
		break;
	case V4L2_PIX_FMT_BGR32:
		pix_val = 13;
		break;
	default:
		pix_val = 0;
		break;
	}
	MFC_WRITEL(pix_val, S5P_FIMV_PIXEL_FORMAT);

	/* memory structure recon. frame */
	/* 0: Linear, 1: 2D tiled */
	reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
	reg |= (0x1 << 8);
	MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* padding control & value */
	MFC_WRITEL(0x0, S5P_FIMV_E_PADDING_CTRL);
	if (p->pad) {
		reg = 0;
		/** enable */
		reg |= (1 << 31);
		/** cr value */
		reg &= ~(0xFF << 16);
		reg |= (p->pad_cr << 16);
		/** cb value */
		reg &= ~(0xFF << 8);
		reg |= (p->pad_cb << 8);
		/** y value */
		reg &= ~(0xFF);
		reg |= (p->pad_luma);
		MFC_WRITEL(reg, S5P_FIMV_E_PADDING_CTRL);
	}

	/* rate control config. */
	reg = 0;
	/** frame-level rate control */
	reg &= ~(0x1 << 9);
	reg |= (p->rc_frame << 9);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* bit rate */
	if (p->rc_frame)
		MFC_WRITEL(p->rc_bitrate,
			S5P_FIMV_E_RC_BIT_RATE);
	else
		MFC_WRITEL(1, S5P_FIMV_E_RC_BIT_RATE);

	if (p->rc_frame) {
		if (FW_HAS_ADV_RC_MODE(dev)) {
			if (FW_HAS_I_LIMIT_RC_MODE(dev) &&
				p->rc_reaction_coeff <= I_LIMIT_CBR_MAX)
				reg = S5P_FIMV_ENC_ADV_I_LIMIT_CBR;
			else if (p->rc_reaction_coeff <= TIGHT_CBR_MAX)
				reg = S5P_FIMV_ENC_ADV_TIGHT_CBR;
			else
				reg = S5P_FIMV_ENC_ADV_CAM_CBR;
		} else {
			if (p->rc_reaction_coeff <= TIGHT_CBR_MAX)
				reg = S5P_FIMV_ENC_TIGHT_CBR;
			else
				reg = S5P_FIMV_ENC_LOOSE_CBR;
		}

		MFC_WRITEL(reg, S5P_FIMV_E_RC_MODE);
	}

	/* extended encoder ctrl */
	/** vbv buffer size */
	if (p->frame_skip_mode == V4L2_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE_BUF_LIMIT)
		MFC_WRITEL(p->vbv_buf_size, S5P_FIMV_E_VBV_BUFFER_SIZE);

	/** seq header ctrl */
	reg = MFC_READL(S5P_FIMV_E_ENC_OPTIONS);
	reg &= ~(0x1 << 2);
	reg |= (p->seq_hdr_mode << 2);
	/** frame skip mode */
	reg &= ~(0x3);
	reg |= (p->frame_skip_mode);
	MFC_WRITEL(reg, S5P_FIMV_E_ENC_OPTIONS);

	/* 'DROP_CONTROL_ENABLE', disable */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	reg &= ~(0x1 << 10);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* setting for MV range [16, 256] */
	if (mfc_version(dev) == 0x61)
		reg = ENC_V61_MV_RANGE;
	else if (mfc_version(dev) == 0x78)
		reg = ENC_V78_MV_RANGE;
	else
		reg = ENC_V65_MV_RANGE;
	MFC_WRITEL(reg, S5P_FIMV_E_MV_HOR_RANGE);
	MFC_WRITEL(reg, S5P_FIMV_E_MV_VER_RANGE);

	/* Disable all macroblock adaptive scaling features */
	reg = 0xF;
	MFC_WRITEL(reg, S5P_FIMV_E_MB_RC_CONFIG);

	MFC_WRITEL(0x0, S5P_FIMV_E_VBV_INIT_DELAY); /* SEQ_start Only */

	/* initialize for '0' only setting */
	MFC_WRITEL(0x0, S5P_FIMV_E_FRAME_INSERTION); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_ROI_BUFFER_ADDR); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_PARAM_CHANGE); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_RC_ROI_CTRL); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_PICTURE_TAG); /* NAL_start Only */

	MFC_WRITEL(0x0, S5P_FIMV_E_BIT_COUNT_ENABLE); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_MAX_BIT_COUNT); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_MIN_BIT_COUNT); /* NAL_start Only */

	MFC_WRITEL(0x0, S5P_FIMV_E_METADATA_BUFFER_ADDR); /* NAL_start Only */
	MFC_WRITEL(0x0, S5P_FIMV_E_METADATA_BUFFER_SIZE); /* NAL_start Only */

	mfc_debug_leave();

	return 0;
}

int s5p_mfc_set_enc_params_h264(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_h264_enc_params *p_264 = &p->codec.h264;
	unsigned int reg = 0, reg2 = 0;
	int i;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	if (!IS_MFCv78(dev)) {
		/* pictype : number of B */
		reg = MFC_READL(S5P_FIMV_E_GOP_CONFIG);
		/** num_b_frame - 0 ~ 2 */
		reg &= ~(0x3 << 16);
		reg |= (p->num_b_frame << 16);
		MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);
	}

	/* UHD encoding case */
	if(is_UHD(ctx)) {
		if (p_264->level < 51) {
			mfc_info_ctx("Set Level 5.1 for UHD\n");
			p_264->level = 51;
		}
		if (p_264->profile != 0x2) {
			mfc_info_ctx("Set High profile for UHD\n");
			p_264->profile = 0x2;
		}
	}

	/* profile & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_264->level << 8);
	/** profile - 0 ~ 3 */
	reg &= ~(0x3F);
	reg |= p_264->profile;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* interlace */
	reg = 0;
	reg &= ~(0x1 << 3);
	reg |= (p_264->interlace << 3);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/** height */
	if (p_264->interlace) {
		MFC_WRITEL(ctx->img_height >> 1, S5P_FIMV_E_FRAME_HEIGHT); /* 32 align */
		/** cropped height */
		MFC_WRITEL(ctx->img_height >> 1, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);
	}

	/* loop filter ctrl */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x3 << 1);
	reg |= (p_264->loop_filter_mode << 1);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* loopfilter alpha offset */
	if (p_264->loop_filter_alpha < 0) {
		reg = 0x10;
		reg |= (0xFF - p_264->loop_filter_alpha) + 1;
	} else {
		reg = 0x00;
		reg |= (p_264->loop_filter_alpha & 0xF);
	}
	MFC_WRITEL(reg, S5P_FIMV_E_H264_LF_ALPHA_OFFSET);

	/* loopfilter beta offset */
	if (p_264->loop_filter_beta < 0) {
		reg = 0x10;
		reg |= (0xFF - p_264->loop_filter_beta) + 1;
	} else {
		reg = 0x00;
		reg |= (p_264->loop_filter_beta & 0xF);
	}
	MFC_WRITEL(reg, S5P_FIMV_E_H264_LF_BETA_OFFSET);

	/* entropy coding mode */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1);
	reg |= (p_264->entropy_mode);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* number of ref. picture */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 7);
	if (!IS_MFCv78(dev))
		reg |= ((p_264->num_ref_pic_4p-1) << 7);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* 8x8 transform enable */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x3 << 12);
	reg |= (p_264->_8x8_transform << 12);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_264->rc_frame_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	if (p->rc_frame == 0 && p->rc_mb == 0) {
		reg |= (0x1 << 11);
		MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	}

	/* frame rate */
	/* Fix value for H.264, H.263 in the driver */
	p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_264->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_264->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_264->rc_min_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* macroblock adaptive scaling features */
	if (p->rc_mb) {
		reg = 0;
		/** dark region */
		reg &= ~(0x1 << 3);
		reg |= (p_264->rc_mb_dark << 3);
		/** smooth region */
		reg &= ~(0x1 << 2);
		reg |= (p_264->rc_mb_smooth << 2);
		/** static region */
		reg &= ~(0x1 << 1);
		reg |= (p_264->rc_mb_static << 1);
		/** high activity region */
		reg &= ~(0x1);
		reg |= p_264->rc_mb_activity;
		MFC_WRITEL(reg, S5P_FIMV_E_MB_RC_CONFIG);
	}

	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 16);
		reg |= (p_264->rc_b_frame_qp << 16);
		reg &= ~(0x3f << 8);
		reg |= (p_264->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_264->rc_frame_qp;
		MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* sps pps control */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 29);
	reg |= (p_264->prepend_sps_pps_to_idr << 29);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/*
	 * CONSTRAINT_SET0_FLAG: all constraints specified in
	 * Baseline Profile
	 */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg |= (0x1 << 26);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* extended encoder ctrl */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 5);
	reg |= (p_264->ar_vui << 5);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	MFC_WRITEL(0x0, S5P_FIMV_E_ASPECT_RATIO);
	MFC_WRITEL(0x0, S5P_FIMV_E_EXTENDED_SAR);
	if (p_264->ar_vui) {
		/* aspect ration IDC */
		reg = 0;
		reg &= ~(0xff);
		reg |= p_264->ar_vui_idc;
		MFC_WRITEL(reg, S5P_FIMV_E_ASPECT_RATIO);
		if (p_264->ar_vui_idc == 0xFF) {
			/* sample  AR info. */
			reg = 0;
			reg &= ~(0xffffffff);
			reg |= p_264->ext_sar_width << 16;
			reg |= p_264->ext_sar_height;
			MFC_WRITEL(reg, S5P_FIMV_E_EXTENDED_SAR);
		}
	}

	/* intra picture period for H.264 open GOP */
	/** control */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	/* from 9.0 Version changed register meaning*/
	reg &= ~(0x1 << 4);
	p_264->open_gop ^= S5P_FIMV_E_IDR_H264_IDR;
	reg |= (p_264->open_gop << 4);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);
	/** value */
	MFC_WRITEL(0x0, S5P_FIMV_E_H264_REFRESH_PERIOD);
	if (p_264->open_gop) {
		reg = 0;
		reg &= ~(0xffff);
		reg |= p_264->open_gop_size;
		MFC_WRITEL(reg, S5P_FIMV_E_H264_REFRESH_PERIOD);
	}

	/* 'WEIGHTED_BI_PREDICTION' for B is disable */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x3 << 9);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* 'CONSTRAINED_INTRA_PRED_ENABLE' is disable */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 14);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* ASO enable */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	if (p_264->aso_enable)
		reg |= (0x1 << 6);
	else
		reg &= ~(0x1 << 6);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

	/* VUI parameter disable */
	if (FW_HAS_VUI_PARAMS(dev)) {
		reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
		if (p_264->vui_enable)
			reg |= (0x1 << 30);
		else
			reg &= ~(0x1 << 30);
		MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);
	}

	/* pic_order_cnt_type = 0 for backward compatibilities */
	if (FW_HAS_POC_TYPE_CTRL(dev)) {
		reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS_2);
		reg &= ~(0x1 << 2);
		reg |= (p_264->enable_ltr << 2); /* Enable LTR */
		reg &= ~(0x3 << 0);
		reg |= (0x0 << 0); /* TODO: add new CID for this */
		MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS_2);
		mfc_debug(2, "enable_ltr : %d\n", p_264->enable_ltr);
	}

	/* hier qp enable */
	reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
	reg &= ~(0x1 << 8);
	reg |= ((p_264->hier_qp_enable & 0x1) << 8);
	MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);
	mfc_debug(2, "hier_qp_enable : %d, num_hier_layer : %d\n", p_264->hier_qp_enable, p_264->num_hier_layer);
	reg = 0;
	/* number of coding layer should be zero when hierarchical is disable */
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);

	if (p_264->num_hier_layer) {
		reg |= (p_264->hier_qp_type & 0x1) << 0x3;
		reg |= p_264->num_hier_layer & 0x7;
		if (p_264->hier_ref_type) {
			reg |= 0x1 << 7;
			reg |= (p_264->num_hier_layer & 0x7) << 4;
		} else {
			reg |= 0x7 << 4;
		}
		MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
		mfc_debug(2, "set NUM_T_LAYER : enable_ltr %d, num_hier_layer: %d, hier_ref_type : %d, NUM_T_LAYER: 0x%x\n",
				p_264->enable_ltr, p_264->num_hier_layer, p_264->hier_ref_type, reg);
		/* QP value for each layer */
		if (p_264->hier_qp_enable) {
			for (i = 0; i < (p_264->num_hier_layer & 0x7); i++)
			MFC_WRITEL(p_264->hier_qp_layer[i],
					S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		}
		if (p->rc_frame) {
			for (i = 0; i < (p_264->num_hier_layer & 0x7); i++)
			MFC_WRITEL(p_264->hier_bit_layer[i],
					S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		}
		if (p_264->set_priority)
		{
			reg = 0;
			reg2 = 0;
			for (i = 0; i < (p_264->num_hier_layer & 0x7); i++) {
				if (i <= 4)
					reg |= ((p_264->base_priority & 0x3F) + i) << (6 * i);
				else
					reg2 |= ((p_264->base_priority & 0x3F) + i) << (6 * (i - 5));
			}
			MFC_WRITEL(reg, S5P_FIMV_E_H264_HD_SVC_EXTENSION_0);
			MFC_WRITEL(reg2, S5P_FIMV_E_H264_HD_SVC_EXTENSION_1);
		}
	}

	/* set frame pack sei generation */
	if (p_264->sei_gen_enable) {
		/* frame packing enable */
		reg = MFC_READL(S5P_FIMV_E_H264_OPTIONS);
		reg |= (1 << 25);
		MFC_WRITEL(reg, S5P_FIMV_E_H264_OPTIONS);

		/* set current frame0 flag & arrangement type */
		reg = 0;
		/** current frame0 flag */
		reg |= ((p_264->sei_fp_curr_frame_0 & 0x1) << 2);
		/** arrangement type */
		reg |= (p_264->sei_fp_arrangement_type - 3) & 0x3;
		MFC_WRITEL(reg, S5P_FIMV_E_H264_FRAME_PACKING_SEI_INFO);
	}

	if (p_264->fmo_enable) {
		switch (p_264->fmo_slice_map_type) {
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES:
			if (p_264->fmo_slice_num_grp > 4)
				p_264->fmo_slice_num_grp = 4;
			for (i = 0; i < (p_264->fmo_slice_num_grp & 0xF); i++)
				MFC_WRITEL(p_264->fmo_run_length[i] - 1,
				S5P_FIMV_E_H264_FMO_RUN_LENGTH_MINUS1_0 + i*4);
			break;
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_SCATTERED_SLICES:
			if (p_264->fmo_slice_num_grp > 4)
				p_264->fmo_slice_num_grp = 4;
			break;
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_RASTER_SCAN:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_WIPE_SCAN:
			if (p_264->fmo_slice_num_grp > 2)
				p_264->fmo_slice_num_grp = 2;
			MFC_WRITEL(p_264->fmo_sg_dir & 0x1,
				S5P_FIMV_E_H264_FMO_SLICE_GRP_CHANGE_DIR);
			/* the valid range is 0 ~ number of macroblocks -1 */
			MFC_WRITEL(p_264->fmo_sg_rate, S5P_FIMV_E_H264_FMO_SLICE_GRP_CHANGE_RATE_MINUS1);
			break;
		default:
			mfc_err_ctx("Unsupported map type for FMO: %d\n",
					p_264->fmo_slice_map_type);
			p_264->fmo_slice_map_type = 0;
			p_264->fmo_slice_num_grp = 1;
			break;
		}

		MFC_WRITEL(p_264->fmo_slice_map_type, S5P_FIMV_E_H264_FMO_SLICE_GRP_MAP_TYPE);
		MFC_WRITEL(p_264->fmo_slice_num_grp - 1, S5P_FIMV_E_H264_FMO_NUM_SLICE_GRP_MINUS1);
	} else {
		MFC_WRITEL(0, S5P_FIMV_E_H264_FMO_NUM_SLICE_GRP_MINUS1);
	}

	mfc_debug_leave();

	return 0;
}

int s5p_mfc_set_enc_params_mpeg4(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_mpeg4_enc_params *p_mpeg4 = &p->codec.mpeg4;
	unsigned int reg = 0;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	if (!IS_MFCv78(dev)) {
		/* pictype : number of B */
		reg = MFC_READL(S5P_FIMV_E_GOP_CONFIG);
		/** num_b_frame - 0 ~ 2 */
		reg &= ~(0x3 << 16);
		reg |= (p->num_b_frame << 16);
		MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);
	}

	/* profile & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_mpeg4->level << 8);
	/** profile - 0 ~ 1 */
	reg &= ~(0x3F);
	reg |= p_mpeg4->profile;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* quarter_pixel */
	/* MFC_WRITEL(p_mpeg4->quarter_pixel, S5P_FIMV_ENC_MPEG4_QUART_PXL); */

	/* qp */
	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 16);
		reg |= (p_mpeg4->rc_b_frame_qp << 16);
		reg &= ~(0x3f << 8);
		reg |= (p_mpeg4->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_mpeg4->rc_frame_qp;
		MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* frame rate */
	if (p->rc_frame) {
		p->rc_frame_delta = p_mpeg4->vop_frm_delta;
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= (p_mpeg4->vop_time_res << 16);
		reg &= ~(0xffff);
		reg |= p_mpeg4->vop_frm_delta;
		MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	} else {
		p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	}

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_frame_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_mpeg4->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_min_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* initialize for '0' only setting*/
	MFC_WRITEL(0x0, S5P_FIMV_E_MPEG4_OPTIONS); /* SEQ_start only */
	MFC_WRITEL(0x0, S5P_FIMV_E_MPEG4_HEC_PERIOD); /* SEQ_start only */

	mfc_debug_leave();

	return 0;
}

int s5p_mfc_set_enc_params_h263(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_mpeg4_enc_params *p_mpeg4 = &p->codec.mpeg4;
	unsigned int reg = 0;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* profile & level */
	reg = 0;
	/** profile */
	reg |= (0x1 << 4);
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* qp */
	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 8);
		reg |= (p_mpeg4->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_mpeg4->rc_frame_qp;
		MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* frame rate */
	/* Fix value for H.264, H.263 in the driver */
	p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_mpeg4->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_frame_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_mpeg4->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_mpeg4->rc_min_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	mfc_debug_leave();

	return 0;
}

int s5p_mfc_set_enc_params_vp8(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_vp8_enc_params *p_vp8 = &p->codec.vp8;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* profile*/
	reg = 0;
	reg |= (p_vp8->vp8_version) ;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	reg = 0;
	/* Disable IVF header */
	reg |= (0x1 << 12);
	reg |= (p_vp8->intra_4x4mode_disable & 0x1) << 10;
	reg |= (p_vp8->vp8_numberofpartitions & 0xF) << 3;
	reg |= (p_vp8->num_refs_for_p - 1) & 0x1;
	MFC_WRITEL(reg, S5P_FIMV_E_VP8_OPTION);

	reg = 0;
	reg |= (p_vp8->vp8_goldenframesel & 0x1);
	reg |= (p_vp8->vp8_gfrefreshperiod & 0xffff) << 1;
	MFC_WRITEL(reg, S5P_FIMV_E_VP8_GOLDEN_FRAME_OPTION);

	/* hier qp enable */
	reg = MFC_READL(S5P_FIMV_E_VP8_OPTION);
	reg &= ~(0x1 << 11);
	reg |= ((p_vp8->hier_qp_enable & 0x1) << 11);
	MFC_WRITEL(reg, S5P_FIMV_E_VP8_OPTION);
	reg = 0;
	if (p_vp8->num_hier_layer) {
		reg |= p_vp8->num_hier_layer & 0x3;
		MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
		/* QP value for each layer */
		if (p_vp8->hier_qp_enable) {
			for (i = 0; i < (p_vp8->num_hier_layer & 0x3); i++)
			MFC_WRITEL(p_vp8->hier_qp_layer[i],
					S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		}
		if (p->rc_frame) {
			for (i = 0; i < (p_vp8->num_hier_layer & 0x3); i++)
			MFC_WRITEL(p_vp8->hier_bit_layer[i],
					S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		}
	}
	/* number of coding layer should be zero when hierarchical is disable */
	reg |= p_vp8->num_hier_layer;
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);

	reg = 0;
	reg |= (p_vp8->vp8_filtersharpness & 0x7);
	reg |= (p_vp8->vp8_filterlevel & 0x3f) << 8;
	MFC_WRITEL(reg , S5P_FIMV_E_VP8_FILTER_OPTIONS);

	/* qp */
	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 8);
		reg |= (p_vp8->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_vp8->rc_frame_qp;
		MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* frame rate */
	p->rc_frame_delta = FRAME_DELTA_DEFAULT;

	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_vp8->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_vp8->rc_frame_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_vp8->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_vp8->rc_min_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	mfc_debug_leave();

	return 0;
}

int s5p_mfc_set_enc_params_vp9(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_vp9_enc_params *p_vp9 = &p->codec.vp9;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* profile*/
	reg = 0;
	reg |= (p_vp9->vp9_version) ;
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	reg = 0;
	reg |= ((p_vp9->ivf_header & 0x1) << 12);
	reg |= ((p_vp9->hier_qp_enable & 0x1) << 11);
	reg |= (p_vp9->max_partition_depth & 0x1) << 3;
	reg |= (p_vp9->intra_pu_split_disable & 0x1) << 1;
	reg |= (p_vp9->num_refs_for_p - 1) & 0x1;
	MFC_WRITEL(reg, S5P_FIMV_E_VP9_OPTION);

	reg = 0;
	reg |= (p_vp9->vp9_goldenframesel & 0x1);
	reg |= (p_vp9->vp9_gfrefreshperiod & 0xffff) << 1;
	MFC_WRITEL(reg, S5P_FIMV_E_VP9_GOLDEN_FRAME_OPTION);

	reg = 0;
	if (p_vp9->num_hier_layer) {
		reg |= p_vp9->num_hier_layer & 0x3;
		MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
		/* QP value for each layer */
		if (p_vp9->hier_qp_enable) {
			for (i = 0; i < (p_vp9->num_hier_layer & 0x3); i++)
			MFC_WRITEL(p_vp9->hier_qp_layer[i],
					S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		}
		if (p->rc_frame) {
			for (i = 0; i < (p_vp9->num_hier_layer & 0x3); i++)
			MFC_WRITEL(p_vp9->hier_bit_layer[i],
					S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		}
	}
	/* number of coding layer should be zero when hierarchical is disable */
	reg |= p_vp9->num_hier_layer;
	MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);

	/* qp */
	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0xff << 8);
		reg |= (p_vp9->rc_p_frame_qp << 8);
		reg &= ~(0xff);
		reg |= p_vp9->rc_frame_qp;
		MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	/* frame rate */
	p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_vp9->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	/** frame QP */
	reg &= ~(0xFF);
	reg |= p_vp9->rc_frame_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0xFF << 8);
	reg |= (p_vp9->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0xFF);
	reg |= p_vp9->rc_min_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	mfc_debug_leave();

	return 0;
}

int s5p_mfc_set_enc_params_hevc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_hevc_enc_params *p_hevc = &p->codec.hevc;
	unsigned int reg = 0;
	int i;

	mfc_debug_enter();

	s5p_mfc_set_enc_params(ctx);

	/* pictype : number of B */
	reg = MFC_READL(S5P_FIMV_E_GOP_CONFIG);
	/** num_b_frame - 0 ~ 2 */
	reg &= ~(0x3 << 16);
	reg |= (p->num_b_frame << 16);
	MFC_WRITEL(reg, S5P_FIMV_E_GOP_CONFIG);

	/* UHD encoding case */
	if ((ctx->img_width == 3840) && (ctx->img_height == 2160)) {
		p_hevc->level = 51;
		p_hevc->tier_flag = 0;
	/* this tier_flag can be changed */
	}

	/* tier_flag & level */
	reg = 0;
	/** level */
	reg &= ~(0xFF << 8);
	reg |= (p_hevc->level << 8);
	/** tier_flag - 0 ~ 1 */
	reg |= (p_hevc->tier_flag << 16);
	MFC_WRITEL(reg, S5P_FIMV_E_PICTURE_PROFILE);

	/* max partition depth */
	reg = 0;
	reg |= (p_hevc->max_partition_depth & 0x1);
	reg |= (p_hevc->num_refs_for_p-1) << 2;
	reg |= (2 << 3); /* always set IDR encoding */
	reg |= (p_hevc->const_intra_period_enable & 0x1) << 5;
	reg |= (p_hevc->lossless_cu_enable & 0x1) << 6;
	reg |= (p_hevc->wavefront_enable & 0x1) << 7;
	reg |= (p_hevc->loopfilter_disable & 0x1) << 8;
	reg |= (p_hevc->enable_ltr & 0x1) << 10;
	reg |= (p_hevc->hier_qp_enable & 0x1) << 11;
	reg |= (p_hevc->sign_data_hiding & 0x1) << 12;
	reg |= (p_hevc->general_pb_enable & 0x1) << 13;
	reg |= (p_hevc->temporal_id_enable & 0x1) << 14;
	reg |= (p_hevc->strong_intra_smooth & 0x1) << 15;
	reg |= (p_hevc->intra_pu_split_disable & 0x1) << 16;
	reg |= (p_hevc->tmv_prediction_disable & 0x1) << 17;
	reg |= (p_hevc->max_num_merge_mv & 0x7) << 18;
	/* reg |= (1 << 21); always eco mode disable */
	reg |= (p_hevc->encoding_nostartcode_enable & 0x1) << 22;
	MFC_WRITEL(reg, S5P_FIMV_E_HEVC_OPTIONS);
	/* refresh period */
	if (p_hevc->refreshtype) {
		reg = 0;
		reg |= (p_hevc->refreshperiod & 0x1);
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_REFRESH_PERIOD);
	}
	/* loop filter setting */
	reg |= (p_hevc->loopfilter_disable & 0x01) << 8;
	if (!p_hevc->loopfilter_disable) {
		reg = 0;
		reg |= (p_hevc->lf_beta_offset_div2 & 0x1);
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_LF_BETA_OFFSET_DIV2);
		reg = 0;
		reg |= (p_hevc->lf_tc_offset_div2 & 0x1);
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_LF_TC_OFFSET_DIV2);
		reg = MFC_READL(S5P_FIMV_E_HEVC_OPTIONS);
		reg &= ~(0x1 << 9);
		reg |= (p_hevc->loopfilter_across & 0x1) << 9;
	}
	/* long term reference */
	if (p_hevc->enable_ltr) {
		reg = 0;
		reg |= (p_hevc->store_ref & 0x3);
		reg &= ~(0x3 << 2);
		reg |= (p_hevc->user_ref & 0x3) << 2;
		MFC_WRITEL(reg, S5P_FIMV_E_HEVC_NAL_CONTROL);
	}
	/* hier qp enable */
	if (p_hevc->num_hier_layer) {
		reg |= (p_hevc->hier_qp_type & 0x1) << 0x3;
		reg |= p_hevc->num_hier_layer & 0x7;
		MFC_WRITEL(reg, S5P_FIMV_E_NUM_T_LAYER);
		/* QP value for each layer */
		if (p_hevc->hier_qp_enable) {
			for (i = 0; i < (p_hevc->num_hier_layer & 0x7); i++)
				MFC_WRITEL(p_hevc->hier_qp_layer[i],
					S5P_FIMV_E_HIERARCHICAL_QP_LAYER0 + i * 4);
		}
		if (p->rc_frame) {
			for (i = 0; i < (p_hevc->num_hier_layer & 0x7); i++)
				MFC_WRITEL(p_hevc->hier_bit_layer[i],
					S5P_FIMV_E_HIERARCHICAL_BIT_RATE_LAYER0 + i * 4);
		}
	}
	/* rate control config. */
	reg = MFC_READL(S5P_FIMV_E_RC_CONFIG);
	/** macroblock level rate control */
	reg &= ~(0x1 << 8);
	reg |= (p->rc_mb << 8);
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);
	/** frame QP */
	reg &= ~(0x3F);
	reg |= p_hevc->rc_frame_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_CONFIG);

	/* frame rate */

	p->rc_frame_delta = FRAME_DELTA_DEFAULT;
	if (p->rc_frame) {
		reg = 0;
		reg &= ~(0xffff << 16);
		reg |= ((p_hevc->rc_framerate * p->rc_frame_delta) << 16);
		reg &= ~(0xffff);
		reg |= p->rc_frame_delta;
		MFC_WRITEL(reg, S5P_FIMV_E_RC_FRAME_RATE);
	}

	/* max & min value of QP */
	reg = 0;
	/** max QP */
	reg &= ~(0x3F << 8);
	reg |= (p_hevc->rc_max_qp << 8);
	/** min QP */
	reg &= ~(0x3F);
	reg |= p_hevc->rc_min_qp;
	MFC_WRITEL(reg, S5P_FIMV_E_RC_QP_BOUND);

	/* macroblock adaptive scaling features */
	if (p->rc_mb) {
		reg = 0;
		/** dark region */
		reg &= ~(0x1 << 3);
		reg |= (p_hevc->rc_lcu_dark << 3);
		/** smooth region */
		reg &= ~(0x1 << 2);
		reg |= (p_hevc->rc_lcu_smooth << 2);
		/** static region */
		reg &= ~(0x1 << 1);
		reg |= (p_hevc->rc_lcu_static << 1);
		/** high activity region */
		reg &= ~(0x1);
		reg |= p_hevc->rc_lcu_activity;
		MFC_WRITEL(reg, S5P_FIMV_E_MB_RC_CONFIG);
	}
	MFC_WRITEL(0x0, S5P_FIMV_E_FIXED_PICTURE_QP);
	if (!p->rc_frame && !p->rc_mb) {
		reg = 0;
		reg &= ~(0x3f << 16);
		reg |= (p_hevc->rc_b_frame_qp << 16);
		reg &= ~(0x3f << 8);
		reg |= (p_hevc->rc_p_frame_qp << 8);
		reg &= ~(0x3f);
		reg |= p_hevc->rc_frame_qp;
		MFC_WRITEL(reg, S5P_FIMV_E_FIXED_PICTURE_QP);
	}
		mfc_debug_leave();

	return 0;
}

