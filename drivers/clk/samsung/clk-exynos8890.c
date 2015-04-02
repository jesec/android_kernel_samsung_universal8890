/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This file contains clocks of Exynos8890.
 */

#include <linux/of.h>

#include <dt-bindings/clock/exynos8890.h>
#include "../../soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"
#include "composite.h"

enum exynos8890_clks {
	none,

	oscclk = 1,

	/* number for mfc driver starts from 10 */
	mfc_hpm = 10,

	/* number for mscl driver starts from 50 */
	mscl0 = 50,

	/* number for imem driver starts from 100 */
	gate_apm = 100, gate_sss, gate_gic400,

	/* number for peris driver starts from 150 */
	peris_sfr = 150, peris_hpm, peris_mct, wdt_mngs, wdt_apollo, sysreg_peris, monocnt_apbif, rtc_apbif, top_rtc, otp_con_top, peris_chipid, peris_tmu,

	/* number for peric0 driver starts from 200 */
	gate_hsi2c0 = 200, gate_hsi2c1, gate_hsi2c4, gate_hsi2c5, gate_hsi2c9, gate_hsi2c10, gate_hsi2c11, puart0, suart0, gate_adcif, gate_pwm, gate_sclk_pwm,

	/* number for peric1 driver starts from 250 */
	gate_hsi2c2 = 250, gate_hsi2c3, gate_hsi2c6, gate_hsi2c7, gate_hsi2c8, gate_hsi2c12, gate_hsi2c13, gate_hsi2c14,
	gate_uart1, gate_uart2, gate_uart3, gate_uart4, gate_uart5, suart1, suart2, suart3, suart4, suart5,
	gate_spi0, gate_spi1, gate_spi2, gate_spi3, gate_spi4, gate_spi5, gate_spi6, gate_spi7,
	sclk_peric1_spi0, sclk_peric1_spi1, sclk_peric1_spi2, sclk_peric1_spi3, sclk_peric1_spi4, sclk_peric1_spi5, sclk_peric1_spi6, sclk_peric1_spi7,
	gate_gpio_nfc, gate_gpio_touch, gate_gpio_fp, gate_gpio_ese, promise_int, promise_disp, ap2cp_mif_pll_out,

	/* number for isp0 driver starts from 400 */
	gate_fimc_isp0 = 400, gate_fimc_tpu, isp0, isp0_tpu, isp0_trex,

	/* number for isp1 driver starts from 450 */
	gate_fimc_isp1 = 450, isp1,

	/* number for isp sensor driver starts from 500 */
	isp_sensor0 = 500, isp_sensor1, isp_sensor2, isp_sensor3,

	/* number for cam0 driver starts from 550 */
	gate_csis0 = 550, gate_csis1, gate_fimc_bns, fimc_3aa0, fimc_3aa1, cam0_hpm, pxmxdx_csis0, pxmxdx_csis1, pxmxdx_csis2, pxmxdx_csis3,
	pxmxdx_3aa0, pxmxdx_3aa1, pxmxdx_trex, hs0_csis0_rx_byte, hs1_csis0_rx_byte, hs2_csis0_rx_byte, hs3_csis0_rx_byte, hs0_csis1_rx_byte, hs1_csis1_rx_byte,

	/* number for cam1 driver starts from 600 */
	gate_isp_cpu = 600, gate_csis2, gate_csis3, gate_fimc_vra, gate_mc_scaler, gate_i2c0_isp, gate_i2c1_isp, gate_i2c2_isp, gate_i2c3_isp, gate_wdt_isp,
	gate_mcuctl_isp, gate_uart_isp, gate_pdma_isp, gate_pwm_isp, gate_spi0_isp, gate_spi1_isp, isp_spi0, isp_spi1, isp_uart, gate_sclk_pwm_isp,
	gate_sclk_uart_isp,

	/* number for audio driver starts from 650 */
	gate_mi2s = 650, gate_pcm, gate_slimbus, gate_sclk_mi2s, d1_sclk_i2s, gate_sclk_pcm, d1_sclk_pcm, gate_sclk_slimbus, sclk_slimbus, sclk_cp_i2s,
	sclk_asrc, aud_pll, aud_cp,

	/* number for fsys0 driver starts from 700 */
	gate_usbdrd30 = 700, gate_usbhost20, usbdrd30 = 703, sclk_fsys0_mmc0, ufsunipro20, phy24m, ufsunipro_cfg, gate_udrd30_phyclock, gate_udrd30_pipe, gate_ufs_tx0,
	gate_ufs_rx0, usbhost20_phyclock, usbhost20_freeclk, usbhost20_clk48, usbhost20phy_ref, ufs_rx_pwm, ufs_tx_pwm, ufs_refclk_out, fsys_200,

	/* number for fsys1 driver starts from 750 */
	gate_ufs20_sdcard = 750, fsys1_hpm, fsys1_sclk_mmc2, ufsunipro20_sdcard, pcie_phy, sclk_ufsunipro_sdcard, ufs_link_sdcard_tx0, ufs_link_sdcard_rx0,
	pcie_wifi0_tx0, pcie_wifi0_rx0, pcie_wifi1_tx0, pcie_wifi1_rx0, wifi0_dig_refclk, wifi1_dig_refclk, sdcard_rx_pwm, sdcard_tx_pwm, sdcard_refclk,

	/* number for g3d driver starts from 800 */
	gate_g3d = 800, gate_g3d_iram,

