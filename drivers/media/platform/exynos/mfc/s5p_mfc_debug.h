/*
 * drivers/media/video/exynos/mfc/s5p_mfc_debug.h
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains debug macros
 *
 * Kamil Debski, Copyright (c) 2010 Samsung Electronics
 * http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S5P_MFC_DEBUG_H
#define __S5P_MFC_DEBUG_H __FILE__

#define DEBUG

#ifdef DEBUG
extern int debug;

#define mfc_debug(level, fmt, args...)				\
	do {							\
		if (debug >= level)				\
			printk(KERN_DEBUG "%s:%d: " fmt,	\
				__func__, __LINE__, ##args);	\
	} while (0)
#else
#define mfc_debug(fmt, args...)
#endif

#define mfc_debug_enter() mfc_debug(5, "enter")
#define mfc_debug_leave() mfc_debug(5, "leave")

#define mfc_err(fmt, args...)				\
	do {						\
		printk(KERN_ERR "%s:%d: " fmt,		\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define mfc_err_dev(fmt, args...)			\
	do {						\
		printk(KERN_ERR "[d:%d] %s:%d: " fmt,	\
			dev->id,			\
		       __func__, __LINE__, ##args);	\
	} while (0)

#define mfc_err_ctx(fmt, args...)				\
	do {							\
		printk(KERN_ERR "[d:%d, c:%d] %s:%d: " fmt,	\
			ctx->dev->id, ctx->num,			\
		       __func__, __LINE__, ##args);		\
	} while (0)

#define mfc_info_dev(fmt, args...)			\
	do {						\
		printk(KERN_INFO "[d:%d] %s:%d: " fmt,	\
			dev->id,			\
			__func__, __LINE__, ##args);	\
	} while (0)

#define mfc_info_ctx(fmt, args...)				\
	do {							\
		printk(KERN_INFO "[d:%d, c:%d] %s:%d: " fmt,	\
			ctx->dev->id, ctx->num,			\
			__func__, __LINE__, ##args);		\
	} while (0)

#define MFC_DEV_NUM_MAX			2
#define MFC_TRACE_STR_LEN		80
#define MFC_TRACE_COUNT_MAX		1024
	struct _mfc_trace {
		unsigned long long time;
		char str[MFC_TRACE_STR_LEN];
	};

#define MFC_TRACE_DEV(fmt, args...)							\
		do {											\
			int cpu = raw_smp_processor_id();						\
			int cnt;									\
			cnt = atomic_inc_return(&dev->trace_ref) & (MFC_TRACE_COUNT_MAX - 1);	\
			dev->mfc_trace[cnt].time = cpu_clock(cpu);					\
			snprintf(dev->mfc_trace[cnt].str, MFC_TRACE_STR_LEN,			\
					"[d:%d] " fmt, dev->id, ##args);				\
		} while(0)

#define MFC_TRACE_CTX(fmt, args...)							\
		do {											\
			int cpu = raw_smp_processor_id();						\
			int cnt;									\
			cnt = atomic_inc_return(&dev->trace_ref) & (MFC_TRACE_COUNT_MAX - 1);	\
			dev->mfc_trace[cnt].time = cpu_clock(cpu);					\
			snprintf(dev->mfc_trace[cnt].str, MFC_TRACE_STR_LEN,			\
					"[d:%d, c:%d] " fmt, ctx->dev->id, ctx->num, ##args);		\
		} while(0)

#endif /* __S5P_MFC_DEBUG_H */
