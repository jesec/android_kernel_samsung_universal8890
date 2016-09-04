/*
 * Customer HW 4 dependant file
 *
 * Copyright (C) 1999-2016, Broadcom Corporation
 * 
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 * 
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 * 
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 *
 * <<Broadcom-WL-IPTag/Open:>>
 *
 * $Id: dhd_custom_sec.c 334946 2012-05-24 20:38:00Z $
 */
#if defined(CUSTOMER_HW4) || defined(CUSTOMER_HW40)
#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>

#include <proto/ethernet.h>
#include <dngl_stats.h>
#include <bcmutils.h>
#include <dhd.h>
#include <dhd_dbg.h>
#include <dhd_linux.h>
#include <bcmdevs.h>

#include <linux/fcntl.h>
#include <linux/fs.h>

struct cntry_locales_custom {
	char iso_abbrev[WLC_CNTRY_BUF_SZ]; /* ISO 3166-1 country abbreviation */
	char custom_locale[WLC_CNTRY_BUF_SZ]; /* Custom firmware locale */
	int32 custom_locale_rev; /* Custom local revisin default -1 */
};

#if !defined(DHD_USE_CLMINFO_PARSER)
/* Locale table for sec */
const struct cntry_locales_custom translate_custom_table[] = {
#if defined(BCM4330_CHIP) || defined(BCM4334_CHIP) || defined(BCM43241_CHIP)
	/* 4330/4334/43241 */
	{"AR", "AR", 1},
	{"AT", "AT", 1},
	{"AU", "AU", 2},
	{"BE", "BE", 1},
	{"BG", "BG", 1},
	{"BN", "BN", 1},
	{"CA", "CA", 2},
	{"CH", "CH", 1},
	{"CY", "CY", 1},
	{"CZ", "CZ", 1},
	{"DE", "DE", 3},
	{"DK", "DK", 1},
	{"EE", "EE", 1},
	{"ES", "ES", 1},
	{"FI", "FI", 1},
	{"FR", "FR", 1},
	{"GB", "GB", 1},
	{"GR", "GR", 1},
	{"HR", "HR", 1},
	{"HU", "HU", 1},
	{"IE", "IE", 1},
	{"IS", "IS", 1},
	{"IT", "IT", 1},
	{"JP", "JP", 5},
	{"KR", "KR", 24},
	{"KW", "KW", 1},
	{"LI", "LI", 1},
	{"LT", "LT", 1},
	{"LU", "LU", 1},
	{"LV", "LV", 1},
	{"MT", "MT", 1},
	{"NL", "NL", 1},
	{"NO", "NO", 1},
	{"PL", "PL", 1},
	{"PT", "PT", 1},
	{"PY", "PY", 1},
	{"RO", "RO", 1},
	{"RU", "RU", 13},
	{"SE", "SE", 1},
	{"SI", "SI", 1},
	{"SK", "SK", 1},
	{"TW", "TW", 2},
#ifdef BCM4330_CHIP
	{"",   "XZ", 1},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 1},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 1},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 1},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 1},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 1},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 1},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 1},	/* Universal if Country code is MARSHALL ISLANDS */
	{"JO", "XZ", 1},	/* Universal if Country code is Jordan */
	{"PG", "XZ", 1},	/* Universal if Country code is Papua New Guinea */
	{"SA", "XZ", 1},	/* Universal if Country code is Saudi Arabia */
	{"AF", "XZ", 1},	/* Universal if Country code is Afghanistan */
	{"US", "US", 5},
	{"UA", "UY", 0},
	{"AD", "AL", 0},
	{"CX", "AU", 2},
	{"GE", "GB", 1},
	{"ID", "MW", 0},
	{"KI", "AU", 2},
	{"NP", "SA", 0},
	{"WS", "SA", 0},
	{"LR", "BR", 0},
	{"ZM", "IN", 0},
	{"AN", "AG", 0},
	{"AI", "AS", 0},
	{"BM", "AS", 0},
	{"DZ", "GB", 1},
	{"LC", "AG", 0},
	{"MF", "BY", 0},
	{"GY", "CU", 0},
	{"LA", "GB", 1},
	{"LB", "BR", 0},
	{"MA", "IL", 0},
	{"MO", "BD", 0},
	{"MW", "BD", 0},
	{"QA", "BD", 0},
	{"TR", "GB", 1},
	{"TZ", "BF", 0},
	{"VN", "BR", 0},
	{"AE", "AZ", 0},
	{"IQ", "GB", 1},
	{"CN", "CL", 0},
	{"MX", "MX", 1},