	/* number for disp0 driver starts from 850 */
	gate_decon0 = 850, gate_dsim0, gate_dsim1, gate_dsim2, gate_hdmi, gate_dp, gate_hpm_apbif_disp0, decon0_eclk0, decon0_vclk0, decon0_vclk1,
	decon0_eclk0_local, decon0_vclk0_local, decon0_vclk1_local, hdmi_audio, disp_pll,
	mipidphy0_rxclkesc0, mipidphy0_bitclkdiv8, mipidphy1_rxclkesc0, mipidphy1_bitclkdiv8, mipidphy2_rxclkesc0, mipidphy2_bitclkdiv8,
	dpphy_ch0_txd, dpphy_ch1_txd, dpphy_ch2_txd, dpphy_ch3_txd, dptx_phy_i_ref_clk_24m,
	mipi_dphy_m1s0, mipi_dphy_m4s0, mipi_dphy_m4s4, phyclk_hdmiphy_tmds_20b, phyclk_hdmiphy_pixel, mipidphy0_bitclkdiv2_user,
	mipidphy1_bitclkdiv2_user, mipidphy2_bitclkdiv2_user,

	/* number for disp1 driver starts from 900 */
	gate_decon1 = 900, gate_hpmdisp1, decon1_eclk0, decon1_eclk1, decon1_eclk0_local, decon1_eclk1_local,
	disp1_phyclk_mipidphy0_bitclkdiv2_user, disp1_phyclk_mipidphy1_bitclkdiv2_user,
	disp1_phyclk_mipidphy2_bitclkdiv2_user, disp1_phyclk_disp1_hdmiphy_pixel_clko_user,

	/* number for ccore driver starts from 950 */
	ccore_i2c = 950,

	nr_clks,
};

static struct samsung_fixed_rate exynos8890_fixed_rate_ext_clks[] __initdata = {
	FRATE(oscclk, "fin_pll", NULL, CLK_IS_ROOT, 0),
};

static struct of_device_id ext_clk_match[] __initdata = {
	{ .compatible = "samsung,exynos8890-oscclk", .data = (void *)0, },
};

static struct init_vclk exynos8890_ccore_vclks[] __initdata = {
	VCLK(ccore_i2c, gate_ccore_i2c, "gate_ccore_i2c", 0, 0, NULL),
};

static struct init_vclk exynos8890_mfc_vclks[] __initdata = {
	/* MFC */
	VCLK(mfc_hpm, gate_mfc_hpm, "mfc_hpm", 0, 0, NULL),
};

static struct init_vclk exynos8890_mscl_vclks[] __initdata = {
	/* MSCL */
	VCLK(mscl0, pxmxdx_mscl, "pxmxdx_mscl", 0, 0, NULL),
};

static struct init_vclk exynos8890_imem_vclks[] __initdata = {
	/* IMEM */
	VCLK(gate_apm, gate_imem_apm, "gate_imem_apm", 0, 0, NULL),
	VCLK(gate_sss, gate_imem_sss, "gate_imem_sss", 0, 0, NULL),
	VCLK(gate_gic400, gate_imem_gic400, "gate_imem_gic400", 0, 0, NULL),
};

static struct init_vclk exynos8890_peris_vclks[] __initdata = {
	/* PERIS */
	VCLK(peris_sfr, gate_peris_sfr_apbif_hdmi_cec, "gate_peris_sfr_apbif_hdmi_cec", 0, 0, NULL),
	VCLK(peris_hpm, gate_peris_hpm, "gate_peris_hpm", 0, 0, NULL),
	VCLK(peris_mct, gate_peris_mct, "gate_peris_mct", 0, 0, NULL),
	VCLK(wdt_mngs, gate_peris_wdt_mngs, "gate_peris_wdt_mngs", 0, 0, NULL),
	VCLK(wdt_apollo, gate_peris_wdt_apollo, "gate_peris_wdt_apollo", 0, 0, NULL),
	VCLK(rtc_apbif, gate_peris_rtc_apbif, "gate_peris_rtc_apbif", 0, 0, NULL),
	VCLK(top_rtc, gate_peris_top_rtc, "gate_peris_top_rtc", 0, 0, NULL),
	VCLK(otp_con_top, gate_peris_otp_con_top, "gate_peris_otp_con_top", 0, 0, NULL),
	VCLK(peris_chipid, gate_peris_chipid, "gate_peris_chipid", 0, 0, NULL),
	VCLK(peris_tmu, gate_peris_tmu, "gate_peris_tmu", 0, 0, NULL),
};

static struct init_vclk exynos8890_peric0_vclks[] __initdata = {
	/* PERIC0 */
	VCLK(gate_hsi2c0, gate_peric0_hsi2c0, "gate_peric0_hsi2c0", 0, 0, NULL),
	VCLK(gate_hsi2c1, gate_peric0_hsi2c1, "gate_peric0_hsi2c1", 0, 0, NULL),
	VCLK(gate_hsi2c4, gate_peric0_hsi2c4, "gate_peric0_hsi2c4", 0, 0, NULL),
	VCLK(gate_hsi2c5, gate_peric0_hsi2c5, "gate_peric0_hsi2c5", 0, 0, NULL),
	VCLK(gate_hsi2c9, gate_peric0_hsi2c9, "gate_peric0_hsi2c9", 0, 0, NULL),
	VCLK(gate_hsi2c10, gate_peric0_hsi2c10, "gate_peric0_hsi2c10", 0, 0, NULL),
	VCLK(gate_hsi2c11, gate_peric0_hsi2c11, "gate_peric0_hsi2c11", 0, 0, NULL),
	VCLK(puart0, gate_peric0_uart0, "gate_peric0_uart0", 0, 0, "console-pclk0"),
	VCLK(suart0, sclk_uart0, "sclk_uart0", 0, 0, "console-sclk0"),
	VCLK(gate_adcif, gate_peric0_adcif, "gate_peric0_adcif", 0, 0, NULL),
	VCLK(gate_pwm, gate_peric0_pwm, "gate_peric0_pwm", 0, 0, NULL),
	VCLK(gate_sclk_pwm, gate_peric0_sclk_pwm, "gate_peric0_sclk_pwm", 0, 0, NULL),
};

