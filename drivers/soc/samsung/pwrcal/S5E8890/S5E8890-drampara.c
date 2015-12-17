#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "S5E8890-sfrbase.h"
#include "S5E8890-vclk-internal.h"

#ifdef PWRCAL_TARGET_LINUX
#include <soc/samsung/ect_parser.h>
#else
#include <mach/ect_parser.h>
#endif

#ifndef MHZ
#define MHZ		((unsigned long long)1000000)
#endif

#define NUM_OF_CH				(4)

#define __SMC_ALL				(DMC_MISC_CCORE_BASE + 0x8000)
#define __PHY_ALL				(DMC_MISC_CCORE_BASE + 0x4000)
#define __DMC_MISC_ALL			(DMC_MISC_CCORE_BASE + 0x0000)

#define ModeRegAddr				((void *)(__SMC_ALL + 0x0000))
#define MprMrCtl				((void *)(__SMC_ALL + 0x0004))
#define ModeRegWrData			((void *)(__SMC_ALL + 0x0008))

#define DramTiming0_0			((void *)(__SMC_ALL + 0x0058))
#define DramTiming1_0			((void *)(__SMC_ALL + 0x005C))
#define DramTiming2_0			((void *)(__SMC_ALL + 0x0060))
#define DramTiming3_0			((void *)(__SMC_ALL + 0x0064))
#define DramTiming4_0			((void *)(__SMC_ALL + 0x0068))
#define DramTiming5_0			((void *)(__SMC_ALL + 0x006C))
#define DramTiming6_0			((void *)(__SMC_ALL + 0x0070))
#define DramTiming7_0			((void *)(__SMC_ALL + 0x0074))
#define DramTiming8_0			((void *)(__SMC_ALL + 0x0078))
#define DramTiming9_0			((void *)(__SMC_ALL + 0x007C))
#define DramDerateTiming0_0		((void *)(__SMC_ALL + 0x0088))
#define DramDerateTiming1_0		((void *)(__SMC_ALL + 0x008C))
#define Dimm0AutoRefTiming1_0	((void *)(__SMC_ALL + 0x009C))
#define Dimm1AutoRefTiming1_0	((void *)(__SMC_ALL + 0x00A4))
#define AutoRefTiming2_0		((void *)(__SMC_ALL + 0x00A8))
#define PwrMgmtTiming0_0		((void *)(__SMC_ALL + 0x00B8))
#define PwrMgmtTiming1_0		((void *)(__SMC_ALL + 0x00BC))
#define PwrMgmtTiming2_0		((void *)(__SMC_ALL + 0x00C0))
#define PwrMgmtTiming3_0		((void *)(__SMC_ALL + 0x00C4))
#define TmrTrnInterval_0		((void *)(__SMC_ALL + 0x00C8))
#define DvfsTrnCtl_0			((void *)(__SMC_ALL + 0x00CC))
#define TrnTiming0_0			((void *)(__SMC_ALL + 0x00D0))
#define TrnTiming1_0			((void *)(__SMC_ALL + 0x00D4))
#define TrnTiming2_0			((void *)(__SMC_ALL + 0x00D8))
#define DFIDelay1_0				((void *)(__SMC_ALL + 0x00E0))
#define DFIDelay2_0				((void *)(__SMC_ALL + 0x00E4))

#define DramTiming0_1			((void *)(__SMC_ALL + 0x0108))
#define DramTiming1_1			((void *)(__SMC_ALL + 0x010C))
#define DramTiming2_1			((void *)(__SMC_ALL + 0x0110))
#define DramTiming3_1			((void *)(__SMC_ALL + 0x0114))
#define DramTiming4_1			((void *)(__SMC_ALL + 0x0118))
#define DramTiming5_1			((void *)(__SMC_ALL + 0x011C))
#define DramTiming6_1			((void *)(__SMC_ALL + 0x0120))
#define DramTiming7_1			((void *)(__SMC_ALL + 0x0124))
#define DramTiming8_1			((void *)(__SMC_ALL + 0x0128))
#define DramTiming9_1			((void *)(__SMC_ALL + 0x012C))
#define DramDerateTiming0_1		((void *)(__SMC_ALL + 0x0138))
#define DramDerateTiming1_1		((void *)(__SMC_ALL + 0x013C))
#define Dimm0AutoRefTiming1_1	((void *)(__SMC_ALL + 0x014C))
#define Dimm1AutoRefTiming1_1	((void *)(__SMC_ALL + 0x0154))
#define AutoRefTiming2_1		((void *)(__SMC_ALL + 0x0158))
#define PwrMgmtTiming0_1		((void *)(__SMC_ALL + 0x0168))
#define PwrMgmtTiming1_1		((void *)(__SMC_ALL + 0x016C))
#define PwrMgmtTiming2_1		((void *)(__SMC_ALL + 0x0170))
#define PwrMgmtTiming3_1		((void *)(__SMC_ALL + 0x0174))
#define TmrTrnInterval_1		((void *)(__SMC_ALL + 0x0178))
#define DvfsTrnCtl_1			((void *)(__SMC_ALL + 0x017C))
#define TrnTiming0_1			((void *)(__SMC_ALL + 0x0180))
#define TrnTiming1_1			((void *)(__SMC_ALL + 0x0184))
#define TrnTiming2_1			((void *)(__SMC_ALL + 0x0188))
#define DFIDelay1_1				((void *)(__SMC_ALL + 0x0190))
#define DFIDelay2_1				((void *)(__SMC_ALL + 0x0194))

#define SMC_TmrTrnCtl			((void *)(__SMC_ALL + 0x01D0))
#define SMC_TmrTrnCtl_CH0		((void *)(SMC0_BASE + 0x01D0))
#define SMC_TmrTrnCtl_CH1		((void *)(SMC1_BASE + 0x01D0))
#define SMC_TmrTrnCtl_CH2		((void *)(SMC2_BASE + 0x01D0))
#define SMC_TmrTrnCtl_CH3		((void *)(SMC3_BASE + 0x01D0))
#define SMC_TrnStatus_CH0		((void *)(SMC0_BASE + 0x01D8))
#define SMC_TrnStatus_CH1		((void *)(SMC1_BASE + 0x01D8))
#define SMC_TrnStatus_CH2		((void *)(SMC2_BASE + 0x01D8))
#define SMC_TrnStatus_CH3		((void *)(SMC3_BASE + 0x01D8))
#define SMC_PwrMgmtMode_CH0		((void *)(SMC0_BASE + 0x0238))
#define SMC_PwrMgmtMode_CH1		((void *)(SMC1_BASE + 0x0238))
#define SMC_PwrMgmtMode_CH2		((void *)(SMC2_BASE + 0x0238))
#define SMC_PwrMgmtMode_CH3		((void *)(SMC3_BASE + 0x0238))

#define PHY_DVFS_CON			((void *)(__PHY_ALL + 0x00B8))
#define PHY_DVFS_CON_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00B8))
#define PHY_DVFS0_CON0_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00BC))
#define PHY_DVFS0_CON1_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00C4))
#define PHY_DVFS0_CON2_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00CC))
#define PHY_DVFS0_CON3_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00D4))
#define PHY_DVFS0_CON4_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00DC))