#else
	/* 4334/43241 */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"GL", "XZ", 11},	/* Universal if Country code is GREENLAND */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"US", "US", 46},
	{"UA", "UA", 8},
	{"CO", "CO", 4},
	{"ID", "ID", 1},
	{"LA", "LA", 1},
	{"LB", "LB", 2},
	{"VN", "VN", 4},
	{"MA", "MA", 1},
	{"TR", "TR", 7},
#endif /* defined(BCM4330_CHIP) */
#ifdef BCM4334_CHIP
	{"AE", "AE", 1},
	{"MX", "MX", 1},
#endif /* defined(BCM4334_CHIP) */
#ifdef BCM43241_CHIP
	{"AE", "AE", 6},
	{"BD", "BD", 2},
	{"CN", "CN", 38},
	{"MX", "MX", 20},
#endif /* defined(BCM43241_CHIP) */
#else  /* defined(BCM4330_CHIP) || defined(BCM4334_CHIP) || defined(BCM43241_CHIP) */
	/* default ccode/regrev */
	{"",   "XZ", 11},	/* Universal if Country code is unknown or empty */
	{"IR", "XZ", 11},	/* Universal if Country code is IRAN, (ISLAMIC REPUBLIC OF) */
	{"SD", "XZ", 11},	/* Universal if Country code is SUDAN */
	{"SY", "XZ", 11},	/* Universal if Country code is SYRIAN ARAB REPUBLIC */
	{"PS", "XZ", 11},	/* Universal if Country code is PALESTINIAN TERRITORY, OCCUPIED */
	{"TL", "XZ", 11},	/* Universal if Country code is TIMOR-LESTE (EAST TIMOR) */
	{"MH", "XZ", 11},	/* Universal if Country code is MARSHALL ISLANDS */
	{"GL", "GP", 2},
	{"AL", "AL", 2},
#ifdef DHD_SUPPORT_GB_999
	{"DZ", "GB", 999},
#else
	{"DZ", "GB", 6},
#endif /* DHD_SUPPORT_GB_999 */
	{"AS", "AS", 12},
	{"AI", "AI", 1},
	{"AF", "AD", 0},
	{"AG", "AG", 2},
	{"AR", "AU", 6},
	{"AW", "AW", 2},
	{"AU", "AU", 6},
	{"AT", "AT", 4},
	{"AZ", "AZ", 2},
	{"BS", "BS", 2},
	{"BH", "BH", 4},
	{"BD", "BD", 1},
	{"BY", "BY", 3},
	{"BE", "BE", 4},
	{"BM", "BM", 12},
	{"BA", "BA", 2},
	{"BR", "BR", 2},
	{"VG", "VG", 2},
	{"BN", "BN", 4},
	{"BG", "BG", 4},
	{"KH", "KH", 2},
	{"KY", "KY", 3},
	{"CN", "CN", 38},
	{"CO", "CO", 17},
	{"CR", "CR", 17},
	{"HR", "HR", 4},
	{"CY", "CY", 4},
	{"CZ", "CZ", 4},
	{"DK", "DK", 4},
	{"EE", "EE", 4},
	{"ET", "ET", 2},
	{"FI", "FI", 4},
	{"FR", "FR", 5},
	{"GF", "GF", 2},
	{"DE", "DE", 7},
	{"GR", "GR", 4},
	{"GD", "GD", 2},
	{"GP", "GP", 2},
	{"GU", "GU", 30},
	{"HK", "HK", 2},
	{"HU", "HU", 4},
	{"IS", "IS", 4},
	{"IN", "IN", 3},
	{"ID", "ID", 1},
	{"IE", "IE", 5},
	{"IL", "IL", 14},
	{"IT", "IT", 4},
	{"JO", "JO", 3},
	{"KE", "SA", 0},
	{"KW", "KW", 5},
	{"LA", "LA", 2},
	{"LV", "LV", 4},
	{"LB", "LB", 5},
	{"LS", "LS", 2},
	{"LI", "LI", 4},
	{"LT", "LT", 4},
	{"LU", "LU", 3},
	{"MO", "SG", 0},
	{"MK", "MK", 2},
	{"MW", "MW", 1},
	{"MY", "MY", 3},
	{"MV", "MV", 3},
	{"MT", "MT", 4},
	{"MQ", "MQ", 2},
	{"MR", "MR", 2},
	{"MU", "MU", 2},
	{"YT", "YT", 2},
	{"MX", "MX", 44},
	{"MD", "MD", 2},
	{"MC", "MC", 1},
	{"ME", "ME", 2},
	{"MA", "MA", 2},
	{"NL", "NL", 4},
	{"AN", "GD", 2},
	{"NZ", "NZ", 4},
	{"NO", "NO", 4},
	{"OM", "OM", 4},
	{"PA", "PA", 17},
	{"PG", "AU", 6},
	{"PY", "PY", 2},
	{"PE", "PE", 20},
	{"PH", "PH", 5},
	{"PL", "PL", 4},
	{"PT", "PT", 4},
	{"PR", "PR", 38},
	{"RE", "RE", 2},
	{"RO", "RO", 4},
	{"SN", "MA", 2},
	{"RS", "RS", 2},
	{"SK", "SK", 4},
	{"SI", "SI", 4},
	{"ES", "ES", 4},
	{"LK", "LK", 1},
	{"SE", "SE", 4},
	{"CH", "CH", 4},
	{"TW", "TW", 1},
	{"TH", "TH", 5},
	{"TT", "TT", 3},
	{"TR", "TR", 7},
	{"AE", "AE", 6},