static struct init_vclk exynos8890_peric1_vclks[] __initdata = {
	/* PERIC1 HSI2C */
	VCLK(gate_hsi2c2, gate_peric1_hsi2c2, "gate_hsi2c2", 0, 0, NULL),
	VCLK(gate_hsi2c3, gate_peric1_hsi2c3, "gate_hsi2c3", 0, 0, NULL),
	VCLK(gate_hsi2c6, gate_peric1_hsi2c6, "gate_hsi2c6", 0, 0, NULL),
	VCLK(gate_hsi2c7, gate_peric1_hsi2c7, "gate_hsi2c7", 0, 0, NULL),
	VCLK(gate_hsi2c8, gate_peric1_hsi2c8, "gate_hsi2c8", 0, 0, NULL),
	VCLK(gate_hsi2c12, gate_peric1_hsi2c12, "gate_hsi2c12", 0, 0, NULL),
	VCLK(gate_hsi2c13, gate_peric1_hsi2c13, "gate_hsi2c13", 0, 0, NULL),
	VCLK(gate_hsi2c14, gate_peric1_hsi2c14, "gate_hsi2c14", 0, 0, NULL),
	/* PERIC1 UART0~5 */
	VCLK(gate_uart1, gate_peric1_uart1, "gate_uart1", 0, 0, "console-pclk1"),
	VCLK(gate_uart2, gate_peric1_uart2, "gate_uart2", 0, 0, "console-pclk2"),
	VCLK(gate_uart3, gate_peric1_uart3, "gate_uart3", 0, 0, "console-pclk3"),
	VCLK(gate_uart4, gate_peric1_uart4, "gate_uart4", 0, 0, "console-pclk4"),
	VCLK(gate_uart5, gate_peric1_uart5, "gate_uart5", 0, 0, "console-pclk5"),
	VCLK(suart1, sclk_uart1, "sclk_uart1", 0, 0, "console-sclk1"),
	VCLK(suart2, sclk_uart2, "sclk_uart2", 0, 0, "console-sclk2"),
	VCLK(suart3, sclk_uart3, "sclk_uart3", 0, 0, "console-sclk3"),
	VCLK(suart4, sclk_uart4, "sclk_uart4", 0, 0, "console-sclk4"),
	VCLK(suart5, sclk_uart5, "sclk_uart5", 0, 0, "console-sclk5"),
	/* PERIC1 SPI0~7 */
	VCLK(gate_spi0, gate_peric1_spi0, "gate_spi0", 0, 0, NULL),
	VCLK(gate_spi1, gate_peric1_spi1, "gate_spi1", 0, 0, NULL),
	VCLK(gate_spi2, gate_peric1_spi2, "gate_spi2", 0, 0, NULL),
	VCLK(gate_spi3, gate_peric1_spi3, "gate_spi3", 0, 0, NULL),
	VCLK(gate_spi4, gate_peric1_spi4, "gate_spi4", 0, 0, NULL),
	VCLK(gate_spi5, gate_peric1_spi5, "gate_spi5", 0, 0, NULL),
	VCLK(gate_spi6, gate_peric1_spi6, "gate_spi6", 0, 0, NULL),
	VCLK(gate_spi7, gate_peric1_spi7, "gate_spi7", 0, 0, NULL),
	VCLK(sclk_peric1_spi0, sclk_spi0, "sclk_spi0", 0, 0, NULL),
	VCLK(sclk_peric1_spi1, sclk_spi1, "sclk_spi1", 0, 0, NULL),
	VCLK(sclk_peric1_spi2, sclk_spi2, "sclk_spi2", 0, 0, NULL),
	VCLK(sclk_peric1_spi3, sclk_spi3, "sclk_spi3", 0, 0, NULL),
	VCLK(sclk_peric1_spi4, sclk_spi4, "sclk_spi4", 0, 0, NULL),
	VCLK(sclk_peric1_spi5, sclk_spi5, "sclk_spi5", 0, 0, NULL),
	VCLK(sclk_peric1_spi6, sclk_spi6, "sclk_spi6", 0, 0, NULL),
	VCLK(sclk_peric1_spi7, sclk_spi7, "sclk_spi7", 0, 0, NULL),
	/* PERIC1 GPIO */
	VCLK(gate_gpio_nfc, gate_peric1_gpio_nfc, "gate_gpio_nfc", 0, 0, NULL),
	VCLK(gate_gpio_touch, gate_peric1_gpio_touch, "gate_gpio_touch", 0, 0, NULL),
	VCLK(gate_gpio_fp, gate_peric1_gpio_fp, "gate_gpio_fp", 0, 0, NULL),
	VCLK(gate_gpio_ese, gate_peric1_gpio_ese, "gate_gpio_ese", 0, 0, NULL),
	/* PERIC1 promise */
	VCLK(promise_int, sclk_promise_int, "sclk_promise_int", 0, 0, NULL),
	VCLK(promise_disp, sclk_promise_disp, "sclk_promise_disp", 0, 0, NULL),
	VCLK(ap2cp_mif_pll_out, sclk_ap2cp_mif_pll_out, "sclk_ap2cp_mif_pll_out", 0, 0, NULL),
};