#define PHY_DVFS_CON_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00B8))
#define PHY_DVFS0_CON0_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00BC))
#define PHY_DVFS0_CON1_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00C4))
#define PHY_DVFS0_CON2_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00CC))
#define PHY_DVFS0_CON3_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00D4))
#define PHY_DVFS0_CON4_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00DC))

#define PHY_DVFS_CON_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00B8))
#define PHY_DVFS0_CON0_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00BC))
#define PHY_DVFS0_CON1_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00C4))
#define PHY_DVFS0_CON2_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00CC))
#define PHY_DVFS0_CON3_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00D4))
#define PHY_DVFS0_CON4_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00DC))

#define PHY_DVFS_CON_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00B8))
#define PHY_DVFS0_CON0_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00BC))
#define PHY_DVFS0_CON1_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00C4))
#define PHY_DVFS0_CON2_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00CC))
#define PHY_DVFS0_CON3_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00D4))
#define PHY_DVFS0_CON4_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00DC))

#define PHY_DVFS1_CON0_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00C0))
#define PHY_DVFS1_CON1_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00C8))
#define PHY_DVFS1_CON2_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00D0))
#define PHY_DVFS1_CON3_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00D8))
#define PHY_DVFS1_CON4_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00E0))

#define PHY_DVFS1_CON0_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00C0))
#define PHY_DVFS1_CON1_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00C8))
#define PHY_DVFS1_CON2_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00D0))
#define PHY_DVFS1_CON3_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00D8))
#define PHY_DVFS1_CON4_CH1		((void *)(LPDDR4_PHY1_BASE + 0x00E0))

#define PHY_DVFS1_CON0_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00C0))
#define PHY_DVFS1_CON1_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00C8))
#define PHY_DVFS1_CON2_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00D0))
#define PHY_DVFS1_CON3_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00D8))
#define PHY_DVFS1_CON4_CH2		((void *)(LPDDR4_PHY2_BASE + 0x00E0))

#define PHY_DVFS1_CON0_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00C0))
#define PHY_DVFS1_CON1_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00C8))
#define PHY_DVFS1_CON2_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00D0))
#define PHY_DVFS1_CON3_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00D8))
#define PHY_DVFS1_CON4_CH3		((void *)(LPDDR4_PHY3_BASE + 0x00E0))

#define PHY_ZQ_CON0_CH0			((void *)(LPDDR4_PHY0_BASE + 0x03C8))
#define PHY_ZQ_CON0_CH1			((void *)(LPDDR4_PHY1_BASE + 0x03C8))
#define PHY_ZQ_CON0_CH2			((void *)(LPDDR4_PHY2_BASE + 0x03C8))
#define PHY_ZQ_CON0_CH3			((void *)(LPDDR4_PHY3_BASE + 0x03C8))

#define DMC_MISC_CON0			((void *)(__DMC_MISC_ALL + 0x0014))
#define DMC_MISC_CON1			((void *)(__DMC_MISC_ALL + 0x003C))
#define DMC_MISC_CON1_CH0		((void *)(DMC_MISC0_BASE + 0x003C))
#define DMC_MISC_CON1_CH1		((void *)(DMC_MISC1_BASE + 0x003C))
#define DMC_MISC_CON1_CH2		((void *)(DMC_MISC2_BASE + 0x003C))
#define DMC_MISC_CON1_CH3		((void *)(DMC_MISC3_BASE + 0x003C))
#define MRS_DATA1				((void *)(__DMC_MISC_ALL + 0x0054))
#define MRS_DATA1_CH0			((void *)(DMC_MISC0_BASE + 0x0054))
#define MRS_DATA1_CH1			((void *)(DMC_MISC1_BASE + 0x0054))
#define MRS_DATA1_CH2			((void *)(DMC_MISC2_BASE + 0x0054))
#define MRS_DATA1_CH3			((void *)(DMC_MISC3_BASE + 0x0054))

#define CG_CTRL_VAL_DDRPHY0		((void *)(CMU_MIF0_BASE + 0x0A08))
#define CG_CTRL_VAL_DDRPHY1		((void *)(CMU_MIF1_BASE + 0x0A08))
#define CG_CTRL_VAL_DDRPHY2		((void *)(CMU_MIF2_BASE + 0x0A08))
#define CG_CTRL_VAL_DDRPHY3		((void *)(CMU_MIF3_BASE + 0x0A08))

#define CG_CTRL_MAN_DDRPHY0		((void *)(CMU_MIF0_BASE + 0x1A08))
#define CG_CTRL_MAN_DDRPHY1		((void *)(CMU_MIF1_BASE + 0x1A08))
#define CG_CTRL_MAN_DDRPHY2		((void *)(CMU_MIF2_BASE + 0x1A08))
#define CG_CTRL_MAN_DDRPHY3		((void *)(CMU_MIF3_BASE + 0x1A08))

#define PMU_DREX_CALIBRATION1	((void *)(PMU_ALIVE_BASE + 0x09a4))
#define PMU_DREX_CALIBRATION2	((void *)(PMU_ALIVE_BASE + 0x09a8))
#define PMU_DREX_CALIBRATION3	((void *)(PMU_ALIVE_BASE + 0x09ac))

#define PHY_CH_ALL							(4)
#define VREF_BUFFER							(20)

enum mif_timing_set_idx {
	MIF_TIMING_SET_0,
	MIF_TIMING_SET_1
};

enum smc_dram_mode_register {
	DRAM_MR0,
	DRAM_MR1,
	DRAM_MR2,
	DRAM_MR3,
	DRAM_MR4,
	DRAM_MR5,
	DRAM_MR6,
	DRAM_MR7,
	DRAM_MR8,
	DRAM_MR9,
	DRAM_MR10,
	DRAM_MR11,
	DRAM_MR12,
	DRAM_MR13,
	DRAM_MR14,
	DRAM_MR15,
	DRAM_MR16,
	DRAM_MR17,
	DRAM_MR18,
	DRAM_MR19,
	DRAM_MR20,
	DRAM_MR21,
	DRAM_MR22,
	DRAM_MR23,
	DRAM_MR24,
	DRAM_MR25,
	DRAM_MR32 = 32,
	DRAM_MR40 = 40,
};

enum timing_parameter_column {
	DramTiming0,
	DramTiming1,
	DramTiming2,
	DramTiming3,
	DramTiming4,
	DramTiming5,
	DramTiming6,
	DramTiming7,
	DramTiming8,
	DramTiming9,
	DramDerateTiming0,
	DramDerateTiming1,
	Dimm0AutoRefTiming1,
	Dimm1AutoRefTiming1,
	AutoRefTiming2,
	PwrMgmtTiming0,
	PwrMgmtTiming1,
	PwrMgmtTiming2,
	PwrMgmtTiming3,
	TmrTrnInterval,
	DFIDelay1,
	DFIDelay2,
	DvfsTrnCtl,
	TrnTiming0,
	TrnTiming1,
	TrnTiming2,
	DVFSn_CON0,
	DVFSn_CON1,
	DVFSn_CON2,
	DVFSn_CON3,
	DVFSn_CON4,
	DirectCmd_MR1,
	DirectCmd_MR2,
	DirectCmd_MR3,
	DirectCmd_MR11,
	DirectCmd_MR12,
	DirectCmd_MR14,
	DirectCmd_MR22,