#ifdef DHD_SUPPORT_GB_999
	{"GB", "GB", 999},
#else
	{"GB", "GB", 6},
#endif /* DHD_SUPPORT_GB_999 */
	{"UY", "VE", 3},
	{"VI", "PR", 38},
	{"VA", "VA", 2},
	{"VE", "VE", 3},
	{"VN", "VN", 4},
	{"ZM", "LA", 2},
	{"EC", "EC", 21},
	{"SV", "SV", 25},
#if defined(BCM4358_CHIP) || defined(BCM4359_CHIP)
	{"KR", "KR", 70},
#else
	{"KR", "KR", 48},
#endif
	{"JP", "JP", 968},
	{"RU", "RU", 986},
	{"UA", "UA", 16},
	{"GT", "GT", 1},
	{"MN", "MN", 1},
	{"NI", "NI", 2},
	{"UZ", "MA", 2},
	{"ZA", "ZA", 6},
	{"EG", "EG", 13},
	{"TN", "TN", 1},
	{"AO", "AD", 0},
	{"BT", "BJ", 0},
	{"BW", "BJ", 0},
	{"LY", "LI", 4},
	{"BO", "NG", 0},
	{"UM", "PR", 38},
	/* Support FCC 15.407 (Part 15E) Changes, effective June 2 2014 */
	/* US/988, Q2/993 country codes with higher power on UNII-1 5G band */
#if defined(DHD_SUPPORT_US_949)
	{"US", "US", 949},
#elif defined(DHD_SUPPORT_US_945)
	{"US", "US", 945},
#else
	{"US", "US", 988},
#endif
	{"CU", "US", 988},
	{"CA", "Q2", 993},
#endif /* default ccode/regrev */
};
#else
struct cntry_locales_custom translate_custom_table[NUM_OF_COUNTRYS];
#endif /* DHD_USE_CLMINFO_PARSER */

/* Customized Locale convertor
*  input : ISO 3166-1 country abbreviation
*  output: customized cspec
*/
void get_customized_country_code(void *adapter, char *country_iso_code, wl_country_t *cspec)
{
	int size, i;

	size = ARRAYSIZE(translate_custom_table);

	if (cspec == 0)
		 return;

	if (size == 0)
		 return;

	for (i = 0; i < size; i++) {
		if (strcmp(country_iso_code, translate_custom_table[i].iso_abbrev) == 0) {
			memcpy(cspec->ccode,
				translate_custom_table[i].custom_locale, WLC_CNTRY_BUF_SZ);
			cspec->rev = translate_custom_table[i].custom_locale_rev;
			return;
		}
	}
	return;
}

#ifdef PLATFORM_SLP
#define PSMINFO "/opt/etc/.psm.info"
#define REVINFO "/opt/etc/.rev"
#define WIFIVERINFO "/opt/etc/.wifiver.info"
#define ANTINFO "/opt/etc/.ant.info"
#define RSDBINFO "/opt/etc/.rsdb.info"
#define LOGTRACEINFO "/opt/etc/.logtrace.info"
#else
#define PSMINFO "/data/.psm.info"
#define	REVINFO "/data/.rev"
#define WIFIVERINFO "/data/.wifiver.info"
#define ANTINFO "/data/.ant.info"
#define RSDBINFO "/data/.rsdb.info"
#define LOGTRACEINFO "/data/.logtrace.info"
#endif /* PLATFORM_SLP */

