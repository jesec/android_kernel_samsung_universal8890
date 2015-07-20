/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_intr.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 * It contains waiting functions declarations.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S5P_MFC_INTR_H
#define __S5P_MFC_INTR_H __FILE__

#include "s5p_mfc_common.h"

int s5p_mfc_wait_for_done_ctx(struct s5p_mfc_ctx *ctx, int command);
int s5p_mfc_wait_for_done_dev(struct s5p_mfc_dev *dev, int command);
void s5p_mfc_cleanup_timeout(struct s5p_mfc_ctx *ctx);

#endif /* __S5P_MFC_INTR_H */