	num_of_g_smc_dfs_table_column = TrnTiming2 - DramTiming0 + 1,
	num_of_g_phy_dfs_table_column = DVFSn_CON4 - DVFSn_CON0 + 1,
	num_of_g_dram_dfs_table_column = DirectCmd_MR22 - DirectCmd_MR1 + 1,
	num_of_dram_parameter = num_of_g_smc_dfs_table_column + num_of_g_phy_dfs_table_column + num_of_g_dram_dfs_table_column,
};

typedef volatile unsigned int rw_bf_t;
typedef const unsigned int ro_bf_t;
typedef const unsigned int wo_bf_t;

typedef union {
	volatile unsigned int data;
	struct {
rw_bf_t dvfs0_zq_ds0_pdds         :
		(2 - 0 + 1);
rw_bf_t dvfs0_zq_ds0_dds          :
		(5 - 3 + 1);
rw_bf_t dvfs0_zq_ds1_pdds         :
		(8 - 6 + 1);
rw_bf_t dvfs0_zq_ds1_dds          :
		(11 - 9 + 1);
rw_bf_t dvfs0_zq_mode_pdds        :
		(14 - 12 + 1);
rw_bf_t dvfs0_zq_mode_dds         :
		(17 - 15 + 1);
ro_bf_t reserved_18_19               :
		(19 - 18 + 1);
rw_bf_t dvfs0_ds0_vref            :
		(25 - 20 + 1);
rw_bf_t dvfs0_ds1_vref            :
		(31 - 26 + 1);
	} bitfield;
} phy_dvfs0_con3_t;

typedef union {
	volatile unsigned int data;
	struct {
rw_bf_t dvfs0_phyupd_req_cycle    :
		(5 - 0 + 1);
ro_bf_t reserved_6_7                 :
		(7 - 6 + 1);
rw_bf_t dvfs0_mcupd_req_cycle     :
		(13 - 8 + 1);
ro_bf_t reserved_14_15               :
		(15 - 14 + 1);
rw_bf_t dvfs0_zq_ds0_term         :
		(18 - 16 + 1);
rw_bf_t dvfs0_zq_ds1_term         :
		(21 - 19 + 1);
ro_bf_t reserved_22_31				 :
		(31 - 22 + 1);
	} bitfield;
} phy_dvfs0_con4_t;

typedef union {
	volatile unsigned int data;
	struct {
ro_bf_t reserved_0                   :
		(0 - 0 + 1);
rw_bf_t zq_manual_str             :
		(1 - 1 + 1);
rw_bf_t zq_manual_mode            :
		(3 - 2 + 1);
rw_bf_t zq_udt_dly                :
		(11 - 4 + 1);
rw_bf_t zq_force_impp             :
		(14 - 12 + 1);
rw_bf_t zq_force_impn             :
		(17 - 15 + 1);
rw_bf_t zq_clk_div_en             :
		(18 - 18 + 1);
rw_bf_t zq_mode_noterm            :
		(19 - 19 + 1);
rw_bf_t zq_rgddr3                 :
		(20 - 20 + 1);
rw_bf_t zq_mode_term              :
		(23 - 21 + 1);
rw_bf_t zq_mode_dds               :
		(26 - 24 + 1);
rw_bf_t zq_clk_en                 :
		(27 - 27 + 1);
rw_bf_t zq_mode_pdds              :
		(30 - 28 + 1);
rw_bf_t zq_mode_lp4               :
		(31 - 31 + 1);
	} bitfield;
} phy_zq_con0_t;

struct smc_dfs_table {
	unsigned int DramTiming0;
	unsigned int DramTiming1;
	unsigned int DramTiming2;
	unsigned int DramTiming3;
	unsigned int DramTiming4;
	unsigned int DramTiming5;
	unsigned int DramTiming6;
	unsigned int DramTiming7;
	unsigned int DramTiming8;
	unsigned int DramTiming9;
	unsigned int DramDerateTiming0;
	unsigned int DramDerateTiming1;
	unsigned int Dimm0AutoRefTiming1;
	unsigned int Dimm1AutoRefTiming1;
	unsigned int AutoRefTiming2;
	unsigned int PwrMgmtTiming0;
	unsigned int PwrMgmtTiming1;
	unsigned int PwrMgmtTiming2;
	unsigned int PwrMgmtTiming3;
	unsigned int TmrTrnInterval;
	unsigned int DFIDelay1;
	unsigned int DFIDelay2;
	unsigned int DvfsTrnCtl;
	unsigned int TrnTiming0;
	unsigned int TrnTiming1;
	unsigned int TrnTiming2;
};

struct phy_dfs_table {
	unsigned int DVFSn_CON0;
	unsigned int DVFSn_CON1;
	unsigned int DVFSn_CON2;
	unsigned int DVFSn_CON3;
	unsigned int DVFSn_CON4;
};

struct dram_dfs_table {
	unsigned int DirectCmd_MR1;
	unsigned int DirectCmd_MR2;
	unsigned int DirectCmd_MR3;
	unsigned int DirectCmd_MR11;
	unsigned int DirectCmd_MR12;
	unsigned int DirectCmd_MR14;
	unsigned int DirectCmd_MR22;
};

struct vref_trn_t {
	unsigned short trained_vref[NUM_OF_CH];
};

struct vref_search_t {
	unsigned short min_step		: 4;
	unsigned short max_step		: 4;
	unsigned short search_step	: 4;
	unsigned short trial		: 4;
};

struct vref_info_t {
	unsigned char			vref;
	unsigned char			center;
	struct vref_search_t	search;
};

struct vref_config_t {
	unsigned char			num_of_level;
	struct vref_info_t		read;
	unsigned char			vref_read[PHY_CH_ALL][VREF_BUFFER];
	struct vref_info_t		write;
	unsigned char			vref_write[PHY_CH_ALL][VREF_BUFFER];
};

static struct smc_dfs_table *g_smc_dfs_table;
static struct phy_dfs_table *g_phy_dfs_table;
static struct dram_dfs_table *g_dram_dfs_table;
static unsigned long long *mif_freq_to_level;
static int num_mif_freq_to_level;
static unsigned int query_key;

static const unsigned long long mif_freq_to_level_switch[] = {
	/* BUS3_PLL SW 936 */	936 * MHZ,
	/* BUS0_PLL SW 528 */	528 * MHZ
};

static unsigned long vref_base;
static struct vref_config_t *vref_config;