#ifdef DHD_PM_CONTROL_FROM_FILE
extern bool g_pm_control;
void sec_control_pm(dhd_pub_t *dhd, uint *power_mode)
{
	struct file *fp = NULL;
	char *filepath = PSMINFO;
	char power_val = 0;
	char iovbuf[WL_EVENTING_MASK_LEN + 12];
	int ret = 0;
#ifdef DHD_ENABLE_LPC
	uint32 lpc = 0;
#endif /* DHD_ENABLE_LPC */

	g_pm_control = FALSE;

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp) || (fp == NULL)) {
		/* Enable PowerSave Mode */
		dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
			sizeof(uint), TRUE, 0);
		DHD_ERROR(("[WIFI_SEC] %s: /data/.psm.info doesn't exist"
			" so set PM to %d\n",
			__FUNCTION__, *power_mode));
		return;
	} else {
		kernel_read(fp, fp->f_pos, &power_val, 1);
		DHD_ERROR(("[WIFI_SEC] %s: POWER_VAL = %c \r\n", __FUNCTION__, power_val));

		if (power_val == '0') {
#ifdef ROAM_ENABLE
			uint roamvar = 1;
#endif
			uint32 ocl_enable = 0;
			uint32 wl_updown = 1;

			*power_mode = PM_OFF;
			/* Disable PowerSave Mode */
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
				sizeof(uint), TRUE, 0);
			/* Turn off MPC in AP mode */
			bcm_mkiovar("mpc", (char *)power_mode, 4,
				iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0);
			g_pm_control = TRUE;
#ifdef ROAM_ENABLE
			/* Roaming off of dongle */
			bcm_mkiovar("roam_off", (char *)&roamvar, 4,
				iovbuf, sizeof(iovbuf));
			dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0);
#endif
#ifdef DHD_ENABLE_LPC
			/* Set lpc 0 */
			bcm_mkiovar("lpc", (char *)&lpc, 4, iovbuf, sizeof(iovbuf));
			if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
				sizeof(iovbuf), TRUE, 0)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: Set lpc failed  %d\n",
				__FUNCTION__, ret));
			}
#endif /* DHD_ENABLE_LPC */
#ifdef DHD_PCIE_RUNTIMEPM
			DHD_ERROR(("[WIFI_SEC] %s : Turn Runtime PM off \n", __FUNCTION__));
			/* Turn Runtime PM off */
			dhdpcie_block_runtime_pm(dhd);
#endif /* DHD_PCIE_RUNTIMEPM */
			/* Disable ocl */
			if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_UP, (char *)&wl_updown,
					sizeof(wl_updown), TRUE, 0)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: WLC_UP faield %d\n", __FUNCTION__, ret));
			}

			bcm_mkiovar("ocl_enable", (char *)&ocl_enable, 4, iovbuf, sizeof(iovbuf));
			if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf,
					sizeof(iovbuf), TRUE, 0)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: Set ocl_enable %d failed %d\n",
						__FUNCTION__, ocl_enable, ret));
			} else {
				DHD_ERROR(("[WIFI_SEC] %s: Set ocl_enable %d succeeded %d\n",
						__FUNCTION__, ocl_enable, ret));
			}

			if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_DOWN, (char *)&wl_updown,
					sizeof(wl_updown), TRUE, 0)) < 0) {
				DHD_ERROR(("[WIFI_SEC] %s: WLC_DOWN faield %d\n",
						__FUNCTION__, ret));
			}
		} else {
			dhd_wl_ioctl_cmd(dhd, WLC_SET_PM, (char *)power_mode,
				sizeof(uint), TRUE, 0);
		}
	}

	if (fp)
		filp_close(fp, NULL);
}
#endif /* DHD_PM_CONTROL_FROM_FILE */