static struct init_vclk exynos8890_isp0_vclks[] __initdata = {
	/* ISP0 */
	VCLK(gate_fimc_isp0, gate_isp0_fimc_isp0, "gate_fimc_isp0", 0, 0, NULL),
	VCLK(gate_fimc_tpu, gate_isp0_fimc_tpu, "gate_isp0_fimc_tpu", 0, 0, NULL),
	VCLK(isp0, pxmxdx_isp0_isp0, "clk_isp0", 0, 0, NULL),
	VCLK(isp0_tpu, pxmxdx_isp0_tpu, "clk_isp0_tpu", 0, 0, NULL),
	VCLK(isp0_trex, pxmxdx_isp0_trex, "clk_isp0_trex", 0, 0, NULL),
};

static struct init_vclk exynos8890_isp1_vclks[] __initdata = {
	/* ISP1 */
	VCLK(gate_fimc_isp1, gate_isp1_fimc_isp1, "gate_isp1_fimc_isp1", 0, 0, NULL),
	VCLK(isp1, pxmxdx_isp1_isp1, "clk_isp1", 0, 0, NULL),
};

static struct init_vclk exynos8890_isp_sensor_vclks[] __initdata = {
	/* ISP1 */
	VCLK(isp_sensor0, sclk_isp_sensor0, "sclk_isp_sensor0", 0, 0, NULL),
	VCLK(isp_sensor1, sclk_isp_sensor1, "sclk_isp_sensor1", 0, 0, NULL),
	VCLK(isp_sensor2, sclk_isp_sensor2, "sclk_isp_sensor2", 0, 0, NULL),
	VCLK(isp_sensor3, sclk_isp_sensor3, "sclk_isp_sensor3", 0, 0, NULL),
};

static struct init_vclk exynos8890_cam0_vclks[] __initdata = {
	/* CAM0 */
	VCLK(gate_csis0, gate_cam0_csis0, "gate_cam0_csis0", 0, 0, NULL),
	VCLK(gate_csis1, gate_cam0_csis1, "gate_cam0_csis1", 0, 0, NULL),
	VCLK(gate_fimc_bns, gate_cam0_fimc_bns, "gate_cam0_fimc_bns", 0, 0, NULL),
	VCLK(fimc_3aa0, gate_cam0_fimc_3aa0, "gate_cam0_fimc_3aa0", 0, 0, NULL),
	VCLK(fimc_3aa1, gate_cam0_fimc_3aa1, "gate_cam0_fimc_3aa1", 0, 0, NULL),
	VCLK(cam0_hpm, gate_cam0_hpm, "gate_cam0_hpm", 0, 0, NULL),
	VCLK(pxmxdx_csis0, pxmxdx_cam0_csis0, "gate_pxmxdx_cam0_csis0", 0, 0, NULL),
	VCLK(pxmxdx_csis1, pxmxdx_cam0_csis1, "gate_pxmxdx_cam0_csis1", 0, 0, NULL),
	VCLK(pxmxdx_csis2, pxmxdx_cam0_csis2, "gate_pxmxdx_cam0_csis2", 0, 0, NULL),
	VCLK(pxmxdx_csis3, pxmxdx_cam0_csis3, "gate_pxmxdx_cam0_csis3", 0, 0, NULL),
	VCLK(pxmxdx_3aa0, pxmxdx_cam0_3aa0, "gate_pxmxdx_cam0_3aa0", 0, 0, NULL),
	VCLK(pxmxdx_3aa1, pxmxdx_cam0_3aa1, "gate_pxmxdx_cam0_3aa1", 0, 0, NULL),
	VCLK(pxmxdx_trex, pxmxdx_cam0_trex, "gate_pxmxdx_cam0_trex", 0, 0, NULL),
	VCLK(hs0_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs0_csis0_user, "umux_cam0_phyclk_rxbyteclkhs0_csis0_user", 0, 0, NULL),
	VCLK(hs1_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs1_csis0_user, "umux_cam0_phyclk_rxbyteclkhs1_csis0_user", 0, 0, NULL),
	VCLK(hs2_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs2_csis0_user, "umux_cam0_phyclk_rxbyteclkhs2_csis0_user", 0, 0, NULL),
	VCLK(hs3_csis0_rx_byte, umux_cam0_phyclk_rxbyteclkhs3_csis0_user, "umux_cam0_phyclk_rxbyteclkhs3_csis0_user", 0, 0, NULL),
	VCLK(hs0_csis1_rx_byte, umux_cam0_phyclk_rxbyteclkhs0_csis1_user, "umux_cam0_phyclk_rxbyteclkhs0_csis1_user", 0, 0, NULL),
	VCLK(hs1_csis1_rx_byte, umux_cam0_phyclk_rxbyteclkhs1_csis1_user, "umux_cam0_phyclk_rxbyteclkhs1_csis1_user", 0, 0, NULL),
};

