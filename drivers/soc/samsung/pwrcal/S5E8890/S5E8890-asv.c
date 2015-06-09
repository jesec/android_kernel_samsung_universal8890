#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "../pwrcal-asv.h"
#include "S5E8890-cmusfr.h"
#include "S5E8890-cmu.h"
#include "S5E8890-vclk.h"
#include "S5E8890-vclk-internal.h"

#ifdef PWRCAL_TARGET_LINUX
#include <linux/io.h>
#include "../../mach/map.h"
#endif
#ifdef PWRCAL_TARGET_FW
#define S5P_VA_APM_SRAM			((void *)0x11200000)
#endif

#define EXYNOS_MAILBOX_RCC_MIN_MAX(x)	(S5P_VA_APM_SRAM + (0x3700) + (x * 0x4))
#define EXYNOS_MAILBOX_ATLAS_RCC(x)	(S5P_VA_APM_SRAM + (0x3730) + (x * 0x4))
#define EXYNOS_MAILBOX_APOLLO_RCC(x)	(S5P_VA_APM_SRAM + (0x3798) + (x * 0x4))
#define EXYNOS_MAILBOX_G3D_RCC(x)	(S5P_VA_APM_SRAM + (0x37EC) + (x * 0x4))
#define EXYNOS_MAILBOX_MIF_RCC(x)	(S5P_VA_APM_SRAM + (0x381C) + (x * 0x4))


enum dvfs_id {
	cal_asv_dvfs_big,
	cal_asv_dvfs_little,
	cal_asv_dvfs_g3d,
	cal_asv_dvfs_mif,
	cal_asv_dvfs_int,
	cal_asv_dvfs_cam,
	cal_asv_dvfs_disp,
	num_of_dvfs,
};

#define MAX_ASV_GROUP	16
#define NUM_OF_ASVTABLE	1
#define PWRCAL_ASV_LIST(table)	{table, sizeof(table) / sizeof(table[0])}

struct asv_table_entry {
	unsigned int index;
	unsigned int voltage[MAX_ASV_GROUP];
};

struct asv_table_list {
	const struct asv_table_entry *table;
	unsigned int table_size;
};

#define FORCE_ASV_MAGIC		0x57E9
static unsigned int force_asv_group[num_of_dvfs];


struct asv_tbl_info {
	unsigned mngs_asv_group:4;
	int mngs_modified_group:4;
	unsigned mngs_ssa10:2;
	unsigned mngs_ssa11:2;
	unsigned mngs_ssa0:4;
	unsigned apollo_asv_group:4;
	int apollo_modified_group:4;
	unsigned apollo_ssa10:2;
	unsigned apollo_ssa11:2;
	unsigned apollo_ssa0:4;

	unsigned g3d_asv_group:4;
	int g3d_modified_group:4;
	unsigned g3d_ssa10:2;
	unsigned g3d_ssa11:2;
	unsigned g3d_ssa0:4;
	unsigned mif_asv_group:4;
	int mif_modified_group:4;
	unsigned mif_ssa10:2;
	unsigned mif_ssa11:2;
	unsigned mif_ssa0:4;

	unsigned int_asv_group:4;
	int int_modified_group:4;
	unsigned int_ssa10:2;
	unsigned int_ssa11:2;
	unsigned int_ssa0:4;
	unsigned disp_asv_group:4;
	int disp_modified_group:4;
	unsigned disp_ssa10:2;
	unsigned disp_ssa11:2;
	unsigned disp_ssa0:4;

	unsigned asv_table_ver:7;
	unsigned fused_grp:1;
	unsigned reserved_0:24;
};
#define ASV_INFO_ADDR_BASE	(0x101E9000)
#define ASV_INFO_ADDR_CNT	(sizeof(struct asv_tbl_info) / 4)

static struct asv_tbl_info asv_tbl_info;

static struct asv_table_list pwrcal_big_asv_table[];
static struct asv_table_list pwrcal_little_asv_table[];
static struct asv_table_list pwrcal_g3d_asv_table[];
static struct asv_table_list pwrcal_mif_asv_table[];
static struct asv_table_list pwrcal_int_asv_table[];
static struct asv_table_list pwrcal_cam_asv_table[];
static struct asv_table_list pwrcal_disp_asv_table[];