#ifdef MIMO_ANT_SETTING
int dhd_sel_ant_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = -1;
	uint32 ant_val = 0;
	uint32 btc_mode = 0;
	uint32 rsdb_mode = 0;
	char *filepath = ANTINFO;
	char iovbuf[WLC_IOCTL_SMLEN];
	uint chip_id = dhd_bus_chip_id(dhd);

	/* Check if this chip can support MIMO */
	if (chip_id != BCM4324_CHIP_ID &&
		chip_id != BCM4350_CHIP_ID &&
		chip_id != BCM4354_CHIP_ID &&
		chip_id != BCM4356_CHIP_ID &&
		chip_id != BCM43569_CHIP_ID &&
		chip_id != BCM4358_CHIP_ID &&
		chip_id != BCM4359_CHIP_ID &&
		chip_id != BCM4355_CHIP_ID) {
		DHD_ERROR(("[WIFI_SEC] %s: This chipset does not support MIMO\n",
			__FUNCTION__));
		return ret;
	}

	/* Read antenna settings from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
		return ret;
	} else {
		ret = kernel_read(fp, 0, (char *)&ant_val, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return ret;
		}

		ant_val = bcm_atoi((char *)&ant_val);

		DHD_ERROR(("[WIFI_SEC]%s: ANT val = %d\n", __FUNCTION__, ant_val));
		filp_close(fp, NULL);

		/* Check value from the file */
		if (ant_val < 1 || ant_val > 3) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, ant_val, filepath));
			return -1;
		}
	}

	/* bt coex mode off */
	if (dhd_get_fw_mode(dhd->info) == DHD_FLAG_MFG_MODE) {
		bcm_mkiovar("btc_mode", (char *)&btc_mode, 4, iovbuf, sizeof(iovbuf));
		ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
		if (ret) {
			DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): "
				"btc_mode, ret=%d\n",
				__FUNCTION__, ret));
			return ret;
		}
	}

	/* rsdb mode off */
	DHD_ERROR(("[WIFI_SEC] %s: %s the RSDB mode!\n",
		__FUNCTION__, rsdb_mode ? "Enable" : "Disable"));
	bcm_mkiovar("rsdb_mode", (char *)&rsdb_mode, 4, iovbuf, sizeof(iovbuf));
	ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
	if (ret) {
		DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): "
			"rsdb_mode, ret=%d\n", __FUNCTION__, ret));
		return ret;
	}

	/* Select Antenna */
	bcm_mkiovar("txchain", (char *)&ant_val, 4, iovbuf, sizeof(iovbuf));
	ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
	if (ret) {
		DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): txchain, ret=%d\n",
			__FUNCTION__, ret));
		return ret;
	}

	bcm_mkiovar("rxchain", (char *)&ant_val, 4, iovbuf, sizeof(iovbuf));
	ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
	if (ret) {
		DHD_ERROR(("[WIFI_SEC] %s: Fail to execute dhd_wl_ioctl_cmd(): rxchain, ret=%d\n",
			__FUNCTION__, ret));
		return ret;
	}

	return 0;
}
#endif /* MIMO_ANTENNA_SETTING */

#ifdef RSDB_MODE_FROM_FILE
/*
 * RSDBOFFINFO = /data/.rsdb.info
 *  - rsdb_mode = 1            => Don't change RSDB mode / RSDB stay as turn on
 *  - rsdb_mode = 0            => Trun Off RSDB mode
 *  - file not exist          => Don't change RSDB mode / RSDB stay as turn on
 */
int dhd_rsdb_mode_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = -1;
	uint32 rsdb_mode = 0;
	char *filepath = RSDBINFO;
	char iovbuf[WLC_IOCTL_SMLEN];

	/* Read RSDB on/off request from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
		return ret;
	} else {
		ret = kernel_read(fp, 0, (char *)&rsdb_mode, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return ret;
		}

		rsdb_mode = bcm_atoi((char *)&rsdb_mode);

		DHD_ERROR(("[WIFI_SEC] %s: RSDB mode from file = %d\n", __FUNCTION__, rsdb_mode));
		filp_close(fp, NULL);

		/* Check value from the file */
		if (rsdb_mode > 2) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, rsdb_mode, filepath));
			return -1;
		}
	}

	if (rsdb_mode == 0) {
		bcm_mkiovar("rsdb_mode", (char *)&rsdb_mode, sizeof(rsdb_mode),
			iovbuf, sizeof(iovbuf));

		if ((ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR,
				iovbuf, sizeof(iovbuf), TRUE, 0)) < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: rsdb_mode ret= %d\n", __FUNCTION__, ret));
		} else {
			DHD_ERROR(("[WIFI_SEC] %s: rsdb_mode to MIMO(RSDB OFF) succeeded\n",
				__FUNCTION__));
		}
	}

	return ret;
}
#endif /* RSDB_MODE_FROM_FILE */

