/* include/soc/samsung/apm-exynos.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *               http://www.samsung.com
 *
 * AP Parameter definitions
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __AP_PARAM_PARSER_H
#define __AP_PARAM_PARSER_H __FILE__

#include <asm/io.h>

#include <linux/slab.h>
#include <linux/device.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/module.h>
#include <linux/stat.h>

#define BLOCK_AP_THERMAL	"THERMAL"
#define BLOCK_MIF_THERMAL	"MR4"
#define BLOCK_DVFS		"DVFS"
#define BLOCK_ASV		"ASV"
#define BLOCK_TIMING_PARAM	"TIMING"
#define BLOCK_RCC		"RCC"
#define BLOCK_PLL		"PLL"
#define BLOCK_MARGIN		"MARGIN"
#define BLOCK_MINLOCK		"MINLOCK"
#define BLOCK_GEN_PARAM		"GEN"

struct ap_param_header
{
	char sign[4];
	char version[4];
	unsigned int total_size;
	int num_of_header;
};

struct ap_param_dvfs_level
{
	unsigned int level;
	int level_en;
};

struct ap_param_dvfs_domain
{
	char *domain_name;
	unsigned int domain_offset;

	unsigned int max_frequency;
	unsigned int min_frequency;
	int num_of_clock;
	int num_of_level;
	char **list_clock;
	struct ap_param_dvfs_level  *list_level;
	unsigned int *list_dvfs_value;
};

struct ap_param_dvfs_header
{
	int parser_version;
	char version[4];
	int num_of_domain;

	struct ap_param_dvfs_domain *domain_list;
};

struct ap_param_pll_frequency
{
	unsigned int frequency;
	unsigned int p;
	unsigned int m;
	unsigned int s;
	unsigned int k;
};

struct ap_param_pll
{
	char *pll_name;
	unsigned int pll_offset;

	unsigned int type_pll;
	int num_of_frequency;
	struct ap_param_pll_frequency *frequency_list;
};

struct ap_param_pll_header
{
	int parser_version;
	char version[4];
	int num_of_pll;

	struct ap_param_pll *pll_list;
};

struct ap_param_voltage_table
{
	int table_version;
	unsigned int *voltages;
};

struct ap_param_voltage_domain
{
	char *domain_name;
	unsigned int domain_offset;

	int num_of_group;
	int num_of_level;
	int num_of_table;
	unsigned int *level_list;
	struct ap_param_voltage_table *table_list;
};

struct ap_param_voltage_header
{
	int parser_version;
	char version[4];
	int num_of_domain;

	struct ap_param_voltage_domain *domain_list;
};

struct ap_param_rcc_table
{
	unsigned int *rcc;
	int table_version;
};

struct ap_param_rcc_domain
{
	char *domain_name;
	unsigned int domain_offset;

	int num_of_group;
	int num_of_level;
	int num_of_table;
	unsigned int *level_list;
	struct ap_param_rcc_table *table_list;
};

struct ap_param_rcc_header
{
	int parser_version;
	char version[4];
	int num_of_domain;

	struct ap_param_rcc_domain *domain_list;
};

struct ap_param_mif_thermal_level
{
	int mr4_level;
	unsigned int max_frequency;
	unsigned int min_frequency;
	unsigned int refresh_rate_value;
	unsigned int polling_period;
	unsigned int sw_trip;
};

struct ap_param_mif_thermal_header
{
	int parser_version;
	char version[4];
	int num_of_level;

	struct ap_param_mif_thermal_level *level;
};

struct ap_param_ap_thermal_range
{
	unsigned int lower_bound_temperature;
	unsigned int upper_bound_temperature;
	unsigned int max_frequency;
	unsigned int sw_trip;
	unsigned int flag;
};

struct ap_param_ap_thermal_function
{
	char *function_name;
	unsigned int function_offset;

	int num_of_range;
	struct ap_param_ap_thermal_range *range_list;
};

struct ap_param_ap_thermal_header
{
	int parser_version;
	char version[4];
	int num_of_function;

	struct ap_param_ap_thermal_function *function_list;
};

struct ap_param_margin_domain
{
	char *domain_name;
	unsigned int domain_offset;

	int num_of_group;
	int num_of_level;
	unsigned int *offset;
};

struct ap_param_margin_header
{
	int parser_version;
	char version[4];
	int num_of_domain;

	struct ap_param_margin_domain *domain_list;
};

struct ap_param_timing_param_size
{
	unsigned int memory_size;
	unsigned int offset
		;
	int num_of_timing_param;
	int num_of_level;
	unsigned int *timing_parameter;
};

struct ap_param_timing_param_header
{
	int parser_version;
	char version[4];
	int num_of_size;

	struct ap_param_timing_param_size *size_list;
};

struct ap_param_minlock_frequency
{
	unsigned int main_frequencies;
	unsigned int sub_frequencies;
};

struct ap_param_minlock_domain
{
	char *domain_name;
	unsigned int domain_offset;

	int num_of_level;
	struct ap_param_minlock_frequency *level;
};

struct ap_param_minlock_header
{
	int parser_version;
	char version[4];
	int num_of_domain;

	struct ap_param_minlock_domain *domain_list;
};

struct ap_param_gen_param_table
{
	char *table_name;
	unsigned int offset;

	int num_of_col;
	int num_of_row;
	unsigned int *parameter;
};

struct ap_param_gen_param_header
{
	int parser_version;
	char version[4];
	int num_of_table;

	struct ap_param_gen_param_table *table_list;
};

struct ap_param_info
{
	char *block_name;
	int block_name_length;
	int (*parser)(void *address, struct ap_param_info *info);
	struct class_attribute sysfs_attr;
	void *block_handle;
};

#if defined(CONFIG_AP_PARAM)
void ap_param_init(phys_addr_t address, phys_addr_t size);
int ap_param_parse_binary_header(void);
void* ap_param_get_block(char *block_name);
struct ap_param_dvfs_domain *ap_param_dvfs_get_domain(void *block, char *domain_name);
struct ap_param_pll *ap_param_pll_get_pll(void *block, char *pll_name);
struct ap_param_voltage_domain *ap_param_asv_get_domain(void *block, char *domain_name);
struct ap_param_rcc_domain *ap_param_rcc_get_domain(void *block, char *domain_name);
struct ap_param_mif_thermal_level *ap_param_mif_thermal_get_level(void *block, int mr4_level);
struct ap_param_ap_thermal_function *ap_param_ap_thermal_get_function(void *block, char *function_name);
struct ap_param_margin_domain *ap_param_margin_get_domain(void *block, char *domain_name);
struct ap_param_timing_param_size *ap_param_timing_param_get_size(void *block, int size);
struct ap_param_minlock_domain *ap_param_minlock_get_domain(void *block, char *domain_name);
struct ap_param_gen_param_table *ap_param_gen_param_get_table(void *block, char *table_name);

void ap_param_init_map_io(void);

int ap_param_strcmp(char *src1, char *src2);

#else

static inline void ap_param_init(phys_addr_t address, phys_addr_t size) {}
static inline int ap_param_parse_binary_header(void) { return 0; }
static inline void* ap_param_get_block(char *block_name) { return NULL; }
static inline struct ap_param_dvfs_domain *ap_param_dvfs_get_domain(void *block, char *domain_name) { return NULL; }
static inline struct ap_param_pll *ap_param_pll_get_pll(void *block, char *pll_name) { return NULL; }
static inline struct ap_param_voltage_domain *ap_param_asv_get_domain(void *block, char *domain_name) { return NULL; }
static inline struct ap_param_rcc_domain *ap_param_rcc_get_domain(void *block, char *domain_name) { return NULL; }
static inline struct ap_param_mif_thermal_level *ap_param_mif_thermal_get_level(void *block, int mr4_level) { return NULL; }
static inline struct ap_param_ap_thermal_function *ap_param_ap_thermal_get_function(void *block, char *function_name) { return NULL; }
static inline struct ap_param_margin_domain *ap_param_margin_get_domain(void *block, char *domain_name) { return NULL; }
static inline struct ap_param_timing_param_size *ap_param_timing_param_get_size(void *block, int size) { return NULL; }
static inline struct ap_param_minlock_domain *ap_param_minlock_get_domain(void *block, char *domain_name) { return NULL; }
static inline struct ap_param_gen_param_table *ap_param_gen_param_get_table(void *block, char *table_name) { return NULL; }

static inline void ap_param_init_map_io(void) {}

static inline int ap_param_strcmp(char *src1, char *src2) { return -1; }

#endif

#endif