#ifndef PWRCAL_TARGET_LINUX
/******************************************************************************
 *
 * @fn      get_smc_dfs_table
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void *get_smc_dfs_table(void)
{
	return((void *)g_smc_dfs_table);
}

/******************************************************************************
 *
 * @fn      get_phy_dfs_table
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void *get_phy_dfs_table(void)
{
	return((void *)g_phy_dfs_table);
}

/******************************************************************************
 *
 * @fn      get_dram_dfs_table
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void *get_dram_dfs_table(void)
{
	return((void *)g_dram_dfs_table);
}
#endif

/******************************************************************************
 *
 * @fn      pwrcal_get_dram_manufacturer
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
unsigned int pwrcal_get_dram_manufacturer(void)
{
	return (query_key);
}

/******************************************************************************
 *
 * @fn      smc_set_qchannel
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void smc_set_qchannel(int enable)
{
	if (enable == 0)	{
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY0, 0, 0x1);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY0, 0, 0x1);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY1, 0, 0x1);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY1, 0, 0x1);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY2, 0, 0x1);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY2, 0, 0x1);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY3, 0, 0x1);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY3, 0, 0x1);
	} else {
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY3, 0, 0x0);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY3, 0, 0x0);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY2, 0, 0x0);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY2, 0, 0x0);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY1, 0, 0x0);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY1, 0, 0x0);
		pwrcal_setbit(CG_CTRL_MAN_DDRPHY0, 0, 0x0);
		pwrcal_setbit(CG_CTRL_VAL_DDRPHY0, 0, 0x0);
	}
}

/******************************************************************************
 *
 * @fn      dmc_misc_direct_dmc_enable
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void dmc_misc_direct_dmc_enable(int enable)
{
	pwrcal_writel(DMC_MISC_CON0, (enable << 24) | (0x2 << 20));
}

/******************************************************************************
 *
 * @fn      smc_mode_register_write
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void smc_mode_register_write(int mr, int op)
{
	pwrcal_writel(ModeRegAddr, ((0x3 << 28) | (mr << 20)));
	pwrcal_writel(ModeRegWrData, op);
	pwrcal_writel(MprMrCtl, 0x10);
}

/******************************************************************************
 *
 * @fn      smc_mode_register_write_per_ch
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void smc_mode_register_write_per_ch(int ch, int mr, int rank, int op)
{
	void *modereg;
	void *moderegwrdata;
	void *mprmrctl;

	unsigned long base;

	switch (ch) {
	case 0:
		base = SMC0_BASE;
		break;
	case 1:
		base = SMC1_BASE;
		break;
	case 2:
		base = SMC2_BASE;
		break;
	case 3:
		base = SMC3_BASE;
		break;
	}
	modereg = (void *)(base);
	moderegwrdata = (void *)(base + 0x8);
	mprmrctl = (void *)(base + 0x4);

	pwrcal_writel(modereg, (((rank & 0x3) << 28) | (mr << 20)));
	pwrcal_writel(moderegwrdata, op);
	dsb(ishst);
	pwrcal_writel(mprmrctl, 0x10);
	dsb(ishst);
}

/******************************************************************************
 *
 * @fn      convert_to_level
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
static unsigned int convert_to_level(unsigned long long freq)
{
	int idx;
	int tablesize = num_mif_freq_to_level;

	for (idx = tablesize - 1; idx >= 0; idx--)
		if (freq <= mif_freq_to_level[idx])
			return (unsigned int)idx;

	return 0;
}

/******************************************************************************
 *
 * @fn      convert_to_level_switch
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
static unsigned int convert_to_level_switch(unsigned long long freq)
{
	int idx;
	int tablesize = sizeof(mif_freq_to_level_switch) / sizeof(mif_freq_to_level_switch[0]);

	for (idx = tablesize - 1; idx >= 0; idx--)
		if (freq <= mif_freq_to_level_switch[idx])
			return (unsigned int)idx;

	return 0;
}

typedef struct phy_dvfs0_con3_t DVFS_CON3;
typedef struct phy_dvfs0_con4_t DVFS_CON4;
typedef struct phy_zq_con0_t ZQ_CON0;

void *PHY_ZQ_CON0[] = {PHY_ZQ_CON0_CH0, PHY_ZQ_CON0_CH1, PHY_ZQ_CON0_CH2, PHY_ZQ_CON0_CH3};

void *PHY_DVFS0_CON0[] = {PHY_DVFS0_CON0_CH0, PHY_DVFS0_CON0_CH1, PHY_DVFS0_CON0_CH2, PHY_DVFS0_CON0_CH3};
void *PHY_DVFS0_CON1[] = {PHY_DVFS0_CON1_CH0, PHY_DVFS0_CON1_CH1, PHY_DVFS0_CON1_CH2, PHY_DVFS0_CON1_CH3};
void *PHY_DVFS0_CON2[] = {PHY_DVFS0_CON2_CH0, PHY_DVFS0_CON2_CH1, PHY_DVFS0_CON2_CH2, PHY_DVFS0_CON2_CH3};
void *PHY_DVFS0_CON3[] = {PHY_DVFS0_CON3_CH0, PHY_DVFS0_CON3_CH1, PHY_DVFS0_CON3_CH2, PHY_DVFS0_CON3_CH3};
void *PHY_DVFS0_CON4[] = {PHY_DVFS0_CON4_CH0, PHY_DVFS0_CON4_CH1, PHY_DVFS0_CON4_CH2, PHY_DVFS0_CON4_CH3};

void *PHY_DVFS1_CON0[] = {PHY_DVFS1_CON0_CH0, PHY_DVFS1_CON0_CH1, PHY_DVFS1_CON0_CH2, PHY_DVFS1_CON0_CH3};
void *PHY_DVFS1_CON1[] = {PHY_DVFS1_CON1_CH0, PHY_DVFS1_CON1_CH1, PHY_DVFS1_CON1_CH2, PHY_DVFS1_CON1_CH3};
void *PHY_DVFS1_CON2[] = {PHY_DVFS1_CON2_CH0, PHY_DVFS1_CON2_CH1, PHY_DVFS1_CON2_CH2, PHY_DVFS1_CON2_CH3};
void *PHY_DVFS1_CON3[] = {PHY_DVFS1_CON3_CH0, PHY_DVFS1_CON3_CH1, PHY_DVFS1_CON3_CH2, PHY_DVFS1_CON3_CH3};
void *PHY_DVFS1_CON4[] = {PHY_DVFS1_CON4_CH0, PHY_DVFS1_CON4_CH1, PHY_DVFS1_CON4_CH2, PHY_DVFS1_CON4_CH3};
/******************************************************************************
 *
 * @fn      pwrcal_dmc_set_dvfs
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void pwrcal_dmc_set_dvfs(unsigned long long target_mif_freq, unsigned int timing_set_idx)
{
	int n;
	int rank;

	unsigned int uReg;
	unsigned int target_mif_level_idx, target_mif_level_switch_idx;
	unsigned char mr13;
	unsigned char mr14;

	phy_dvfs0_con3_t DVFS_CON3_ECT, DVFS_CON3;
	phy_dvfs0_con4_t DVFS_CON4_ECT, DVFS_CON4;
	phy_zq_con0_t ZQ_CON0;

	target_mif_level_idx = convert_to_level(target_mif_freq);

		target_mif_level_switch_idx = convert_to_level_switch(target_mif_freq);
		target_mif_level_switch_idx += num_mif_freq_to_level;

	vref_config = (struct vref_config_t *)(unsigned long)vref_base;

	/* 1. Configure parameter */
	if (timing_set_idx == MIF_TIMING_SET_0) {
		//periodic training enable/disable as DVFS level

		//1. Set TmrTrnInterval as zero to disable new periodic training.
		pwrcal_writel(TmrTrnInterval_1, 0x0);

#ifndef PWRCAL_TARGET_LINUX
		for (n = 0; n < 4; n++) {
		//2. Wait until that TrnStatus.TrnSelStatus becomes 4'b0000 or 4'b1xxxx.
			while (!((pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x100000) == 0x8) || (pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x100000) == 0x0)));

		//3. Change TmrTrnInterval and TmrTrnCtl for new DVFS level.
		if (g_smc_dfs_table[target_mif_level_idx].DvfsTrnCtl == 0)
				pwrcal_writel(SMC_TmrTrnCtl_CH0 + n * 0x100000, 0x0);
		else
				pwrcal_writel(SMC_TmrTrnCtl_CH0 + n * 0x100000, (0x1 << 31) | g_smc_dfs_table[target_mif_level_idx].DvfsTrnCtl);
		}