static struct pwrcal_vclk_dfs *asv_dvfs_big;
static struct pwrcal_vclk_dfs *asv_dvfs_little;
static struct pwrcal_vclk_dfs *asv_dvfs_g3d;
static struct pwrcal_vclk_dfs *asv_dvfs_mif;
static struct pwrcal_vclk_dfs *asv_dvfs_int;
static struct pwrcal_vclk_dfs *asv_dvfs_cam;
static struct pwrcal_vclk_dfs *asv_dvfs_disp;


static void asv_set_grp(unsigned int id, unsigned int asvgrp)
{
	force_asv_group[id & 0x0000FFFF] = FORCE_ASV_MAGIC | asvgrp;
}

static void asv_set_tablever(unsigned int version)
{
	asv_tbl_info.asv_table_ver = version;

	return;
}

static void asv_get_asvinfo(void)
{
	int i;
	unsigned int *pasv_table;
	unsigned long tmp;

	pasv_table = (unsigned int *)&asv_tbl_info;
	for (i = 0; i < ASV_INFO_ADDR_CNT; i++) {
#ifdef PWRCAL_TARGET_LINUX
		exynos_smc_readsfr((unsigned long)(ASV_INFO_ADDR_BASE + 0x4 * i), &tmp);
#else
#if (CONFIG_STARTUP_EL_MODE == STARTUP_EL3)
		tmp = *((volatile unsigned int *)(unsigned long)(ASV_INFO_ADDR_BASE + 0x4 * i));
#else
		smc_readsfr((unsigned long)(ASV_INFO_ADDR_BASE + 0x4 * i), &tmp);
#endif
#endif
		*(pasv_table + i) = (unsigned int)tmp;
	}

	if (asv_tbl_info.asv_table_ver > 0)
		asv_tbl_info.asv_table_ver = 0;

	if (!asv_tbl_info.fused_grp) {
		asv_tbl_info.mngs_asv_group = 0;
		asv_tbl_info.mngs_modified_group = 0;
		asv_tbl_info.mngs_ssa10 = 0;
		asv_tbl_info.mngs_ssa11 = 0;
		asv_tbl_info.mngs_ssa0 = 0;
		asv_tbl_info.apollo_asv_group = 0;
		asv_tbl_info.apollo_modified_group = 0;
		asv_tbl_info.apollo_ssa10 = 0;
		asv_tbl_info.apollo_ssa11 = 0;
		asv_tbl_info.apollo_ssa0 = 0;

		asv_tbl_info.g3d_asv_group = 0;
		asv_tbl_info.g3d_modified_group = 0;
		asv_tbl_info.g3d_ssa10 = 0;
		asv_tbl_info.g3d_ssa11 = 0;
		asv_tbl_info.g3d_ssa0 = 0;
		asv_tbl_info.mif_asv_group = 0;
		asv_tbl_info.mif_modified_group = 0;
		asv_tbl_info.mif_ssa10 = 0;
		asv_tbl_info.mif_ssa11 = 0;
		asv_tbl_info.mif_ssa0 = 0;

		asv_tbl_info.int_asv_group = 0;
		asv_tbl_info.int_modified_group = 0;
		asv_tbl_info.int_ssa10 = 0;
		asv_tbl_info.int_ssa11 = 0;
		asv_tbl_info.int_ssa0 = 0;
		asv_tbl_info.disp_asv_group = 0;
		asv_tbl_info.disp_modified_group = 0;
		asv_tbl_info.disp_ssa10 = 0;
		asv_tbl_info.disp_ssa11 = 0;
		asv_tbl_info.disp_ssa0 = 0;

		asv_tbl_info.asv_table_ver = 0;
		asv_tbl_info.fused_grp = 0;
		asv_tbl_info.reserved_0 = 0;
	}
}

