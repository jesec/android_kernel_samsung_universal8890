#include "../pwrcal-env.h"
#include "../pwrcal-rae.h"
#include "S5E8890-sfrbase.h"

#ifndef MHZ
#define MHZ		((unsigned long long)1000000)
#endif

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
#define AutoRefTiming2_1		((void *)(__SMC_ALL + 0x0168))
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

#define PHY_DVFS_CON_CH0		((void *)(LPDDR4_PHY0_BASE + 0x00B8))
#define PHY_DVFS_CON			((void *)(__PHY_ALL + 0x00B8))
#define PHY_DVFS0_CON0			((void *)(__PHY_ALL + 0x00BC))
#define PHY_DVFS0_CON1			((void *)(__PHY_ALL + 0x00C4))
#define PHY_DVFS0_CON2			((void *)(__PHY_ALL + 0x00CC))
#define PHY_DVFS0_CON3			((void *)(__PHY_ALL + 0x00D4))
#define PHY_DVFS0_CON4			((void *)(__PHY_ALL + 0x00DC))

#define PHY_DVFS1_CON0			((void *)(__PHY_ALL + 0x00C0))
#define PHY_DVFS1_CON1			((void *)(__PHY_ALL + 0x00C8))
#define PHY_DVFS1_CON2			((void *)(__PHY_ALL + 0x00D0))
#define PHY_DVFS1_CON3			((void *)(__PHY_ALL + 0x00D8))
#define PHY_DVFS1_CON4			((void *)(__PHY_ALL + 0x00E0))

#define DMC_MISC_CON0			((void *)(__DMC_MISC_ALL + 0x0014))
#define DMC_MISC_CON1			((void *)(__DMC_MISC_ALL + 0x003C))
#define MRS_DATA1				((void *)(__DMC_MISC_ALL + 0x0054))

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