#else
		for (n = 0; n < 4; n++) {
		//2. Wait until that TrnStatus.TrnSelStatus becomes 4'b0000 or 4'b1xxxx.
			while (!((pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x10000) == 0x8) || (pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x10000) == 0x0)));

		//3. Change TmrTrnInterval and TmrTrnCtl for new DVFS level.
		if (g_smc_dfs_table[target_mif_level_idx].DvfsTrnCtl == 0)
				pwrcal_writel(SMC_TmrTrnCtl_CH0 + n * 0x10000, 0x0);
		else
				pwrcal_writel(SMC_TmrTrnCtl_CH0 + n * 0x10000, (0x1 << 31) | g_smc_dfs_table[target_mif_level_idx].DvfsTrnCtl);
		}
#endif

		pwrcal_writel(DMC_MISC_CON1_CH0, 0x0);	//timing_set_sw_r=0x0
		pwrcal_writel(DMC_MISC_CON1_CH1, 0x0);	//timing_set_sw_r=0x0
		pwrcal_writel(DMC_MISC_CON1_CH2, 0x0);	//timing_set_sw_r=0x0
		pwrcal_writel(DMC_MISC_CON1_CH3, 0x0);	//timing_set_sw_r=0x0

		pwrcal_writel(DramTiming0_0, g_smc_dfs_table[target_mif_level_idx].DramTiming0);
		pwrcal_writel(DramTiming1_0, g_smc_dfs_table[target_mif_level_idx].DramTiming1);
		pwrcal_writel(DramTiming2_0, g_smc_dfs_table[target_mif_level_idx].DramTiming2);
		pwrcal_writel(DramTiming3_0, g_smc_dfs_table[target_mif_level_idx].DramTiming3);
		pwrcal_writel(DramTiming4_0, g_smc_dfs_table[target_mif_level_idx].DramTiming4);
		pwrcal_writel(DramTiming5_0, g_smc_dfs_table[target_mif_level_idx].DramTiming5);
		pwrcal_writel(DramTiming6_0, g_smc_dfs_table[target_mif_level_idx].DramTiming6);
		pwrcal_writel(DramTiming7_0, g_smc_dfs_table[target_mif_level_idx].DramTiming7);
		pwrcal_writel(DramTiming8_0, g_smc_dfs_table[target_mif_level_idx].DramTiming8);
		pwrcal_writel(DramTiming9_0, g_smc_dfs_table[target_mif_level_idx].DramTiming9);
		pwrcal_writel(DramDerateTiming0_0, g_smc_dfs_table[target_mif_level_idx].DramDerateTiming0);
		pwrcal_writel(DramDerateTiming1_0, g_smc_dfs_table[target_mif_level_idx].DramDerateTiming1);
		pwrcal_writel(Dimm0AutoRefTiming1_0, g_smc_dfs_table[target_mif_level_idx].Dimm0AutoRefTiming1);
		pwrcal_writel(Dimm1AutoRefTiming1_0, g_smc_dfs_table[target_mif_level_idx].Dimm1AutoRefTiming1);
		pwrcal_writel(AutoRefTiming2_0, g_smc_dfs_table[target_mif_level_idx].AutoRefTiming2);
		pwrcal_writel(PwrMgmtTiming0_0, g_smc_dfs_table[target_mif_level_idx].PwrMgmtTiming0);
		pwrcal_writel(PwrMgmtTiming1_0, g_smc_dfs_table[target_mif_level_idx].PwrMgmtTiming1);
		pwrcal_writel(PwrMgmtTiming2_0, g_smc_dfs_table[target_mif_level_idx].PwrMgmtTiming2);
		pwrcal_writel(PwrMgmtTiming3_0, g_smc_dfs_table[target_mif_level_idx].PwrMgmtTiming3);
		pwrcal_writel(TmrTrnInterval_0, g_smc_dfs_table[target_mif_level_idx].TmrTrnInterval);
		pwrcal_writel(DFIDelay1_0, g_smc_dfs_table[target_mif_level_idx].DFIDelay1);
		pwrcal_writel(DFIDelay2_0, g_smc_dfs_table[target_mif_level_idx].DFIDelay2);
		pwrcal_writel(DvfsTrnCtl_0, g_smc_dfs_table[target_mif_level_idx].DvfsTrnCtl);
		pwrcal_writel(TrnTiming0_0, g_smc_dfs_table[target_mif_level_idx].TrnTiming0);
		pwrcal_writel(TrnTiming1_0, g_smc_dfs_table[target_mif_level_idx].TrnTiming1);
		pwrcal_writel(TrnTiming2_0, g_smc_dfs_table[target_mif_level_idx].TrnTiming2);

		uReg = pwrcal_readl((void *)PHY_DVFS_CON_CH0);
		uReg &= ~(0x3 << 30);
		uReg |= (0x1 << 30);	//0x1 = DVFS 1 mode
		pwrcal_writel(PHY_DVFS_CON, uReg);

		for (n = 0; n < NUM_OF_CH; n++) {
			DVFS_CON3.data = DVFS_CON3_ECT.data = g_phy_dfs_table[target_mif_level_idx].DVFSn_CON3;
			DVFS_CON4.data = DVFS_CON4_ECT.data = g_phy_dfs_table[target_mif_level_idx].DVFSn_CON4;
			ZQ_CON0.data = pwrcal_readl(PHY_ZQ_CON0[n]);

			/* WA: Ch1 and Ch2 wrong connection SFR and IO control pin */
			if ((n == 1) || (n == 2)) {

				/* ca register => byte1 pad */
				DVFS_CON3.bitfield.dvfs0_zq_mode_dds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_dds;
				DVFS_CON3.bitfield.dvfs0_zq_mode_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_pdds;
				ZQ_CON0.bitfield.zq_mode_term				= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds1_term;

				/* ds0 register => ca pad */
				DVFS_CON3.bitfield.dvfs0_zq_ds0_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds0_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds0_term		= 0x0;

				/* ds1 register => byte0 pad */
				DVFS_CON3.bitfield.dvfs0_zq_ds1_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds1_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds1_term		= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds0_term;

			} else {

				DVFS_CON3.bitfield.dvfs0_zq_mode_dds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_dds;
				DVFS_CON3.bitfield.dvfs0_zq_mode_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_pdds;
				ZQ_CON0.bitfield.zq_mode_term				= 0x0;

				DVFS_CON3.bitfield.dvfs0_zq_ds0_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds0_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds0_term		= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds0_term;

				DVFS_CON3.bitfield.dvfs0_zq_ds1_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds1_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds1_term		= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds1_term;
			}

			/* tied to byte0 */
			DVFS_CON3.bitfield.dvfs0_ds0_vref				= DVFS_CON3_ECT.bitfield.dvfs0_ds0_vref;
			DVFS_CON3.bitfield.dvfs0_ds1_vref				= DVFS_CON3_ECT.bitfield.dvfs0_ds0_vref;

			pwrcal_writel(PHY_DVFS0_CON0[n], g_phy_dfs_table[target_mif_level_idx].DVFSn_CON0);
			pwrcal_writel(PHY_DVFS0_CON1[n], g_phy_dfs_table[target_mif_level_idx].DVFSn_CON1);
			pwrcal_writel(PHY_DVFS0_CON2[n], g_phy_dfs_table[target_mif_level_idx].DVFSn_CON2);
			pwrcal_writel(PHY_DVFS0_CON3[n], DVFS_CON3.data);
			pwrcal_writel(PHY_DVFS0_CON4[n], DVFS_CON4.data);
			pwrcal_writel(PHY_ZQ_CON0[n],	 ZQ_CON0.data);
		}

		mr13 = (0x1 << 7) | (0x0 << 6) | (0x0 << 5) | (0x1 << 3);	//FSP-OP=0x1, FSP-WR=0x0, DMD=0x0, VRCG=0x1
		smc_mode_register_write(DRAM_MR13, mr13);
		smc_mode_register_write(DRAM_MR1, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR1);
		smc_mode_register_write(DRAM_MR2, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR2);
		smc_mode_register_write(DRAM_MR3, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR3);
		smc_mode_register_write(DRAM_MR11, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR11);
		smc_mode_register_write(DRAM_MR12, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR12);

		if ((vref_config->write.vref != 0)) {
			for (n = 0; n < NUM_OF_CH; n++) {
				mr14 = vref_config->vref_write[n][target_mif_level_idx];
				for (rank = 1; rank < 0x4; rank <<= 1)
					smc_mode_register_write_per_ch(n, DRAM_MR14, rank, mr14 & 0xff);
			}
		} else {
			smc_mode_register_write(DRAM_MR14, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR14);
		}

		smc_mode_register_write(DRAM_MR22, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR22);

		mr13 &= ~(0x1 << 7);	// clear FSP-OP[7]
		pwrcal_writel(MRS_DATA1_CH0, mr13);
		pwrcal_writel(MRS_DATA1_CH1, mr13);
		pwrcal_writel(MRS_DATA1_CH2, mr13);
		pwrcal_writel(MRS_DATA1_CH3, mr13);
	} else if (timing_set_idx == MIF_TIMING_SET_1) {
		//periodic training enable/disable as DVFS level

		//1. Set TmrTrnInterval as zero to disable new periodic training.
		pwrcal_writel(TmrTrnInterval_0, 0x0);

#ifndef PWRCAL_TARGET_LINUX
		for (n = 0; n < 4; n++) {
		//2. Wait until that TrnStatus.TrnSelStatus becomes 4'b0000 or 4'b1xxxx.
			while (!((pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x100000) == 0x8) || (pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x100000) == 0x0)));

		//3. Change TmrTrnInterval and TmrTrnCtl for new DVFS level.
			pwrcal_writel(SMC_TmrTrnCtl_CH0 + n * 0x100000, 0x0);	//switching PLL is always periodic training off
		}