static unsigned int get_asv_voltage(enum dvfs_id domain, unsigned int lv)
{
	int asv;
	int mod;
	unsigned int ssa10, ssa11;
	unsigned int ssa0;
	unsigned int subgrp_index;
	const unsigned int *table;
	unsigned int volt;

	switch (domain) {
	case cal_asv_dvfs_big:
		asv = asv_tbl_info.mngs_asv_group;
		mod = asv_tbl_info.mngs_modified_group;
		ssa10 = asv_tbl_info.mngs_ssa10;
		ssa11 = asv_tbl_info.mngs_ssa11;
		ssa0 = asv_tbl_info.mngs_ssa0;
		subgrp_index = 16;
		table = pwrcal_big_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_little:
		asv = asv_tbl_info.apollo_asv_group;
		mod = asv_tbl_info.apollo_modified_group;
		ssa10 = asv_tbl_info.apollo_ssa10;
		ssa11 = asv_tbl_info.apollo_ssa11;
		ssa0 = asv_tbl_info.apollo_ssa0;
		subgrp_index = 10;
		table = pwrcal_little_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_g3d:
		asv = asv_tbl_info.g3d_asv_group;
		mod = asv_tbl_info.g3d_modified_group;
		ssa10 = asv_tbl_info.g3d_ssa10;
		ssa11 = asv_tbl_info.g3d_ssa11;
		ssa0 = asv_tbl_info.g3d_ssa0;
		subgrp_index = 7;
		table = pwrcal_g3d_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_mif:
		asv = asv_tbl_info.mif_asv_group;
		mod = asv_tbl_info.mif_modified_group;
		ssa10 = asv_tbl_info.mif_ssa10;
		ssa11 = asv_tbl_info.mif_ssa11;
		ssa0 = asv_tbl_info.mif_ssa0;
		subgrp_index = 8;
		table = pwrcal_mif_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_int:
		asv = asv_tbl_info.int_asv_group;
		mod = asv_tbl_info.int_modified_group;
		ssa10 = asv_tbl_info.int_ssa10;
		ssa11 = asv_tbl_info.int_ssa11;
		ssa0 = asv_tbl_info.int_ssa0;
		subgrp_index = 15;
		table = pwrcal_int_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_cam:
		asv = asv_tbl_info.disp_asv_group;
		mod = asv_tbl_info.disp_modified_group;
		ssa10 = asv_tbl_info.disp_ssa10;
		ssa11 = asv_tbl_info.disp_ssa11;
		ssa0 = asv_tbl_info.disp_ssa0;
		subgrp_index = 5;
		table = pwrcal_cam_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	case cal_asv_dvfs_disp:
		asv = asv_tbl_info.disp_asv_group;
		mod = asv_tbl_info.disp_modified_group;
		ssa10 = asv_tbl_info.disp_ssa10;
		ssa11 = asv_tbl_info.disp_ssa11;
		ssa0 = asv_tbl_info.disp_ssa0;
		subgrp_index = 2;
		table = pwrcal_disp_asv_table[asv_tbl_info.asv_table_ver].table[lv].voltage;
		break;
	default:
		asm("b .");	/* Never reach */
		break;
	}

	if ((force_asv_group[domain & 0x0000FFFF] & 0xFFFF0000) != FORCE_ASV_MAGIC) {
		if (mod)
			asv += mod;
	} else {
		asv = force_asv_group[domain & 0x0000FFFF] & 0x0000FFFF;
	}

	if (asv < 0 || asv >= MAX_ASV_GROUP)
		asm("b .");	/* Never reach */

	volt = table[asv];
	if (lv < subgrp_index)
		volt += 12500 * ssa10;
	else
		volt += 12500 * ssa11;

	if (volt < 575000 + ssa0 * 25000)
		volt = 575000 + ssa0 * 25000;

	return volt;
}

static int dvfsbig_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_big->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_big, lv);

	return max_lv;
}

static int dvfslittle_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_little->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_little, lv);

	return max_lv;
}

static int dvfsg3d_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_g3d->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_g3d, lv);

	return max_lv;
}

static int dvfsmif_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_mif->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_mif, lv);

	return max_lv;
}

static int dvfsint_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_int->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_int, lv);

	return max_lv;
}

static int dvfscam_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_cam->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_cam, lv);

	return max_lv;
}