static struct init_vclk exynos8890_cam1_vclks[] __initdata = {
	/* CAM1 */
	VCLK(gate_isp_cpu, gate_cam1_isp_cpu, "gate_cam1_isp_cpu", 0, 0, NULL),
	VCLK(gate_csis2, gate_cam1_csis2, "gate_cam1_csis2", 0, 0, NULL),
	VCLK(gate_csis3, gate_cam1_csis3, "gate_cam1_csis3", 0, 0, NULL),
	VCLK(gate_fimc_vra, gate_cam1_fimc_vra, "gate_cam1_fimc_vra", 0, 0, NULL),
	VCLK(gate_mc_scaler, gate_cam1_mc_scaler, "gate_cam1_mc_scaler", 0, 0, NULL),
	VCLK(gate_i2c0_isp, gate_cam1_i2c0_isp, "gate_cam1_i2c0_isp", 0, 0, NULL),
	VCLK(gate_i2c1_isp, gate_cam1_i2c1_isp, "gate_cam1_i2c1_isp", 0, 0, NULL),
	VCLK(gate_i2c2_isp, gate_cam1_i2c2_isp, "gate_cam1_i2c2_isp", 0, 0, NULL),
	VCLK(gate_i2c3_isp, gate_cam1_i2c3_isp, "gate_cam1_i2c3_isp", 0, 0, NULL),
	VCLK(gate_wdt_isp, gate_cam1_wdt_isp, "gate_cam1_wdt_isp", 0, 0, NULL),
	VCLK(gate_mcuctl_isp, gate_cam1_mcuctl_isp, "gate_cam1_mcuctl_isp", 0, 0, NULL),
	VCLK(gate_uart_isp, gate_cam1_uart_isp, "gate_cam1_uart_isp", 0, 0, NULL),
	VCLK(gate_pdma_isp, gate_cam1_pdma_isp, "gate_cam1_pdma_isp", 0, 0, NULL),
	VCLK(gate_pwm_isp, gate_cam1_pwm_isp, "gate_cam1_pwm_isp", 0, 0, NULL),
	VCLK(gate_spi0_isp, gate_cam1_spi0_isp, "gate_cam1_spi0_isp", 0, 0, NULL),
	VCLK(gate_spi1_isp, gate_cam1_spi1_isp, "gate_cam1_spi1_isp", 0, 0, NULL),
	VCLK(isp_spi0, sclk_isp_spi0, "sclk_isp_spi0", 0, 0, NULL),
	VCLK(isp_spi1, sclk_isp_spi1, "sclk_isp_spi1", 0, 0, NULL),
	VCLK(isp_uart, sclk_isp_uart, "sclk_isp_uart", 0, 0, NULL),
	VCLK(gate_sclk_pwm_isp, gate_cam1_sclk_pwm_isp, "gate_cam1_sclk_pwm_isp", 0, 0, NULL),
	VCLK(gate_sclk_uart_isp, gate_cam1_sclk_uart_isp, "gate_cam1_sclk_uart_isp", 0, 0, NULL),
};

static struct init_vclk exynos8890_audio_vclks[] __initdata = {
	/* AUDIO */
	VCLK(gate_mi2s, gate_aud_mi2s, "gate_aud_mi2s", 0, 0, NULL),
	VCLK(gate_pcm, gate_aud_pcm, "gate_aud_pcm", 0, 0, NULL),
	VCLK(gate_slimbus, gate_aud_slimbus, "gate_aud_slimbus", 0, 0, NULL),
	VCLK(gate_sclk_mi2s, gate_aud_sclk_mi2s, "gate_aud_sclk_mi2s", 0, 0, NULL),
	VCLK(d1_sclk_i2s, d1_sclk_i2s_local, "dout_sclk_i2s_local", 0, 0, NULL),
	VCLK(gate_sclk_pcm, gate_aud_sclk_pcm, "gate_aud_sclk_pcm", 0, 0, NULL),
	VCLK(d1_sclk_pcm, d1_sclk_pcm_local, "dout_sclk_pcm_local", 0, 0, NULL),
	VCLK(gate_sclk_slimbus, gate_aud_sclk_slimbus, "gate_aud_sclk_slimbus", 0, 0, NULL),
	VCLK(sclk_slimbus, d1_sclk_slimbus, "dout_sclk_slimbus", 0, 0, NULL),
	VCLK(sclk_cp_i2s, d1_sclk_cp_i2s, "dout_sclk_cp_i2s", 0, 0, NULL),
	VCLK(sclk_asrc, d1_sclk_asrc, "dout_sclk_asrc", 0, 0, NULL),
	VCLK(aud_pll, p1_aud_pll, "sclk_aud_pll", 0, 0, NULL),
	VCLK(aud_cp, pxmxdx_aud_cp, "gate_aud_cp", 0, 0, NULL),
};