static const struct smc_dfs_table g_smc_dfs_table[] = { /*
  Freq       DramTiming0 DramTiming1 DramTiming2 DramTiming3 DramTiming4 DramTiming5 DramTiming6 DramTiming7 DramTiming8 DramTiming9 DramDerateTiming0 DramDerateTiming1 Dimm0AutoRefTiming1 Dimm1AutoRefTiming1 AutoRefTiming2 PwrMgmtTiming0 PwrMgmtTiming1 PwrMgmtTiming2 PwrMgmtTiming3 TmrTrnInterval DFIDelay1   DFIDelay2  DvfsTrnCtl  TrnTiming0  TrnTiming1  TrnTiming2 */
/*L0 1846*/ {0x00081411, 0x11073B27, 0x0A000025, 0x00000100, 0x110A0A00, 0x001B0E05, 0x00070004, 0x00070004, 0x00001504, 0x0D18281E,   0x3F291613,       0x0000130B,        0x005400A7,         0x005400A7,       0x0000000A,     0x07020907,    0x00000707,    0x000000B0,    0x00000904,    0x00000000, 0x0001081A, 0x00001A07, 0x00000303, 0x2F220EE7, 0x222F1722, 0x00000014},
/*L1 1742*/ {0x00081310, 0x10073825, 0x09000023, 0x00000100, 0x10090900, 0x001A0E05, 0x00070004, 0x00070004, 0x00001504, 0x0D17281D,   0x3C271412,       0x0000120B,        0x004F009D,         0x004F009D,       0x00000009,     0x07020907,    0x00000707,    0x000000A6,    0x00000882,    0x00000000, 0x00010819, 0x00001907, 0x00000303, 0x2E220EDA, 0x222E1622, 0x00000014},
/*L2 1560*/ {0x0007110F, 0x0F063221, 0x08000020, 0x00000100, 0x0F080800, 0x00180C05, 0x00070004, 0x00070004, 0x00001304, 0x0B14281A,   0x35231210,       0x0000100A,        0x0047008D,         0x0047008D,       0x00000008,     0x06020806,    0x00000606,    0x00000095,    0x0000079E,    0x00000000, 0x00010717, 0x00001706, 0x00000303, 0x2C1E0CC3, 0x1E2C141E, 0x00000014},
/*L3 1482*/ {0x0007100E, 0x0E063020, 0x0800001E, 0x00000100, 0x0E080800, 0x00180C05, 0x00070004, 0x00070004, 0x00001304, 0x0B142819,   0x3321110F,       0x00000F09,        0x00430086,         0x00430086,       0x00000008,     0x06020806,    0x00000606,    0x0000008D,    0x0000073D,    0x00000000, 0x00010717, 0x00001706, 0x00000303, 0x2C1E0CBA, 0x1E2C141E, 0x00000014},
/*L4 1352*/ {0x00070F0D, 0x0D062C1D, 0x0700001C, 0x00000100, 0x0D070700, 0x00170B05, 0x00070004, 0x00070004, 0x00001304, 0x0A132817,   0x2F1E100E,       0x00000E09,        0x003D007A,         0x003D007A,       0x00000007,     0x06020806,    0x00000606,    0x00000081,    0x0000069A,    0x00000000, 0x00010716, 0x00001606, 0x00000303, 0x2B1D0BA9, 0x1D2B131D, 0x00000014},
/*L5 1222*/ {0x00060D0B, 0x0B05271A, 0x07000019, 0x00000100, 0x0B070700, 0x00150A05, 0x00070004, 0x00070004, 0x00001204, 0x0A122815,   0x2A1B0E0D,       0x00000D08,        0x0037006E,         0x0037006E,       0x00000007,     0x05020705,    0x00000505,    0x00000075,    0x000005F8,    0x00000000, 0x00010614, 0x00001405, 0x00000303, 0x291B0A99, 0x1B29121B, 0x00000014},
/*L6 1092*/ {0x00060C0A, 0x0A052317, 0x06000016, 0x00000100, 0x0A060600, 0x00140905, 0x00070004, 0x00070004, 0x00001104, 0x0A112813,   0x26180D0B,	     0x00000B07,		0x00320063,		    0x00320063,		  0x00000006,	  0x05020705,    0x00000505,	0x00000069,	   0x00000555,	  0x00000000, 0x00010613, 0x00001305, 0x00000303, 0x281A0989, 0x1A28111A, 0x00000014},
/*L7  962*/ {0x00050B09, 0x09042015, 0x05000014, 0x00000100, 0x09050500, 0x00110805, 0x00070004, 0x00070004, 0x00000F04, 0x0A0F2811,   0x22160C0A,	     0x00000A06,	    0x002C0057,		    0x002C0057,		  0x00000005,	  0x04020604,    0x00000404,	0x0000005D,	   0x000004B3,	  0x00000000, 0x00010510, 0x00001004, 0x00000303, 0x25180879, 0x16250F18, 0x00000014},
/*L8  832*/ {0x00050908, 0x08041B12, 0x05000011, 0x00000100, 0x08050500, 0x00100705, 0x00070004, 0x00070004, 0x00000F04, 0x0A0F270F,   0x1D130A09,	     0x00000905,		0x0026004B,		    0x0026004B,		  0x00000005,     0x04020604,    0x00000404,	0x00000050,	   0x00000410,	  0x00000000, 0x0001050F, 0x00000F04, 0x00000000, 0x24180768, 0x16240F18, 0x00000014},
/*L9  676*/ {0x00040808, 0x0804170F, 0x0400000E, 0x00000100, 0x07040400, 0x000D0605, 0x00070004, 0x00070004, 0x00000D04, 0x0A0D210D,   0x190F0807,	     0x00000705,		0x001F003D,		    0x001F003D,		  0x00000004,	  0x03020503,    0x00000404,	0x00000042,	   0x0000034D,	  0x00000000, 0x0001040C, 0x00000C03, 0x00000000, 0x21180655, 0x13210D18, 0x00000014},
/*L10 572*/ {0x00040708, 0x0804140D, 0x0400000C, 0x00000100, 0x06040400, 0x000D0605, 0x00070004, 0x00070004, 0x00000D04, 0x0A0D210B,   0x160D0706,	     0x00000604,	    0x001A0034,		    0x001A0034,		  0x00000004,	  0x03020503,    0x00000404,	0x00000038,	   0x000002CB,	  0x00000000, 0x0001040C, 0x00000C03, 0x00000000, 0x21180548, 0x13210D18, 0x00000014},
/*L11 416*/ {0x00030508, 0x08040E09, 0x04000009, 0x00000100, 0x04040400, 0x000A0605, 0x00070005, 0x00070005, 0x00000B04, 0x0A0C1C09,   0x0F0A0505,	     0x00000504,	    0x00130026,		    0x00130026,		  0x00000004,	  0x03020403,    0x00000404,    0x00000029,	   0x00000208,	  0x00000000, 0x00010309, 0x00000902, 0x00000000, 0x1E180434, 0x101E0C18, 0x00000014},
/*L12 286*/ {0x00030408, 0x08040C07, 0x04000006, 0x00000100, 0x04040400, 0x000A0605, 0x00070007, 0x00070007, 0x00000B04, 0x0A0C1C07,   0x0D070404,	     0x00000404,		0x000D001A,		    0x000D001A,		  0x00000004,	  0x03020403,    0x00000404,	0x0000001D,	   0x00000166,	  0x00000000, 0x00010309, 0x00000902, 0x00000000, 0x1E180324, 0x101E0C18, 0x00000014}
};