#else
		for (n = 0; n < 4; n++) {
		//2. Wait until that TrnStatus.TrnSelStatus becomes 4'b0000 or 4'b1xxxx.
			while (!((pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x10000) == 0x8) || (pwrcal_readl(SMC_TrnStatus_CH0 + n * 0x10000) == 0x0)));

		//3. Change TmrTrnInterval and TmrTrnCtl for new DVFS level.
			pwrcal_writel(SMC_TmrTrnCtl_CH0 + n * 0x10000, 0x0);	//switching PLL is always periodic training off
		}
#endif

		pwrcal_writel(DMC_MISC_CON1_CH0, 0x1);	//timing_set_sw_r=0x1
		pwrcal_writel(DMC_MISC_CON1_CH1, 0x1);	//timing_set_sw_r=0x1
		pwrcal_writel(DMC_MISC_CON1_CH2, 0x1);	//timing_set_sw_r=0x1
		pwrcal_writel(DMC_MISC_CON1_CH3, 0x1);	//timing_set_sw_r=0x1

		pwrcal_writel(DramTiming0_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming0);
		pwrcal_writel(DramTiming1_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming1);
		pwrcal_writel(DramTiming2_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming2);
		pwrcal_writel(DramTiming3_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming3);
		pwrcal_writel(DramTiming4_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming4);
		pwrcal_writel(DramTiming5_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming5);
		pwrcal_writel(DramTiming6_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming6);
		pwrcal_writel(DramTiming7_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming7);
		pwrcal_writel(DramTiming8_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming8);
		pwrcal_writel(DramTiming9_1, g_smc_dfs_table[target_mif_level_switch_idx].DramTiming9);
		pwrcal_writel(DramDerateTiming0_1, g_smc_dfs_table[target_mif_level_switch_idx].DramDerateTiming0);
		pwrcal_writel(DramDerateTiming1_1, g_smc_dfs_table[target_mif_level_switch_idx].DramDerateTiming1);
		pwrcal_writel(Dimm0AutoRefTiming1_1, g_smc_dfs_table[target_mif_level_switch_idx].Dimm0AutoRefTiming1);
		pwrcal_writel(Dimm1AutoRefTiming1_1, g_smc_dfs_table[target_mif_level_switch_idx].Dimm1AutoRefTiming1);
		pwrcal_writel(AutoRefTiming2_1, g_smc_dfs_table[target_mif_level_switch_idx].AutoRefTiming2);
		pwrcal_writel(PwrMgmtTiming0_1, g_smc_dfs_table[target_mif_level_switch_idx].PwrMgmtTiming0);
		pwrcal_writel(PwrMgmtTiming1_1, g_smc_dfs_table[target_mif_level_switch_idx].PwrMgmtTiming1);
		pwrcal_writel(PwrMgmtTiming2_1, g_smc_dfs_table[target_mif_level_switch_idx].PwrMgmtTiming2);
		pwrcal_writel(PwrMgmtTiming3_1, g_smc_dfs_table[target_mif_level_switch_idx].PwrMgmtTiming3);
		pwrcal_writel(TmrTrnInterval_1, g_smc_dfs_table[target_mif_level_switch_idx].TmrTrnInterval);
		pwrcal_writel(DFIDelay1_1, g_smc_dfs_table[target_mif_level_switch_idx].DFIDelay1);
		pwrcal_writel(DFIDelay2_1, g_smc_dfs_table[target_mif_level_switch_idx].DFIDelay2);
		pwrcal_writel(DvfsTrnCtl_1, g_smc_dfs_table[target_mif_level_switch_idx].DvfsTrnCtl);
		pwrcal_writel(TrnTiming0_1, g_smc_dfs_table[target_mif_level_switch_idx].TrnTiming0);
		pwrcal_writel(TrnTiming1_1, g_smc_dfs_table[target_mif_level_switch_idx].TrnTiming1);
		pwrcal_writel(TrnTiming2_1, g_smc_dfs_table[target_mif_level_switch_idx].TrnTiming2);

		uReg = pwrcal_readl(PHY_DVFS_CON_CH0);
		uReg &= ~(0x3 << 30);
		uReg |= (0x2 << 30);	//0x2 = DVFS 2 mode
		pwrcal_writel(PHY_DVFS_CON, uReg);

		for (n = 0; n < NUM_OF_CH; n++) {

			DVFS_CON3.data = DVFS_CON3_ECT.data = g_phy_dfs_table[target_mif_level_switch_idx].DVFSn_CON3;
			DVFS_CON4.data = DVFS_CON4_ECT.data = g_phy_dfs_table[target_mif_level_switch_idx].DVFSn_CON4;
			ZQ_CON0.data = pwrcal_readl(PHY_ZQ_CON0[n]);

			/* WA: Ch1 and Ch2 wrong connection SFR and IO control pin */
			if ((n == 1) || (n == 2)) {

				/* ca register => byte1 pad */
				DVFS_CON3.bitfield.dvfs0_zq_mode_dds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_dds;
				DVFS_CON3.bitfield.dvfs0_zq_mode_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_pdds;
				ZQ_CON0.bitfield.zq_mode_term				= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds1_term;

				/* ds0 register => ca pad */
				DVFS_CON3.bitfield.dvfs0_zq_ds0_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds0_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds0_term		= 0x0;

				/* ds1 register => byte0 pad */
				DVFS_CON3.bitfield.dvfs0_zq_ds1_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds1_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds1_term		= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds0_term;

			} else {

				DVFS_CON3.bitfield.dvfs0_zq_mode_dds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_dds;
				DVFS_CON3.bitfield.dvfs0_zq_mode_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_mode_pdds;
				ZQ_CON0.bitfield.zq_mode_term				= 0x0;

				DVFS_CON3.bitfield.dvfs0_zq_ds0_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds0_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds0_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds0_term		= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds0_term;

				DVFS_CON3.bitfield.dvfs0_zq_ds1_dds			= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_dds;
				DVFS_CON3.bitfield.dvfs0_zq_ds1_pdds		= DVFS_CON3_ECT.bitfield.dvfs0_zq_ds1_pdds;
				DVFS_CON4.bitfield.dvfs0_zq_ds1_term		= DVFS_CON4_ECT.bitfield.dvfs0_zq_ds1_term;
			}

			/* tied to byte0 */
			DVFS_CON3.bitfield.dvfs0_ds0_vref				= DVFS_CON3_ECT.bitfield.dvfs0_ds0_vref;
			DVFS_CON3.bitfield.dvfs0_ds1_vref				= DVFS_CON3_ECT.bitfield.dvfs0_ds0_vref;

			pwrcal_writel(PHY_DVFS1_CON0[n], g_phy_dfs_table[target_mif_level_switch_idx].DVFSn_CON0);
			pwrcal_writel(PHY_DVFS1_CON1[n], g_phy_dfs_table[target_mif_level_switch_idx].DVFSn_CON1);
			pwrcal_writel(PHY_DVFS1_CON2[n], g_phy_dfs_table[target_mif_level_switch_idx].DVFSn_CON2);
			pwrcal_writel(PHY_DVFS1_CON3[n], DVFS_CON3.data);
			pwrcal_writel(PHY_DVFS1_CON4[n], DVFS_CON4.data);
			pwrcal_writel(PHY_ZQ_CON0[n],	 ZQ_CON0.data);
		}


		mr13 = (0x0 << 7) | (0x1 << 6) | (0x0 << 5) | (0x1 << 3);	//FSP-OP=0x0, FSP-WR=0x1, DMD=0x0, VRCG=0x1
		smc_mode_register_write(DRAM_MR13, mr13);
		smc_mode_register_write(DRAM_MR1, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR1);
		smc_mode_register_write(DRAM_MR2, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR2);
		smc_mode_register_write(DRAM_MR3, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR3);
		smc_mode_register_write(DRAM_MR11, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR11);
		smc_mode_register_write(DRAM_MR12, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR12);

		if ((vref_config->write.vref != 0)) {
			for (n = 0; n < NUM_OF_CH; n++) {
				mr14 = vref_config->vref_write[n][target_mif_level_switch_idx];
				for (rank = 1; rank < 0x4; rank <<= 1)
					smc_mode_register_write_per_ch(n, DRAM_MR14, rank, mr14 & 0xff);
			}
		} else {
			smc_mode_register_write(DRAM_MR14, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR14);
		}

		smc_mode_register_write(DRAM_MR22, g_dram_dfs_table[target_mif_level_switch_idx].DirectCmd_MR22);

		mr13 &= ~(0x1 << 7);	// clear FSP-OP[7]
		mr13 |= (0x1 << 7);	// set FSP-OP[7]=0x1
		pwrcal_writel(MRS_DATA1_CH0, mr13);
		pwrcal_writel(MRS_DATA1_CH1, mr13);
		pwrcal_writel(MRS_DATA1_CH2, mr13);
		pwrcal_writel(MRS_DATA1_CH3, mr13);
	}	 else {
		pr_err("wrong DMC timing set selection on DVFS\n");
		return;
	}
}

