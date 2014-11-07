/*
 * Copyright (c) 2013 Samsung Electronics Co., Ltd.
 * Authors: Thomas Abraham <thomas.ab@samsung.com>
 *	    Chander Kashyap <k.chander@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Common Clock Framework support for Exynos8890 SoC.
*/

#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/clk-provider.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <dt-bindings/clock/exynos8890.h>

#include "clk.h"
#include "clk-pll.h"

/*
 * list of controller registers to be saved and restored during a
 * suspend/resume cycle.
 */
/* fixed rate clocks generated outside the soc */
struct samsung_fixed_rate_clock exynos8890_fixed_rate_ext_clks[] __initdata = {
	FRATE(CLK_FIN_PLL, "fin_pll", NULL, CLK_IS_ROOT, 24000000),
};

/* After complete to prepare clock tree, these will be removed. */
struct samsung_fixed_rate_clock exynos8890_fixed_rate_clks[] __initdata = {
	FRATE(CLK_UART_BAUD0, "clk_uart_baud0", NULL, CLK_IS_ROOT, 130867200),
	FRATE(CLK_GATE_PCLK0, "gate_pclk0", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_PCLK1, "gate_pclk1", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_PCLK2, "gate_pclk2", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_PCLK3, "gate_pclk3", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_PCLK4, "gate_pclk4", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_PCLK5, "gate_pclk5", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_UART0, "gate_uart0", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_UART1, "gate_uart1", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_UART2, "gate_uart2", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_UART3, "gate_uart3", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_UART4, "gate_uart4", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_GATE_UART5, "gate_uart5", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_UART0, "uart0", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_UART1, "uart1", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_UART2, "uart2", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_UART3, "uart3", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_UART4, "uart4", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_UART5, "uart5", NULL, CLK_IS_ROOT, 24000000),
	FRATE(CLK_MCT, "mct", NULL, CLK_IS_ROOT, 24000000),
};

static __initdata struct of_device_id ext_clk_match[] = {
	{ .compatible = "samsung,exynos8890-oscclk", .data = (void *)0, },
	{ },
};

/* register exynos8890 clocks */
void __init exynos8890_clk_init(struct device_node *np)
{
	struct samsung_clk_provider *ctx;
	void __iomem *reg_base;

	if (np) {
		reg_base = of_iomap(np, 0);
		if (!reg_base)
			panic("%s: failed to map registers\n", __func__);
	} else {
		panic("%s: unable to determine soc\n", __func__);
	}

	ctx = samsung_clk_init(np, reg_base, CLK_NR_CLKS);
	if (!ctx)
		panic("%s: unable to allocate context.\n", __func__);

	samsung_clk_of_register_fixed_ext(ctx, exynos8890_fixed_rate_ext_clks,
			ARRAY_SIZE(exynos8890_fixed_rate_ext_clks),
			ext_clk_match);

	samsung_clk_register_fixed_rate(ctx, exynos8890_fixed_rate_clks,
			ARRAY_SIZE(exynos8890_fixed_rate_clks));

	samsung_clk_of_add_provider(np, ctx);
}
CLK_OF_DECLARE(exynos8890_clk, "samsung,exynos8890-clock", exynos8890_clk_init);