static struct init_vclk exynos8890_fsys0_vclks[] __initdata = {
	/* FSYS0 */
	VCLK(gate_usbdrd30, gate_fsys0_usbdrd30, "gate_fsys0_usbdrd30", 0, 0, NULL),
	VCLK(gate_usbhost20, gate_fsys0_usbhost20, "gate_fsys0_usbhost20", 0, 0, NULL),
	VCLK(usbdrd30, sclk_usbdrd30, "sclk_usbdrd30", 0, 0, NULL),
	VCLK(sclk_fsys0_mmc0, sclk_mmc0, "sclk_mmc0", 0, 0, NULL),
	VCLK(ufsunipro20, sclk_ufsunipro20, "sclk_ufsunipro20", 0, 0, NULL),
	VCLK(phy24m, sclk_phy24m, "sclk_phy24m", 0, 0, NULL),
	VCLK(ufsunipro_cfg, sclk_ufsunipro_cfg, "sclk_ufsunipro_cfg", 0, 0, NULL),
	/* UMUX GATE related clock sources */
	VCLK(gate_udrd30_phyclock, umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user, "umux_fsys0_phyclk_usbdrd30_udrd30_phyclock_user", 0, 0, NULL),
	VCLK(gate_udrd30_pipe, umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user, "umux_fsys0_phyclk_usbdrd30_udrd30_pipe_pclk_user", 0, 0, NULL),
	VCLK(gate_ufs_tx0, umux_fsys0_phyclk_ufs_tx0_symbol_user, "umux_fsys0_phyclk_ufs_tx0_symbol_user", 0, 0, NULL),
	VCLK(gate_ufs_rx0, umux_fsys0_phyclk_ufs_rx0_symbol_user, "umux_fsys0_phyclk_ufs_rx0_symbol_user", 0, 0, NULL),
	VCLK(usbhost20_phyclock, umux_fsys0_phyclk_usbhost20_phyclock_user, "umux_fsys0_phyclk_usbhost20_phyclock_user", 0, 0, NULL),
	VCLK(usbhost20_freeclk, umux_fsys0_phyclk_usbhost20_freeclk_user, "umux_fsys0_phyclk_usbhost20_freeclk_user", 0, 0, NULL),
	VCLK(usbhost20_clk48, umux_fsys0_phyclk_usbhost20_clk48mohci_user, "umux_fsys0_phyclk_usbhost20_clk48mohci_user", 0, 0, NULL),
	VCLK(usbhost20phy_ref, umux_fsys0_phyclk_usbhost20phy_ref_clk, "umux_fsys0_phyclk_usbhost20phy_ref_clk", 0, 0, NULL),
	VCLK(ufs_rx_pwm, umux_fsys0_phyclk_ufs_rx_pwm_clk_user, "umux_fsys0_phyclk_ufs_rx_pwm_clk_user", 0, 0, NULL),
	VCLK(ufs_tx_pwm, umux_fsys0_phyclk_ufs_tx_pwm_clk_user, "umux_fsys0_phyclk_ufs_tx_pwm_clk_user", 0, 0, NULL),
	VCLK(ufs_refclk_out, umux_fsys0_phyclk_ufs_refclk_out_soc_user, "umux_fsys0_phyclk_ufs_refclk_out_soc_user", 0, 0, NULL),
	VCLK(fsys_200, pxmxdx_fsys0, "aclk_ufs", 0, 0, NULL),
};

static struct init_vclk exynos8890_fsys1_vclks[] __initdata = {
	/* FSYS1 */
	VCLK(fsys1_hpm, gate_fsys1_hpm, "gate_fsys1_hpm", 0, 0, NULL),
	VCLK(fsys1_sclk_mmc2, sclk_mmc2, "sclk_mmc2", 0, 0, NULL),
	VCLK(ufsunipro20_sdcard, sclk_ufsunipro20_sdcard, "sclk_ufsunipro20_sdcard", 0, 0, NULL),
	VCLK(pcie_phy, sclk_pcie_phy, "sclk_pcie_phy", 0, 0, NULL),
	VCLK(sclk_ufsunipro_sdcard, sclk_ufsunipro_sdcard_cfg, "sclk_ufsunipro_sdcard_cfg", 0, 0, NULL),
	/* UMUX GATE related clock sources */
	VCLK(ufs_link_sdcard_tx0, umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user, "umux_fsys1_phyclk_ufs_link_sdcard_tx0_symbol_user", 0, 0, NULL),
	VCLK(ufs_link_sdcard_rx0, umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user, "umux_fsys1_phyclk_ufs_link_sdcard_rx0_symbol_user", 0, 0, NULL),
	VCLK(pcie_wifi0_tx0, umux_fsys1_phyclk_pcie_wifi0_tx0_user, "umux_fsys1_phyclk_pcie_wifi0_tx0_user", 0, 0, NULL),
	VCLK(pcie_wifi0_rx0, umux_fsys1_phyclk_pcie_wifi0_rx0_user, "umux_fsys1_phyclk_pcie_wifi0_rx0_user", 0, 0, NULL),
	VCLK(pcie_wifi1_tx0, umux_fsys1_phyclk_pcie_wifi1_tx0_user, "umux_fsys1_phyclk_pcie_wifi1_tx0_user", 0, 0, NULL),
	VCLK(pcie_wifi1_rx0, umux_fsys1_phyclk_pcie_wifi1_rx0_user, "umux_fsys1_phyclk_pcie_wifi1_rx0_user", 0, 0, NULL),
	VCLK(wifi0_dig_refclk, umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user, "umux_fsys1_phyclk_pcie_wifi0_dig_refclk_user", 0, 0, NULL),
	VCLK(wifi1_dig_refclk, umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user, "umux_fsys1_phyclk_pcie_wifi1_dig_refclk_user", 0, 0, NULL),
	VCLK(sdcard_rx_pwm, umux_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk_user, "umux_fsys1_phyclk_ufs_link_sdcard_rx_pwm_clk_user", 0, 0, NULL),
	VCLK(sdcard_tx_pwm, umux_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk_user, "umux_fsys1_phyclk_ufs_link_sdcard_tx_pwm_clk_user", 0, 0, NULL),
	VCLK(sdcard_refclk, umux_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc_user, "umux_fsys1_phyclk_ufs_link_sdcard_refclk_out_soc_user", 0, 0, NULL),
};

static struct init_vclk exynos8890_g3d_vclks[] __initdata = {
	/* G3D */
	VCLK(gate_g3d, gate_g3d_g3d, "gate_g3d_g3d", 0, 0, NULL),
	VCLK(gate_g3d_iram, gate_g3d_iram_path_test, "g3d_iram_path", 0, 0, NULL),
};