/******************************************************************************
 *
 * @fn      dfs_dram_param_init
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void dfs_dram_param_init(void)
{
	int i;
	void *dram_block;
	struct ect_timing_param_size *size;

	dram_block = ect_get_block(BLOCK_TIMING_PARAM);
	if (dram_block == NULL)
		return;
	query_key = pwrcal_readl(PMU_DREX_CALIBRATION3);
	if (query_key != 0) {
		size = ect_timing_param_get_key(dram_block, query_key | 0x1);
	} else {
		query_key = 3;
		size = ect_timing_param_get_size(dram_block, query_key);
	}

	if (size == NULL)
		return;

	if (num_of_g_smc_dfs_table_column + num_of_g_phy_dfs_table_column + num_of_g_dram_dfs_table_column != size->num_of_timing_param)
		return;

	g_smc_dfs_table = kzalloc(sizeof(struct smc_dfs_table) * num_of_g_smc_dfs_table_column * size->num_of_level, GFP_KERNEL);
	if (g_smc_dfs_table == NULL)
		return;

	g_phy_dfs_table = kzalloc(sizeof(struct phy_dfs_table) * num_of_g_phy_dfs_table_column * size->num_of_level, GFP_KERNEL);
	if (g_phy_dfs_table == NULL)
		return;

	g_dram_dfs_table = kzalloc(sizeof(struct dram_dfs_table) * num_of_g_dram_dfs_table_column * size->num_of_level, GFP_KERNEL);
	if (g_dram_dfs_table == NULL)
		return;

	for (i = 0; i < size->num_of_level; ++i) {
		g_smc_dfs_table[i].DramTiming0 = size->timing_parameter[i * num_of_dram_parameter + DramTiming0];
		g_smc_dfs_table[i].DramTiming1 = size->timing_parameter[i * num_of_dram_parameter + DramTiming1];
		g_smc_dfs_table[i].DramTiming2 = size->timing_parameter[i * num_of_dram_parameter + DramTiming2];
		g_smc_dfs_table[i].DramTiming3 = size->timing_parameter[i * num_of_dram_parameter + DramTiming3];
		g_smc_dfs_table[i].DramTiming4 = size->timing_parameter[i * num_of_dram_parameter + DramTiming4];
		g_smc_dfs_table[i].DramTiming5 = size->timing_parameter[i * num_of_dram_parameter + DramTiming5];
		g_smc_dfs_table[i].DramTiming6 = size->timing_parameter[i * num_of_dram_parameter + DramTiming6];
		g_smc_dfs_table[i].DramTiming7 = size->timing_parameter[i * num_of_dram_parameter + DramTiming7];
		g_smc_dfs_table[i].DramTiming8 = size->timing_parameter[i * num_of_dram_parameter + DramTiming8];
		g_smc_dfs_table[i].DramTiming9 = size->timing_parameter[i * num_of_dram_parameter + DramTiming9];
		g_smc_dfs_table[i].DramDerateTiming0 = size->timing_parameter[i * num_of_dram_parameter + DramDerateTiming0];
		g_smc_dfs_table[i].DramDerateTiming1 = size->timing_parameter[i * num_of_dram_parameter + DramDerateTiming1];
		g_smc_dfs_table[i].Dimm0AutoRefTiming1 = size->timing_parameter[i * num_of_dram_parameter + Dimm0AutoRefTiming1];
		g_smc_dfs_table[i].Dimm1AutoRefTiming1 = size->timing_parameter[i * num_of_dram_parameter + Dimm1AutoRefTiming1];
		g_smc_dfs_table[i].AutoRefTiming2 = size->timing_parameter[i * num_of_dram_parameter + AutoRefTiming2];
		g_smc_dfs_table[i].PwrMgmtTiming0 = size->timing_parameter[i * num_of_dram_parameter + PwrMgmtTiming0];
		g_smc_dfs_table[i].PwrMgmtTiming1 = size->timing_parameter[i * num_of_dram_parameter + PwrMgmtTiming1];
		g_smc_dfs_table[i].PwrMgmtTiming2 = size->timing_parameter[i * num_of_dram_parameter + PwrMgmtTiming2];
		g_smc_dfs_table[i].PwrMgmtTiming3 = size->timing_parameter[i * num_of_dram_parameter + PwrMgmtTiming3];
		g_smc_dfs_table[i].TmrTrnInterval = size->timing_parameter[i * num_of_dram_parameter + TmrTrnInterval];
		g_smc_dfs_table[i].DFIDelay1 = size->timing_parameter[i * num_of_dram_parameter + DFIDelay1];
		g_smc_dfs_table[i].DFIDelay2 = size->timing_parameter[i * num_of_dram_parameter + DFIDelay2];
		g_smc_dfs_table[i].DvfsTrnCtl = size->timing_parameter[i * num_of_dram_parameter + DvfsTrnCtl];
		g_smc_dfs_table[i].TrnTiming0 = size->timing_parameter[i * num_of_dram_parameter + TrnTiming0];
		g_smc_dfs_table[i].TrnTiming1 = size->timing_parameter[i * num_of_dram_parameter + TrnTiming1];
		g_smc_dfs_table[i].TrnTiming2 = size->timing_parameter[i * num_of_dram_parameter + TrnTiming2];

		g_phy_dfs_table[i].DVFSn_CON0 = size->timing_parameter[i * num_of_dram_parameter + DVFSn_CON0];
		g_phy_dfs_table[i].DVFSn_CON1 = size->timing_parameter[i * num_of_dram_parameter + DVFSn_CON1];
		g_phy_dfs_table[i].DVFSn_CON2 = size->timing_parameter[i * num_of_dram_parameter + DVFSn_CON2];
		g_phy_dfs_table[i].DVFSn_CON3 = size->timing_parameter[i * num_of_dram_parameter + DVFSn_CON3];
		g_phy_dfs_table[i].DVFSn_CON4 = size->timing_parameter[i * num_of_dram_parameter + DVFSn_CON4];

		g_dram_dfs_table[i].DirectCmd_MR1 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR1];
		g_dram_dfs_table[i].DirectCmd_MR2 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR2];
		g_dram_dfs_table[i].DirectCmd_MR3 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR3];
		g_dram_dfs_table[i].DirectCmd_MR11 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR11];
		g_dram_dfs_table[i].DirectCmd_MR12 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR12];
		g_dram_dfs_table[i].DirectCmd_MR14 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR14];
		g_dram_dfs_table[i].DirectCmd_MR22 = size->timing_parameter[i * num_of_dram_parameter + DirectCmd_MR22];
	}
}

/******************************************************************************
 *
 * @fn      dfs_mif_level_init
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void dfs_mif_level_init(void)
{
	int i;
	void *dvfs_block;
	struct ect_dvfs_domain *domain;

	dvfs_block = ect_get_block(BLOCK_DVFS);
	if (dvfs_block == NULL)
		return;

	domain = ect_dvfs_get_domain(dvfs_block, vclk_dvfs_mif.vclk.name);
	if (domain == NULL)
		return;

	mif_freq_to_level = kzalloc(sizeof(unsigned long long) * domain->num_of_level, GFP_KERNEL);
	if (mif_freq_to_level == NULL)
		return;

	num_mif_freq_to_level = domain->num_of_level;

	for (i = 0; i < domain->num_of_level; ++i)
		mif_freq_to_level[i] = domain->list_level[i].level * KHZ;
}

/******************************************************************************
 *
 * @fn      dfs_dram_init
 *
 * @brief
 *
 * @param
 *
 * @return
 *
 *****************************************************************************/
void dfs_dram_init(void)
{
#ifndef PWRCAL_TARGET_LINUX
	vref_base = pwrcal_readl(PMU_DREX_CALIBRATION2);
#else
	vref_base = (unsigned long)ioremap(pwrcal_readl(PMU_DREX_CALIBRATION2), SZ_2K);
#endif

	dfs_dram_param_init();
	dfs_mif_level_init();
}
