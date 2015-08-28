/* drivers/video/fbdev/exynos/decon_8890/panels/s6e3hf4_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2015 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "s6e3hf4_param.h"
#include "lcd_ctrl.h"

/* use FW_TEST definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "../dsim_fw.h"
#include "mipi_display.h"
#else
#include "../dsim.h"
#include <video/mipi_display.h>
#endif

/* Porch values. It depends on command or video mode */
#define S6E3HF4_CMD_VBP		15
#define S6E3HF4_CMD_VFP		3
#define S6E3HF4_CMD_VSA		1
#define S6E3HF4_CMD_HBP		2
#define S6E3HF4_CMD_HFP		2
#define S6E3HF4_CMD_HSA		2

#define S6E3HF4_HORIZONTAL	1440
#define S6E3HF4_VERTICAL	2560

#ifdef FW_TEST /* This information is moved to DT */
#define CONFIG_FB_I80_COMMAND_MODE

#define TN

struct decon_lcd s6e3ha3_lcd_info = {
	.mode = DECON_MIPI_COMMAND_MODE,
	.vfp = S6E3HF4_CMD_VFP,
	.vbp = S6E3HF4_CMD_VBP,
	.hfp = S6E3HF4_CMD_HFP,
	.hbp = S6E3HF4_CMD_HBP,
	.vsa = S6E3HF4_CMD_VSA,
	.hsa = S6E3HF4_CMD_HSA,
	.xres = S6E3HF4_HORIZONTAL,
	.yres = S6E3HF4_VERTICAL,

	/* Maybe, width and height will be removed */
	.width = 70,
	.height = 121,

	/* Mhz */
	.hs_clk = 1100,
	.esc_clk = 20,

	.p = 3,
	.m = 127,
	.s = 0,

	.fps = 60,
	.mic_enabled = 0,
	.mic_ver = MIC_VER_1_2,

	.dsc_enabled = 1,
	.dsc_cnt = 2,
	.dsc_slice_num = 4,
};
#endif

/*
 * S6E3HF4 lcd init sequence
 *
 * Parameters
 *	- mic_enabled : if mic is enabled, MIC_ENABLE command must be sent
 *	- mode : LCD init sequence depends on command or video mode
 *	- 1/3 DSC 4 Slices
 */
void lcd_init(int id, struct decon_lcd *lcd)
{
	/* DSC setting */
	if (dsim_wr_data(id, MIPI_DSI_DSC_PRA, (unsigned long)SEQ_DSC_EN[0],
				SEQ_DSC_EN[1]) < 0)
		dsim_err("fail to write SEQ_DSC_EN command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DSC_PPS, (unsigned long)SEQ_PPS_SLICE4,
			ARRAY_SIZE(SEQ_PPS_SLICE4)) < 0)
		dsim_err("fail to write SEQ_PPS_SLICE4 command.\n");

	/* Sleep Out(11h) */
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE,
				(unsigned long)SEQ_SLEEP_OUT[0], 0) < 0)
		dsim_err("fail to send SEQ_SLEEP_OUT command.\n");

	msleep(120);

	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, SEQ_TE_ON[0], 0) < 0)
		dsim_err("fail to write SEQ_TE_ON init command.\n");

	if (dsim_wr_data(id, MIPI_DSI_DCS_LONG_WRITE, (unsigned long)SEQ_TEST_KEY_ON_2A,
				ARRAY_SIZE(SEQ_TEST_KEY_ON_2A)) < 0);
		dsim_err("fail to write SEQ_TE_ON command.\n");
}

void lcd_enable(int id)
{
	if (dsim_wr_data(id, MIPI_DSI_DCS_SHORT_WRITE, SEQ_DISPLAY_ON[0], 0) < 0)
		dsim_err("fail to send SEQ_DISPLAY_ON command.\n");
}

void lcd_disable(int id)
{
	/* This function needs to implement */
}
