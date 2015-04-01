/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos VPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_VPP_COMMON_H__
#define ___SAMSUNG_VPP_COMMON_H__

struct vpp_size_param {
	u32		src_x;
	u32		src_y;
	u32		src_fw;
	u32		src_fh;
	u32		src_w;
	u32		src_h;
	u32		dst_w;
	u32		dst_h;
	u32		fr_w;
	u32		fr_h;
	u32		fr_yx;
	u32		fr_yy;
	u32		fr_cx;
	u32		fr_cy;
	u32		vpp_h_ratio;
	u32		vpp_v_ratio;
	u32		rot;
	u32		block_w;
	u32		block_h;
	u32		block_x;
	u32		block_y;
	u64		addr0;
	u64		addr1;
	u64		addr2;
};

/* CAL APIs list */
void vpp_reg_set_realtime_path(u32 id);
void vpp_reg_set_framedone_irq(u32 id, u32 enable);
void vpp_reg_set_deadlock_irq(u32 id, u32 enable);
void vpp_reg_set_read_slave_err_irq(u32 id, u32 enable);
void vpp_reg_set_sfr_update_done_irq(u32 id, u32 enable);
void vpp_reg_set_hw_reset_done_mask(u32 id, u32 enable);
void vpp_reg_set_lookup_table(u32 id);
void vpp_reg_set_enable_interrupt(u32 id);
void vpp_reg_set_rgb_type(u32 id);
void vpp_reg_set_dynamic_clock_gating(u32 id);
void vpp_reg_set_plane_alpha_fixed(u32 id);

/* CAL raw functions list */
int vpp_reg_set_sw_reset(u32 id);
void vpp_reg_set_in_size(u32 id, struct vpp_size_param *p);
void vpp_reg_set_out_size(u32 id, u32 dst_w, u32 dst_h);
void vpp_reg_set_scale_ratio(u32 id, struct vpp_size_param *p, u32 rot_en);
int vpp_reg_set_in_format(u32 id, u32 format);
void vpp_reg_set_in_block_size(u32 id, u32 enable, struct vpp_size_param *p);
void vpp_reg_set_in_buf_addr(u32 id, struct vpp_size_param *p);
void vpp_reg_set_smart_if_pix_num(u32 id, u32 dst_w, u32 dst_h);
void vpp_reg_set_sfr_update_force(u32 id);
int vpp_reg_wait_op_status(u32 id);
int vpp_reg_set_rotation(u32 id, struct vpp_size_param *p);
void vpp_reg_set_plane_alpha(u32 id, u32 plane_alpha);
void vpp_reg_wait_idle(u32 id);
int vpp_reg_get_irq_status(u32 id);
void vpp_reg_set_clear_irq(u32 id, u32 irq);
void vpp_reg_init(u32 id);
void vpp_reg_deinit(u32 id, u32 reset_en);
#endif /* ___SAMSUNG_VPP_COMMON_H__ */