#ifdef LOGTRACE_FROM_FILE
/*
 * LOGTRACEINFO = /data/.logtrace.info
 *  - logtrace = 1            => Enable LOGTRACE Event
 *  - logtrace = 0            => Disable LOGTRACE Event
 *  - file not exist          => Disable LOGTRACE Event
 */
int dhd_logtrace_from_file(dhd_pub_t *dhd)
{
	struct file *fp = NULL;
	int ret = -1;
	uint32 logtrace = 0;
	char *filepath = LOGTRACEINFO;

	/* Read LOGTRACE Event on/off request from the file */
	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist\n", __FUNCTION__, filepath));
		return 0;
	} else {
		ret = kernel_read(fp, 0, (char *)&logtrace, 4);
		if (ret < 0) {
			DHD_ERROR(("[WIFI_SEC] %s: File read error, ret=%d\n", __FUNCTION__, ret));
			filp_close(fp, NULL);
			return 0;
		}

		logtrace = bcm_atoi((char *)&logtrace);

		DHD_ERROR(("[WIFI_SEC] %s: LOGTRACE On/Off from file = %d\n",
			__FUNCTION__, logtrace));
		filp_close(fp, NULL);

		/* Check value from the file */
		if (logtrace > 2) {
			DHD_ERROR(("[WIFI_SEC] %s: Invalid value %d read from the file %s\n",
				__FUNCTION__, logtrace, filepath));
			return 0;
		}
	}

	return (int)logtrace;
}
#endif /* LOGTRACE_FROM_FILE */

#ifdef USE_WFA_CERT_CONF
int sec_get_param_wfa_cert(dhd_pub_t *dhd, int mode, uint* read_val)
{
	struct file *fp = NULL;
	char *filepath = NULL;
	int val = 0;

	if (!dhd || (mode < SET_PARAM_BUS_TXGLOM_MODE) ||
		(mode >= PARAM_LAST_VALUE)) {
		DHD_ERROR(("[WIFI_SEC] %s: invalid argument\n", __FUNCTION__));
		return BCME_ERROR;
	}

	switch (mode) {
		case SET_PARAM_BUS_TXGLOM_MODE:
			filepath = "/data/.bustxglom.info";
			break;
		case SET_PARAM_ROAMOFF:
			filepath = "/data/.roamoff.info";
			break;
#ifdef USE_WL_FRAMEBURST
		case SET_PARAM_FRAMEBURST:
			filepath = "/data/.frameburst.info";
			break;
#endif /* USE_WL_FRAMEBURST */
#ifdef USE_WL_TXBF
		case SET_PARAM_TXBF:
			filepath = "/data/.txbf.info";
			break;
#endif /* USE_WL_TXBF */
#ifdef PROP_TXSTATUS
		case SET_PARAM_PROPTX:
			filepath = "/data/.proptx.info";
			break;
#endif /* PROP_TXSTATUS */
		default:
			DHD_ERROR(("[WIFI_SEC] %s: File to find file name for index=%d\n",
				__FUNCTION__, mode));
			return BCME_ERROR;
	}

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: File [%s] doesn't exist \n",
			__FUNCTION__, filepath));
		return BCME_ERROR;
	} else {
		if (kernel_read(fp, fp->f_pos, (char *)&val, 4) < 0) {
			filp_close(fp, NULL);
			/* File operation is failed so we will return error code */
			DHD_ERROR(("[WIFI_SEC] %s: read failed, file path=%s\n",
				__FUNCTION__, filepath));
			return BCME_ERROR;
		}
		filp_close(fp, NULL);
	}

	val = bcm_atoi((char *)&val);

	switch (mode) {
		case SET_PARAM_ROAMOFF:
#ifdef USE_WL_FRAMEBURST
		case SET_PARAM_FRAMEBURST:
#endif /* USE_WL_FRAMEBURST */
#ifdef USE_WL_TXBF
		case SET_PARAM_TXBF:
#endif /* USE_WL_TXBF */
#ifdef PROP_TXSTATUS
		case SET_PARAM_PROPTX:
#endif /* PROP_TXSTATUS */
		if (val < 0 || val > 1) {
			DHD_ERROR(("[WIFI_SEC] %s: value[%d] is out of range\n",
				__FUNCTION__, *read_val));
			return BCME_ERROR;
		}
			break;
		default:
			return BCME_ERROR;
	}
	*read_val = (uint)val;
	return BCME_OK;
}
#endif /* USE_WFA_CERT_CONF */