static struct init_vclk exynos8890_disp0_vclks[] __initdata = {
	/* DISP0 */
	VCLK(gate_decon0, gate_disp0_decon0, "gate_disp0_decon0", 0, 0, NULL),
	VCLK(gate_dsim0, gate_disp0_dsim0, "gate_disp0_dsim0", 0, 0, NULL),
	VCLK(gate_dsim1, gate_disp0_dsim1, "gate_disp0_dsim1", 0, 0, NULL),
	VCLK(gate_dsim2, gate_disp0_dsim2, "gate_disp0_dsim2", 0, 0, NULL),
	VCLK(gate_hdmi, gate_disp0_hdmi, "gate_disp0_hdmi", 0, 0, NULL),
	VCLK(gate_dp, gate_disp0_dp, "gate_disp0_dp", 0, 0, NULL),
	VCLK(gate_hpm_apbif_disp0, gate_disp0_hpm_apbif_disp0, "gate_disp0_hpm_apbif_disp0", 0, 0, NULL),
	/* special clock - sclk */
	VCLK(decon0_eclk0, sclk_decon0_eclk0, "sclk_decon0_eclk0", 0, 0, NULL),
	VCLK(decon0_vclk0, sclk_decon0_vclk0, "sclk_decon0_vclk0", 0, 0, NULL),
	VCLK(decon0_vclk1, sclk_decon0_vclk1, "sclk_decon0_vclk1", 0, 0, NULL),
	VCLK(decon0_eclk0_local, sclk_decon0_eclk0_local, "sclk_decon0_eclk0_local", 0, 0, NULL),
	VCLK(decon0_vclk0_local, sclk_decon0_vclk0_local, "sclk_decon0_vclk0_local", 0, 0, NULL),
	VCLK(decon0_vclk1_local, sclk_decon0_vclk1_local, "sclk_decon0_vclk1_local", 0, 0, NULL),
	VCLK(hdmi_audio, sclk_hdmi_audio, "sclk_hdmi_audio", 0, 0, NULL),
	/* PLL clock source */
	VCLK(disp_pll, p1_disp_pll, "p1_disp_pll", 0, 0, NULL),
	/* USERMUX related clock source */
	VCLK(mipidphy0_rxclkesc0, umux_disp0_phyclk_mipidphy0_rxclkesc0_user, "umux_disp0_phyclk_mipidphy0_rxclkesc0_user", 0, 0, NULL),
	VCLK(mipidphy0_bitclkdiv8, umux_disp0_phyclk_mipidphy0_bitclkdiv8_user, "umux_disp0_phyclk_mipidphy0_bitclkdiv8_user", 0, 0, NULL),
	VCLK(mipidphy1_rxclkesc0, umux_disp0_phyclk_mipidphy1_rxclkesc0_user, "umux_disp0_phyclk_mipidphy1_rxclkesc0_user", 0, 0, NULL),
	VCLK(mipidphy1_bitclkdiv8, umux_disp0_phyclk_mipidphy1_bitclkdiv8_user, "umux_disp0_phyclk_mipidphy1_bitclkdiv8_user", 0, 0, NULL),
	VCLK(mipidphy2_rxclkesc0, umux_disp0_phyclk_mipidphy2_rxclkesc0_user, "umux_disp0_phyclk_mipidphy2_rxclkesc0_user", 0, 0, NULL),
	VCLK(mipidphy2_bitclkdiv8, umux_disp0_phyclk_mipidphy2_bitclkdiv8_user, "umux_disp0_phyclk_mipidphy2_bitclkdiv8_user", 0, 0, NULL),
	VCLK(dpphy_ch0_txd, umux_disp0_phyclk_dpphy_ch0_txd_clk_user, "umux_disp0_phyclk_dpphy_ch0_txd_clk_user", 0, 0, NULL),
	VCLK(dpphy_ch1_txd, umux_disp0_phyclk_dpphy_ch1_txd_clk_user, "umux_disp0_phyclk_dpphy_ch1_txd_clk_user", 0, 0, NULL),
	VCLK(dpphy_ch2_txd, umux_disp0_phyclk_dpphy_ch2_txd_clk_user, "umux_disp0_phyclk_dpphy_ch2_txd_clk_user", 0, 0, NULL),
	VCLK(dpphy_ch3_txd, umux_disp0_phyclk_dpphy_ch3_txd_clk_user, "umux_disp0_phyclk_dpphy_ch3_txd_clk_user", 0, 0, NULL),
	VCLK(dptx_phy_i_ref_clk_24m, gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m, "gate_disp0_oscclk_i_dptx_phy_i_ref_clk_24m", 0, 0, NULL),
	VCLK(mipi_dphy_m1s0, gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi, "gate_disp0_oscclk_i_mipi_dphy_m1s0_m_xi", 0, 0, NULL),
	VCLK(mipi_dphy_m4s0, gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi, "gate_disp0_oscclk_i_mipi_dphy_m4s0_m_xi", 0, 0, NULL),
	VCLK(mipi_dphy_m4s4, gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi, "gate_disp0_oscclk_i_mipi_dphy_m4s4_m_xi", 0, 0, NULL),
	VCLK(phyclk_hdmiphy_tmds_20b, umux_disp0_phyclk_hdmiphy_tmds_clko_user, "umux_disp0_phyclk_hdmiphy_tmds_clko_user", 0, 0, NULL),
	VCLK(phyclk_hdmiphy_pixel, umux_disp0_phyclk_hdmiphy_pixel_clko_user, "umux_disp0_phyclk_hdmiphy_pixel_clko_user", 0, 0, NULL),
	VCLK(mipidphy0_bitclkdiv2_user, umux_disp0_phyclk_mipidphy0_bitclkdiv2_user, "umux_disp0_phyclk_mipidphy0_bitclkdiv2_user", 0, 0, NULL),
	VCLK(mipidphy1_bitclkdiv2_user, umux_disp0_phyclk_mipidphy1_bitclkdiv2_user, "umux_disp0_phyclk_mipidphy1_bitclkdiv2_user", 0, 0, NULL),
	VCLK(mipidphy2_bitclkdiv2_user, umux_disp0_phyclk_mipidphy2_bitclkdiv2_user, "umux_disp0_phyclk_mipidphy2_bitclkdiv2_user", 0, 0, NULL),
};

