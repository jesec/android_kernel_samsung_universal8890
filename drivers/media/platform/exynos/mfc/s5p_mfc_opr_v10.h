/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_opr_v10.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 * Contains declarations of hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S5P_MFC_OPR_V10_H
#define __S5P_MFC_OPR_V10_H __FILE__

#include "s5p_mfc_common.h"

#define MFC_CTRL_MODE_CUSTOM	MFC_CTRL_MODE_SFR

int s5p_mfc_set_dec_frame_buffer(struct s5p_mfc_ctx *ctx);
int s5p_mfc_set_dec_stream_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t buf_addr,
		unsigned int start_num_byte,
		unsigned int buf_size);

void s5p_mfc_set_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes);
int s5p_mfc_set_enc_stream_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr, unsigned int size);
void s5p_mfc_get_enc_frame_buffer(struct s5p_mfc_ctx *ctx,
		dma_addr_t addr[], int num_planes);
int s5p_mfc_set_enc_ref_buffer(struct s5p_mfc_ctx *mfc_ctx);

int s5p_mfc_decode_one_frame(struct s5p_mfc_ctx *ctx, int last_frame);
int s5p_mfc_encode_one_frame(struct s5p_mfc_ctx *ctx, int last_frame);

/* Memory allocation */
int s5p_mfc_alloc_dec_temp_buffers(struct s5p_mfc_ctx *ctx);
void s5p_mfc_set_dec_desc_buffer(struct s5p_mfc_ctx *ctx);
void s5p_mfc_release_dec_desc_buffer(struct s5p_mfc_ctx *ctx);

int s5p_mfc_alloc_codec_buffers(struct s5p_mfc_ctx *ctx);
void s5p_mfc_release_codec_buffers(struct s5p_mfc_ctx *ctx);

int s5p_mfc_alloc_instance_buffer(struct s5p_mfc_ctx *ctx);
void s5p_mfc_release_instance_buffer(struct s5p_mfc_ctx *ctx);
int s5p_mfc_alloc_dev_context_buffer(struct s5p_mfc_dev *dev);
void s5p_mfc_release_dev_context_buffer(struct s5p_mfc_dev *dev);

void s5p_mfc_dec_calc_dpb_size(struct s5p_mfc_ctx *ctx);
void s5p_mfc_enc_calc_src_size(struct s5p_mfc_ctx *ctx);

void s5p_mfc_try_run(struct s5p_mfc_dev *dev);
void s5p_mfc_cleanup_timeout_and_try_run(struct s5p_mfc_ctx *ctx);

void s5p_mfc_cleanup_queue(struct list_head *lh);

int s5p_mfc_get_new_ctx(struct s5p_mfc_dev *dev);

#endif /* __S5P_MFC_OPR_V10_H */