#ifdef WRITE_WLANINFO
#define FIRM_PREFIX "Firm_ver:"
#define DHD_PREFIX "DHD_ver:"
#define NV_PREFIX "Nv_info:"
#define CLM_PREFIX "CLM_ver:"
#define max_len(a, b) ((sizeof(a)/(2)) - (strlen(b)) - (3))
#define tstr_len(a, b) ((strlen(a)) + (strlen(b)) + (3))

char version_info[512];
char version_old_info[512];

int write_filesystem(struct file *file, unsigned long long offset,
	unsigned char* data, unsigned int size)
{
	mm_segment_t oldfs;
	int ret;

	oldfs = get_fs();
	set_fs(get_ds());

	ret = vfs_write(file, data, size, &offset);

	set_fs(oldfs);
	return ret;
}

uint32 sec_save_wlinfo(char *firm_ver, char *dhd_ver, char *nvram_p, char *clm_ver)
{
	struct file *fp = NULL;
	struct file *nvfp = NULL;
	char *filepath = WIFIVERINFO;
	int min_len, str_len = 0;
	int ret = 0;
	char* nvram_buf;
	char temp_buf[256];

	DHD_TRACE(("[WIFI_SEC] %s: Entered.\n", __FUNCTION__));

	DHD_INFO(("[WIFI_SEC] firmware version   : %s\n", firm_ver));
	DHD_INFO(("[WIFI_SEC] dhd driver version : %s\n", dhd_ver));
	DHD_INFO(("[WIFI_SEC] nvram path : %s\n", nvram_p));
	DHD_INFO(("[WIFI_SEC] clm version : %s\n", clm_ver));

	memset(version_info, 0, sizeof(version_info));

	if (strlen(dhd_ver)) {
		min_len = min(strlen(dhd_ver), max_len(temp_buf, DHD_PREFIX));
		min_len += strlen(DHD_PREFIX) + 3;
		DHD_INFO(("[WIFI_SEC] DHD ver length : %d\n", min_len));
		snprintf(version_info+str_len, min_len, DHD_PREFIX " %s\n", dhd_ver);
		str_len = strlen(version_info);

		DHD_INFO(("[WIFI_SEC] Driver version_info len : %d\n", str_len));
		DHD_INFO(("[WIFI_SEC] Driver version_info : %s\n", version_info));
	} else {
		DHD_ERROR(("[WIFI_SEC] Driver version is missing.\n"));
	}

	if (strlen(firm_ver)) {
		min_len = min(strlen(firm_ver), max_len(temp_buf, FIRM_PREFIX));
		min_len += strlen(FIRM_PREFIX) + 3;
		DHD_INFO(("[WIFI_SEC] firmware ver length : %d\n", min_len));
		snprintf(version_info+str_len, min_len, FIRM_PREFIX " %s\n", firm_ver);
		str_len = strlen(version_info);

		DHD_INFO(("[WIFI_SEC] Firmware version_info len : %d\n", str_len));
		DHD_INFO(("[WIFI_SEC] Firmware version_info : %s\n", version_info));
	} else {
		DHD_ERROR(("[WIFI_SEC] Firmware version is missing.\n"));
	}

	if (nvram_p) {
		memset(temp_buf, 0, sizeof(temp_buf));
		nvfp = filp_open(nvram_p, O_RDONLY, 0);
		if (IS_ERR(nvfp) || (nvfp == NULL)) {
			DHD_ERROR(("[WIFI_SEC] %s: Nvarm File open failed.\n", __FUNCTION__));
			return -1;
		} else {
			ret = kernel_read(nvfp, nvfp->f_pos, temp_buf, sizeof(temp_buf));
			filp_close(nvfp, NULL);
		}

		if (strlen(temp_buf)) {
			nvram_buf = temp_buf;
			bcmstrtok(&nvram_buf, "\n", 0);
			DHD_INFO(("[WIFI_SEC] nvram tolkening : %s(%zu) \n",
				temp_buf, strlen(temp_buf)));
			snprintf(version_info+str_len, tstr_len(temp_buf, NV_PREFIX),
				NV_PREFIX " %s\n", temp_buf);
			str_len = strlen(version_info);
			DHD_INFO(("[WIFI_SEC] NVRAM version_info : %s\n", version_info));
			DHD_INFO(("[WIFI_SEC] NVRAM version_info len : %d, nvram len : %zu\n",
				str_len, strlen(temp_buf)));
		} else {
			DHD_ERROR(("[WIFI_SEC] NVRAM info is missing.\n"));
		}
	} else {
		DHD_ERROR(("[WIFI_SEC] Not exist nvram path\n"));
	}

	if (strlen(clm_ver)) {
		min_len = min(strlen(clm_ver), max_len(temp_buf, CLM_PREFIX));
		min_len += strlen(CLM_PREFIX) + 3;
		DHD_INFO(("[WIFI_SEC] clm ver length : %d\n", min_len));
		snprintf(version_info+str_len, min_len, CLM_PREFIX " %s\n", clm_ver);
		str_len = strlen(version_info);

		DHD_INFO(("[WIFI_SEC] CLM version_info len : %d\n", str_len));
		DHD_INFO(("[WIFI_SEC] CLM version_info : %s\n", version_info));
	} else {
		DHD_ERROR(("[WIFI_SEC] CLM version is missing.\n"));
	}

	DHD_INFO(("[WIFI_SEC] version_info : %s, strlen : %zu\n",
		version_info, strlen(version_info)));

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: .wifiver.info File open failed.\n", __FUNCTION__));
	} else {
		memset(version_old_info, 0, sizeof(version_old_info));
		ret = kernel_read(fp, fp->f_pos, version_old_info, sizeof(version_info));
		filp_close(fp, NULL);
		DHD_INFO(("[WIFI_SEC] kernel_read ret : %d.\n", ret));
		if (strcmp(version_info, version_old_info) == 0) {
			DHD_ERROR(("[WIFI_SEC] .wifiver.info already saved.\n"));
			return 0;
		}
	}

	fp = filp_open(filepath, O_RDWR | O_CREAT, 0664);
	if (IS_ERR(fp) || (fp == NULL)) {
		DHD_ERROR(("[WIFI_SEC] %s: .wifiver.info File open failed.\n",
			__FUNCTION__));
	} else {
		ret = write_filesystem(fp, fp->f_pos, version_info, sizeof(version_info));
		DHD_INFO(("[WIFI_SEC] sec_save_wlinfo done. ret : %d\n", ret));
		DHD_ERROR(("[WIFI_SEC] save .wifiver.info file.\n"));
		filp_close(fp, NULL);
	}
	return ret;
}
#endif /* WRITE_WLANINFO */
#endif /* CUSTOMER_HW4 || CUSTOMER_HW40 */

