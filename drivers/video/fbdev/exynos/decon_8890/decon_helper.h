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

#ifndef __SAMSUNG_DECON_HELPER_H__
#define __SAMSUNG_DECON_HELPER_H__

#include <linux/device.h>

#include "decon.h"

int decon_clk_set_parent(struct device *dev, const char *c, const char *p);
int decon_clk_set_rate(struct device *dev, struct clk *clk,
		const char *conid, unsigned long rate);
unsigned long decon_clk_get_rate(struct device *dev, const char *clkid);
void decon_to_psr_info(struct decon_device *decon, struct decon_mode_info *psr);
void decon_to_init_param(struct decon_device *decon, struct decon_param *p);
u32 decon_get_bpp(enum decon_pixel_format fmt);
int decon_get_plane_cnt(enum decon_pixel_format format);

#ifdef CONFIG_FB_DSU
void decon_bmpbuffer_settimer( int value );
int decon_bmpbuffer_gettimer( void );
int decon_bmpbuffer_is_storetime( void );
void decon_bmpbuffer_clear( void );

void decon_get_window_rect_log( char* buffer, struct decon_device *decon, struct decon_win_config_data *win_data );
void decon_store_window_rect_log( struct decon_device *decon, struct decon_win_config_data *win_data );
char* decon_last_window_rect_log( void );
void decon_print_bufered_window_rect_log( void );

int decon_bmpbuffer_log_window(struct decon_device *decon, struct decon_reg_data *regs, int *old_plane_cnt);
void decon_bmpbuffer_clear( void );
int decon_bmpbuffer_store_window(struct decon_device *decon, struct decon_reg_data *regs, int *old_plane_cnt);
int decon_bmpbuffer_write_file( void );
#endif

#endif /* __SAMSUNG_DECON_HELPER_H__ */