static int dvfsdisp_get_asv_table(unsigned int *table)
{
	int lv, max_lv;

	max_lv = asv_dvfs_disp->table->num_of_lv;

	for (lv = 0; lv < max_lv; lv++)
		table[lv] = get_asv_voltage(cal_asv_dvfs_disp, lv);

	return max_lv;
}



static int asv_rcc_set_table(void)
{
	return 0;
}

static int asv_init(void)
{
	struct vclk *vclk;

	asv_get_asvinfo();

	vclk = cal_get_vclk(dvfs_big);
	asv_dvfs_big = to_dfs(vclk);
	asv_dvfs_big->dfsops->get_asv_table = dvfsbig_get_asv_table;
/*	asv_dvfs_big->dfsops->set_ema = dvfsbig_set_ema;
*/
	vclk = cal_get_vclk(dvfs_little);
	asv_dvfs_little = to_dfs(vclk);
	asv_dvfs_little->dfsops->get_asv_table = dvfslittle_get_asv_table;
/*	asv_dvfs_little->dfsops->set_ema = dvfslittle_set_ema;
*/
	vclk = cal_get_vclk(dvfs_g3d);
	asv_dvfs_g3d = to_dfs(vclk);
	asv_dvfs_g3d->dfsops->get_asv_table = dvfsg3d_get_asv_table;
/*	asv_dvfs_g3d->dfsops->set_ema = dvfsg3d_set_ema;
*/
	vclk = cal_get_vclk(dvfs_mif);
	asv_dvfs_mif = to_dfs(vclk);
	asv_dvfs_mif->dfsops->get_asv_table = dvfsmif_get_asv_table;

	vclk = cal_get_vclk(dvfs_int);
	asv_dvfs_int = to_dfs(vclk);
	asv_dvfs_int->dfsops->get_asv_table = dvfsint_get_asv_table;
/*	asv_dvfs_int->dfsops->set_ema = dvfsint_set_ema;
*/
	vclk = cal_get_vclk(dvfs_cam);
	asv_dvfs_cam = to_dfs(vclk);
	asv_dvfs_cam->dfsops->get_asv_table = dvfscam_get_asv_table;

	vclk = cal_get_vclk(dvfs_disp);
	asv_dvfs_disp = to_dfs(vclk);
	asv_dvfs_disp->dfsops->get_asv_table = dvfsdisp_get_asv_table;

	return 0;
}

static void asv_print_info(void)
{
}

struct cal_asv_ops cal_asv_ops = {
	.print_info = asv_print_info,
	.set_grp = asv_set_grp,
	.set_tablever = asv_set_tablever,
	.set_rcc_table = asv_rcc_set_table,
	.asv_init = asv_init,
};