#if defined(FORCE_DISABLE_SINGLECORE_SCAN)
void
dhd_force_disable_singlcore_scan(dhd_pub_t *dhd)
{
	int ret = 0;
	struct file *fp = NULL;
	char *filepath = "/data/.cid.info";
	s8 iovbuf[WL_EVENTING_MASK_LEN + 12];
	char vender[10] = {0, };
	uint32 pm_bcnrx = 0;
	uint32 scan_ps = 0;

	if (BCM4354_CHIP_ID != dhd_bus_chip_id(dhd))
		return;

	fp = filp_open(filepath, O_RDONLY, 0);
	if (IS_ERR(fp)) {
		DHD_ERROR(("/data/.cid.info file open error\n"));
	} else {
		ret = kernel_read(fp, 0, (char *)vender, 5);

		if (ret > 0 && NULL != strstr(vender, "wisol")) {
			DHD_ERROR(("wisol module : set pm_bcnrx=0, set scan_ps=0\n"));

			bcm_mkiovar("pm_bcnrx", (char *)&pm_bcnrx, 4, iovbuf, sizeof(iovbuf));
			ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
			if (ret < 0)
				DHD_ERROR(("Set pm_bcnrx error (%d)\n", ret));

			bcm_mkiovar("scan_ps", (char *)&scan_ps, 4, iovbuf, sizeof(iovbuf));
			ret = dhd_wl_ioctl_cmd(dhd, WLC_SET_VAR, iovbuf, sizeof(iovbuf), TRUE, 0);
			if (ret < 0)
				DHD_ERROR(("Set scan_ps error (%d)\n", ret));
		}
		filp_close(fp, NULL);
	}
}
#endif /* FORCE_DISABLE_SINGLECORE_SCAN */