static const struct phy_dfs_table g_phy_dfs_table[] = { /*
  Freq       DVFSn_CON0  DVFSn_CON1  DVFSn_CON2  DVFSn_CON3  DVFSn_CON4*/
/*L0 1846*/ {0x73691800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L1 1742*/ {0x6CE91800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L2 1560*/ {0x61881800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L3 1482*/ {0x5CA81800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L4 1352*/ {0x54881800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L5 1222*/ {0x4C671800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L6 1092*/ {0x44471800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L7  962*/ {0x3C259800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/*L8  832*/ {0x34058800, 0x80100000, 0x00004051, 0x7DF3FFFF, 0x00003F3F},
/*L9  676*/ {0x2A440800, 0x80100000, 0x00004051, 0x7DF3FFFF, 0x00003F3F},
/*L10 572*/ {0x23C40800, 0x80100000, 0x00004051, 0x7DF3FFFF, 0x00003F3F},
/*L11 416*/ {0x1A030800, 0xC0100000, 0xA3004051, 0x7DF3FFFF, 0x00003F3F},
/*L11 286*/ {0x11E30800, 0xC0100000, 0xA3004051, 0x7DF3FFFF, 0x00003F3F}
};

static const struct dram_dfs_table g_dram_dfs_table[] = { /*
  Freq           MR1         MR2         MR3         MR11        MR12        MR14        MR22   */
/*L0 1846*/ {0x0000006E, 0x00000036, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L1 1742*/ {0x0000006E, 0x00000036, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L2 1560*/ {0x0000005E, 0x0000002D, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L3 1482*/ {0x0000005E, 0x0000002D, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L4 1352*/ {0x0000005E, 0x0000002D, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L5 1222*/ {0x0000004E, 0x00000024, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L6 1092*/ {0x0000004E, 0x00000024, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L7  962*/ {0x0000003E, 0x0000001B, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L8  832*/ {0x00000036, 0x0000001B, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L9  676*/ {0x00000026, 0x00000012, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L10 572*/ {0x00000026, 0x00000012, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L11 416*/ {0x00000016, 0x00000009, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/*L12 286*/ {0x00000016, 0x00000009, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026}
};

static const unsigned long long mif_freq_to_level[] = {
/*L0 1846*/ 1846 * MHZ,
/*L1 1742*/ 1742 * MHZ,
/*L2 1560*/ 1560 * MHZ,
/*L3 1482*/ 1482 * MHZ,
/*L4 1352*/ 1352 * MHZ,
/*L5 1222*/ 1222 * MHZ,
/*L6 1092*/ 1092 * MHZ,
/*L7  962*/  962 * MHZ,
/*L8  832*/  832 * MHZ,
/*L9  676*/  676 * MHZ,
/*L10 572*/  572 * MHZ,
/*L11 416*/  416 * MHZ,
/*L12 286*/  286 * MHZ,
};


static const struct smc_dfs_table g_smc_dfs_table_switch[] = { /*
  Freq       DramTiming0 DramTiming1 DramTiming2 DramTiming3 DramTiming4 DramTiming5 DramTiming6 DramTiming7 DramTiming8 DramTiming9 DramDerateTiming0 DramDerateTiming1 Dimm0AutoRefTiming1 Dimm1AutoRefTiming1 AutoRefTiming2 PwrMgmtTiming0 PwrMgmtTiming1 PwrMgmtTiming2 PwrMgmtTiming3 TmrTrnInterval DFIDelay1   DFIDelay2  DvfsTrnCtl  TrnTiming0  TrnTiming1  TrnTiming2 */
/* 962 */ {0x00050B09, 0x09042015, 0x05000014, 0x00000100, 0x09050500, 0x00110805, 0x00070004, 0x00070004, 0x00000F04, 0x0A0F2811,	 0x22160C0A,	   0x00000A06,		  0x002C0057,		  0x002C0057,		0x00000005,	0x04020604,    0x00000404,	  0x0000005D,	 0x000004B3,	0x00000000, 0x00010510, 0x00001004, 0x00000303, 0x25180879, 0x16250F18, 0x00000014},
//* 936 */ {0x00050A09, 0x09041E14, 0x05000013, 0x00000100, 0x09050500, 0x00110805, 0x00070004, 0x00070004, 0x00000F04, 0x0A0F2811,	0x20150B0A,	   0x00000A06,		  0x002B0055,		  0x002B0055,		0x00000005,	0x04020604,    0x00000404,	  0x0000005A,	 0x00000492,	0x00000000, 0x00010510, 0x00001004, 0x00000303, 0x25180875, 0x16250F18, 0x00000014},
/* 572 */ {0x00040708, 0x0804140D, 0x0400000C, 0x00000100, 0x06040400, 0x000D0605, 0x00070004, 0x00070004, 0x00000D04, 0x0A0D210B,	 0x160D0706,	   0x00000604,		  0x001A0034,		  0x001A0034,		0x00000004,	0x03020503,    0x00000404,	  0x00000038,	 0x000002CB,	0x00000000, 0x0001040C, 0x00000C03, 0x00000000, 0x21180548, 0x13210D18, 0x00000014}
//* 468 */ {0x00030508, 0x08040F0A, 0x0400000A, 0x00000100, 0x05040400, 0x000A0605, 0x00070005, 0x00070005, 0x00000B04, 0x0A0C1C0A,   0x100B0605,	     0x00000504,	    0x0016002B,		    0x0016002B,		  0x00000004,	  0x03020403,    0x00000404,    0x0000002E,	   0x00000249,	  0x00000000, 0x00010309, 0x00000902, 0x00000000, 0x1E18043B, 0x101E0C18, 0x00000014}
};

static const struct phy_dfs_table g_phy_dfs_table_switch[] = { /*
  Freq       DVFSn_CON0  DVFSn_CON1  DVFSn_CON2  DVFSn_CON3  DVFSn_CON4*/
/* 962 */ {0x3C259800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
//* 936 */ {0x3A859800, 0x80100000, 0x4001A070, 0x7DF3FFFF, 0x00003F3F},
/* 572 */ {0x23C40800, 0x80100000, 0x00004051, 0x7DF3FFFF, 0x00003F3F}
//* 468 */ {0x1D430800, 0xC0100000, 0x00004051, 0x7DF3FFFF, 0x00003F3F}
};

static const struct dram_dfs_table g_dram_dfs_table_switch[] = { /*
  Freq           MR1         MR2         MR3         MR11        MR12        MR14        MR22   */
/* 962 */ {0x0000003E, 0x0000001B, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
//* 936 */ {0x0000003E, 0x0000001B, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026},
/* 572 */ {0x00000026, 0x00000012, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026}
//* 468 */ {0x00000016, 0x00000009, 0x000000F1, 0x00000004, 0x0000005D, 0x00000017, 0x00000026}
};

static const unsigned long long mif_freq_to_level_switch[] = {
/* 936 */  936 * MHZ,
/* 468 */  468 * MHZ
};

void dmc_misc_direct_dmc_enable(int enable)
{
	pwrcal_writel(DMC_MISC_CON0, (enable<<24)|(0x2<<20));
}

void smc_mode_register_write(int mr, int op)
{
	pwrcal_writel(ModeRegAddr, ((0x3<<28)|(mr<<20)));
	pwrcal_writel(ModeRegWrData, op);
	pwrcal_writel(MprMrCtl, 0x10);
}

static unsigned int convert_to_level(unsigned long long freq)
{
	int idx;
	int tablesize = sizeof(mif_freq_to_level) / sizeof(mif_freq_to_level[0]);

	for (idx = tablesize - 1; idx >= 0; idx--)
		if (freq <= mif_freq_to_level[idx])
			return (unsigned int)idx;

	return 0;
}

static unsigned int convert_to_level_switch(unsigned long long freq)
{
	int idx;
	int tablesize = sizeof(mif_freq_to_level_switch) / sizeof(mif_freq_to_level_switch[0]);

	for (idx = tablesize - 1; idx >= 0; idx--)
		if (freq <= mif_freq_to_level_switch[idx])
			return (unsigned int)idx;

	return 0;
}

void pwrcal_dmc_set_dvfs(unsigned long long target_mif_freq, unsigned int timing_set_idx)
{
	unsigned int uReg;
	unsigned int target_mif_level_idx, target_mif_level_switch_idx;
	unsigned int mr13;

	target_mif_level_idx = convert_to_level(target_mif_freq);

	target_mif_level_switch_idx = convert_to_level_switch(target_mif_freq);

	/* 1. Configure parameter */
	if (timing_set_idx == MIF_TIMING_SET_0) {
		pwrcal_writel(DMC_MISC_CON1, 0x0);	//timing_set_sw_r=0x0

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
		uReg &= ~(0x3<<30);
		uReg |= (0x1<<30);	//0x1 = DVFS 1 mode
		pwrcal_writel(PHY_DVFS_CON, uReg);

		pwrcal_writel(PHY_DVFS0_CON0, g_phy_dfs_table[target_mif_level_idx].DVFSn_CON0);
		pwrcal_writel(PHY_DVFS0_CON1, g_phy_dfs_table[target_mif_level_idx].DVFSn_CON1);
		pwrcal_writel(PHY_DVFS0_CON2, g_phy_dfs_table[target_mif_level_idx].DVFSn_CON2);
		pwrcal_writel(PHY_DVFS0_CON3, g_phy_dfs_table[target_mif_level_idx].DVFSn_CON3);
		pwrcal_writel(PHY_DVFS0_CON4, g_phy_dfs_table[target_mif_level_idx].DVFSn_CON4);

		mr13 = (0x1<<7)|(0x0<<6)|(0x0<<5);	//0x0 = FSP-WR[6], 0x1 = FSP_OP[7]
		smc_mode_register_write(DRAM_MR13, mr13);
		smc_mode_register_write(DRAM_MR1, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR1);
		smc_mode_register_write(DRAM_MR2, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR2);
		smc_mode_register_write(DRAM_MR3, g_dram_dfs_table[target_mif_level_idx].DirectCmd_MR3);

		mr13 &= ~(0x1<<7);	// clear FSP-OP[7]
		pwrcal_writel(MRS_DATA1, mr13);
	} else if (timing_set_idx == MIF_TIMING_SET_1) {
		pwrcal_writel(DMC_MISC_CON1, 0x1);	//timing_set_sw_r=0x1

		pwrcal_writel(DramTiming0_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming0);
		pwrcal_writel(DramTiming1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming1);
		pwrcal_writel(DramTiming2_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming2);
		pwrcal_writel(DramTiming3_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming3);
		pwrcal_writel(DramTiming4_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming4);
		pwrcal_writel(DramTiming5_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming5);
		pwrcal_writel(DramTiming6_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming6);
		pwrcal_writel(DramTiming7_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming7);
		pwrcal_writel(DramTiming8_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming8);
		pwrcal_writel(DramTiming9_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramTiming9);
		pwrcal_writel(DramDerateTiming0_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramDerateTiming0);
		pwrcal_writel(DramDerateTiming1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DramDerateTiming1);
		pwrcal_writel(Dimm0AutoRefTiming1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].Dimm0AutoRefTiming1);
		pwrcal_writel(Dimm1AutoRefTiming1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].Dimm1AutoRefTiming1);
		pwrcal_writel(AutoRefTiming2_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].AutoRefTiming2);
		pwrcal_writel(PwrMgmtTiming0_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].PwrMgmtTiming0);
		pwrcal_writel(PwrMgmtTiming1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].PwrMgmtTiming1);
		pwrcal_writel(PwrMgmtTiming2_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].PwrMgmtTiming2);
		pwrcal_writel(PwrMgmtTiming3_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].PwrMgmtTiming3);
		pwrcal_writel(TmrTrnInterval_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].TmrTrnInterval);
		pwrcal_writel(DFIDelay1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DFIDelay1);
		pwrcal_writel(DFIDelay2_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DFIDelay2);
		pwrcal_writel(DvfsTrnCtl_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].DvfsTrnCtl);
		pwrcal_writel(TrnTiming0_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].TrnTiming0);
		pwrcal_writel(TrnTiming1_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].TrnTiming1);
		pwrcal_writel(TrnTiming2_1, g_smc_dfs_table_switch[target_mif_level_switch_idx].TrnTiming2);

		uReg = pwrcal_readl(PHY_DVFS_CON_CH0);
		uReg &= ~(0x3<<30);
		uReg |= (0x2<<30);	//0x2 = DVFS 2 mode
		pwrcal_writel(PHY_DVFS_CON, uReg);

		pwrcal_writel(PHY_DVFS1_CON0, g_phy_dfs_table_switch[target_mif_level_switch_idx].DVFSn_CON0);
		pwrcal_writel(PHY_DVFS1_CON1, g_phy_dfs_table_switch[target_mif_level_switch_idx].DVFSn_CON1);
		pwrcal_writel(PHY_DVFS1_CON2, g_phy_dfs_table_switch[target_mif_level_switch_idx].DVFSn_CON2);
		pwrcal_writel(PHY_DVFS1_CON3, g_phy_dfs_table_switch[target_mif_level_switch_idx].DVFSn_CON3);
		pwrcal_writel(PHY_DVFS1_CON4, g_phy_dfs_table_switch[target_mif_level_switch_idx].DVFSn_CON4);

		mr13 = (0x0<<7)|(0x1<<6)|(0x0<<5);	//0x1 = FSP-WR[6], 0x0 = FSP_OP[7]
		smc_mode_register_write(DRAM_MR13, mr13);
		smc_mode_register_write(DRAM_MR1, g_dram_dfs_table_switch[target_mif_level_switch_idx].DirectCmd_MR1);
		smc_mode_register_write(DRAM_MR2, g_dram_dfs_table_switch[target_mif_level_switch_idx].DirectCmd_MR2);
		smc_mode_register_write(DRAM_MR3, g_dram_dfs_table_switch[target_mif_level_switch_idx].DirectCmd_MR3);

		mr13 &= ~(0x1<<7);	// clear FSP-OP[7]
		mr13 |= (0x1<<7);	// set FSP-OP[7]=0x1
		pwrcal_writel(MRS_DATA1, mr13);
	}	 else {
		pr_err("wrong DMC timing set selection on DVFS\n");
		return;
	}
}