static const struct asv_table_entry pwrcal_volt_table_big_asv_v0[] = {
	{	3000,	{	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	} },
	{	2900,	{	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	} },
	{	2800,	{	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	} },
	{	2700,	{	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	} },
	{	2600,	{	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	} },
	{	2496,	{	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	} },
	{	2392,	{	1268750,	1256250,	1243750,	1231250,	1218750,	1206250,	1193750,	1181250,	1168750,	1156250,	1143750,	1131250,	1118750,	1106250,	1093750,	1081250,	} },
	{	2288,	{	1187500,	1175000,	1162500,	1150000,	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	} },
	{	2184,	{	1131250,	1118750,	1106250,	1093750,	1081250,	1068750,	1056250,	1043750,	1031250,	1018750,	1006250,	993750,	981250,	968750,	956250,	943750,	} },
	{	2080,	{	1081250,	1068750,	1056250,	1043750,	1031250,	1018750,	1006250,	993750,	981250,	968750,	956250,	943750,	931250,	918750,	906250,	893750,	} },
	{	1976,	{	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	900000,	887500,	875000,	862500,	850000,	} },
	{	1872,	{	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	900000,	887500,	875000,	862500,	850000,	837500,	825000,	812500,	} },
	{	1768,	{	962500,	950000,	937500,	925000,	912500,	900000,	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	} },
	{	1664,	{	931250,	918750,	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	} },
	{	1560,	{	900000,	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	} },
	{	1456,	{	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	} },
	{	1352,	{	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	} },
	{	1248,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	1144,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	1040,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	936,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	832,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	728,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	624,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	520,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	416,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	312,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	208,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
};

static const struct asv_table_entry pwrcal_volt_table_little_asv_v0[] = {
	{	1976,	{	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	} },
	{	1898,	{	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	} },
	{	1794,	{	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	} },
	{	1690,	{	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	} },
	{	1586,	{	1162500,	1150000,	1137500,	1125000,	1112500,	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	} },
	{	1482,	{	1100000,	1087500,	1075000,	1062500,	1050000,	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	} },
	{	1378,	{	1037500,	1025000,	1012500,	1000000,	987500,	975000,	962500,	950000,	937500,	925000,	912500,	900000,	887500,	875000,	862500,	850000,	} },
	{	1274,	{	987500,	975000,	962500,	950000,	937500,	925000,	912500,	900000,	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	} },
	{	1170,	{	937500,	925000,	912500,	900000,	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	} },
	{	1066,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	962,	{	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	} },
	{	858,	{	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	637500,	625000,	} },
	{	754,	{	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	} },
	{	650,	{	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	637500,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	546,	{	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	442,	{	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	338,	{	650000,	637500,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	234,	{	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	130,	{	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
};

static const struct asv_table_entry pwrcal_volt_table_g3d_asv_v0[] = {
	{	1000,	{	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	} },
	{	900,	{	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	} },
	{	800,	{	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	} },
	{	780,	{	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	} },
	{	702,	{	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	} },
	{	598,	{	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	} },
	{	546,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
	{	416,	{	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	351,	{	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	273,	{	725000,	712500,	700000,	687500,	675000,	662500,	650000,	637500,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	169,	{	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	104,	{	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
};

static const struct asv_table_entry pwrcal_volt_table_mif_asv_v0[] = {
	{	1846,	{	931250,	918750,	906250,	893750,	881250,	868750,	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	} },
	{	1742,	{	912500,	900000,	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	} },
	{	1560,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	1482,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	1352,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	1222,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	1092,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	962,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	832,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	676,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	572,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	416,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
	{	286,	{	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	687500,	675000,	} },
};

static const struct asv_table_entry pwrcal_volt_table_int_asv_v0[] = {
	{	690,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	680,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	670,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	660,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	650,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	640,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	630,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	620,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	610,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	600,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	511,	{	887500,	875000,	862500,	850000,	837500,	825000,	812500,	800000,	787500,	775000,	762500,	750000,	737500,	725000,	712500,	700000,	} },
	{	468,	{	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	} },
	{	400,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
	{	336,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
	{	255,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
	{	200,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
	{	168,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
	{	127,	{	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	656250,	643750,	631250,	625000,	} },
};

static const struct asv_table_entry pwrcal_volt_table_disp_asv_v0[] = {
	{	528,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	400,	{	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	637500,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	336,	{	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	637500,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
	{	168,	{	737500,	725000,	712500,	700000,	687500,	675000,	662500,	650000,	637500,	625000,	625000,	625000,	625000,	625000,	625000,	625000,	} },
};

static const struct asv_table_entry pwrcal_volt_table_cam_asv_v0[] = {
	{	690,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	680,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	670,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	660,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	650,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	640,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	630,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	620,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	610,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
	{	600,	{	856250,	843750,	831250,	818750,	806250,	793750,	781250,	768750,	756250,	743750,	731250,	718750,	706250,	693750,	681250,	668750,	} },
};



static struct asv_table_list pwrcal_big_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_big_asv_v0),
};

static struct asv_table_list pwrcal_little_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_little_asv_v0),
};

static struct asv_table_list pwrcal_g3d_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_g3d_asv_v0),
};

static struct asv_table_list pwrcal_mif_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_mif_asv_v0),
};

static struct asv_table_list pwrcal_int_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_int_asv_v0),
};

static struct asv_table_list pwrcal_cam_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_cam_asv_v0),
};

static struct asv_table_list pwrcal_disp_asv_table[] = {
	PWRCAL_ASV_LIST(pwrcal_volt_table_disp_asv_v0),
};
