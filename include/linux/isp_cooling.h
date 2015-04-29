/*
 *  linux/include/linux/gpu_cooling.h
 *
 *  Copyright (C) 2012	Samsung Electronics Co., Ltd(http://www.samsung.com)
 *  Copyright (C) 2012  Amit Daniel <amit.kachhap@linaro.org>
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#ifndef __ISP_COOLING_H__
#define __ISP_COOLING_H__

#include <linux/of.h>
#include <linux/thermal.h>
#include <linux/cpumask.h>

#ifdef CONFIG_ISP_THERMAL

/**
 * isp_cooling_register - function to create isp cooling device.
 * @clip_gpus: cpumask of gpus where the frequency constraints will happen
 */
struct thermal_cooling_device *
isp_cooling_register(const struct cpumask *clip_gpus);

/**
 * of_isp_cooling_register - create isp cooling device based on DT.
 * @np: a valid struct device_node to the cooling device device tree node.
 * @clip_gpus: cpumask of gpus where the frequency constraints will happen
 */
#ifdef CONFIG_THERMAL_OF
struct thermal_cooling_device *
of_isp_cooling_register(struct device_node *np,
			    const struct cpumask *clip_gpus);
#else
static inline struct thermal_cooling_device *
of_isp_cooling_register(struct device_node *np,
			    const struct cpumask *clip_gpus)
{
	return NULL;
}
#endif

/**
 * isp_cooling_unregister - function to remove isp cooling device.
 * @cdev: thermal cooling device pointer.
 */
void isp_cooling_unregister(struct thermal_cooling_device *cdev);

unsigned long isp_cooling_get_fps(unsigned int isp, unsigned int fps);
#else /* !CONFIG_GPU_THERMAL */
static inline struct thermal_cooling_device *
isp_cooling_register(const struct cpumask *clip_gpus)
{
	return NULL;
}
static inline struct thermal_cooling_device *
of_isp_cooling_register(struct device_node *np,
			    const struct cpumask *clip_gpus)
{
	return NULL;
}
static inline
void isp_cooling_unregister(struct thermal_cooling_device *cdev)
{
	return;
}
static inline
unsigned long isp_cooling_get_fps(unsigned int isp, unsigned int fps)
{
	return THERMAL_CSTATE_INVALID;
}
#endif	/* CONFIG_ISP_THERMAL */

#endif /* __ISP_COOLING_H__ */