static struct init_vclk exynos8890_disp1_vclks[] __initdata = {
	/* DISP1 */
	VCLK(gate_decon1, gate_disp1_decon1, "gate_disp1_decon1", 0, 0, NULL),
	VCLK(gate_hpmdisp1, gate_disp1_hpmdisp1, "gate_disp1_hpmdisp1", 0, 0, NULL),
	/* special clock - sclk */
	VCLK(decon1_eclk0, sclk_decon1_eclk0, "sclk_decon1_eclk0", 0, 0, NULL),
	VCLK(decon1_eclk1, sclk_decon1_eclk1, "sclk_decon1_eclk1", 0, 0, NULL),
	VCLK(decon1_eclk0_local, sclk_decon1_eclk0_local, "sclk_decon1_eclk0_local", 0, 0, NULL),
	VCLK(decon1_eclk1_local, sclk_decon1_eclk1_local, "sclk_decon1_eclk1_local", 0, 0, NULL),
	/* USERMUX related clock source */
	VCLK(disp1_phyclk_mipidphy0_bitclkdiv2_user, umux_disp1_phyclk_mipidphy0_bitclkdiv2_user, "umux_disp1_phyclk_mipidphy0_bitclkdiv2_user", 0, 0, NULL),
	VCLK(disp1_phyclk_mipidphy1_bitclkdiv2_user, umux_disp1_phyclk_mipidphy1_bitclkdiv2_user, "umux_disp1_phyclk_mipidphy1_bitclkdiv2_user", 0, 0, NULL),
	VCLK(disp1_phyclk_mipidphy2_bitclkdiv2_user, umux_disp1_phyclk_mipidphy2_bitclkdiv2_user, "umux_disp1_phyclk_mipidphy2_bitclkdiv2_user", 0, 0, NULL),
	VCLK(disp1_phyclk_disp1_hdmiphy_pixel_clko_user, umux_disp1_phyclk_disp1_hdmiphy_pixel_clko_user, "umux_disp1_phyclk_disp1_hdmiphy_pixel_clko_user", 0, 0, NULL),
};

void __init exynos8890_clk_init(struct device_node *np)
{
	int ret;

	if (!np)
		panic("%s: unable to determine SoC\n", __func__);

	ret = cal_init();
	if (ret)
		pr_err("%s: unable to initialize power cal\n", __func__);

	samsung_clk_init(np, 0, nr_clks, NULL, 0, NULL, 0);
	samsung_register_of_fixed_ext(exynos8890_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos8890_fixed_rate_ext_clks),
			ext_clk_match);

	/* Regist clock local IP */
	samsung_register_vclk(exynos8890_audio_vclks, ARRAY_SIZE(exynos8890_audio_vclks));
	samsung_register_vclk(exynos8890_cam0_vclks, ARRAY_SIZE(exynos8890_cam0_vclks));
	samsung_register_vclk(exynos8890_cam1_vclks, ARRAY_SIZE(exynos8890_cam1_vclks));
	samsung_register_vclk(exynos8890_disp0_vclks, ARRAY_SIZE(exynos8890_disp0_vclks));
	samsung_register_vclk(exynos8890_disp1_vclks, ARRAY_SIZE(exynos8890_disp1_vclks));
	samsung_register_vclk(exynos8890_fsys0_vclks, ARRAY_SIZE(exynos8890_fsys0_vclks));
	samsung_register_vclk(exynos8890_fsys1_vclks, ARRAY_SIZE(exynos8890_fsys1_vclks));
	samsung_register_vclk(exynos8890_g3d_vclks, ARRAY_SIZE(exynos8890_g3d_vclks));
	samsung_register_vclk(exynos8890_imem_vclks, ARRAY_SIZE(exynos8890_imem_vclks));
	samsung_register_vclk(exynos8890_isp0_vclks, ARRAY_SIZE(exynos8890_isp0_vclks));
	samsung_register_vclk(exynos8890_isp1_vclks, ARRAY_SIZE(exynos8890_isp1_vclks));
	samsung_register_vclk(exynos8890_isp_sensor_vclks, ARRAY_SIZE(exynos8890_isp1_vclks));
	samsung_register_vclk(exynos8890_mfc_vclks, ARRAY_SIZE(exynos8890_mfc_vclks));
	samsung_register_vclk(exynos8890_mscl_vclks, ARRAY_SIZE(exynos8890_mscl_vclks));
	samsung_register_vclk(exynos8890_peric0_vclks, ARRAY_SIZE(exynos8890_peric0_vclks));
	samsung_register_vclk(exynos8890_peric1_vclks, ARRAY_SIZE(exynos8890_peric1_vclks));
	samsung_register_vclk(exynos8890_peris_vclks, ARRAY_SIZE(exynos8890_peris_vclks));
	samsung_register_vclk(exynos8890_ccore_vclks, ARRAY_SIZE(exynos8890_ccore_vclks));

	pr_info("EXYNOS8890: Clock setup completed\n");
}
CLK_OF_DECLARE(exynos8890_clks, "samsung,exynos8890-clock", exynos8890_clk_init);
