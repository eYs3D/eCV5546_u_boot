// SPDX-License-Identifier: GPL-2.0+
/*
 * dwc3-of-simple.c - OF glue layer for simple integrations
 *
 * Copyright (c) 2015 Texas Instruments Incorporated - https://www.ti.com
 *
 * Author: Felipe Balbi <balbi@ti.com>
 *
 * Copyright (C) 2018 BayLibre, SAS
 * Author: Neil Armstrong <narmstron@baylibre.com>
 */

#include <common.h>
#include <dm.h>
#include <reset.h>
#include <clk.h>

struct dwc3_of_simple {
	struct clk_bulk		clks;
	struct reset_ctl_bulk	resets;
};

static int dwc3_of_simple_reset_init(struct udevice *dev,
				     struct dwc3_of_simple *simple)
{
	int ret;

	ret = reset_get_bulk(dev, &simple->resets);
	if (ret == -ENOTSUPP)
		return 0;
	else if (ret)
		return ret;

	ret = reset_deassert_bulk(&simple->resets);
	if (ret) {
		reset_release_bulk(&simple->resets);
		return ret;
	}

	return 0;
}

static int dwc3_of_simple_clk_init(struct udevice *dev,
				   struct dwc3_of_simple *simple)
{
	int ret;

	ret = clk_get_bulk(dev, &simple->clks);
	if (ret == -ENOSYS)
		return 0;
	if (ret)
		return ret;

#if CONFIG_IS_ENABLED(CLK)
	ret = clk_enable_bulk(&simple->clks);
	if (ret) {
		clk_release_bulk(&simple->clks);
		return ret;
	}
#endif

	return 0;
}

static int dwc3_of_simple_probe(struct udevice *dev)
{
	struct dwc3_of_simple *simple = dev_get_plat(dev);
	int ret;

	ret = dwc3_of_simple_clk_init(dev, simple);
	if (ret)
		return ret;

	ret = dwc3_of_simple_reset_init(dev, simple);
	if (ret)
		return ret;

	return 0;
}

static int dwc3_of_simple_remove(struct udevice *dev)
{
	struct dwc3_of_simple *simple = dev_get_plat(dev);

	reset_release_bulk(&simple->resets);

	clk_release_bulk(&simple->clks);

	return dm_scan_fdt_dev(dev);
}

static const struct udevice_id dwc3_of_simple_ids[] = {
	{ .compatible = "eys3d,dwcusb3" },
	{ .compatible = "amlogic,meson-gxl-dwc3" },
	{ .compatible = "rockchip,rk3399-dwc3" },
	{ .compatible = "ti,dwc3" },
	{ }
};

U_BOOT_DRIVER(dwc3_of_simple) = {
	.name = "dwc3-of-simple",
	.id = UCLASS_SIMPLE_BUS,
	.of_match = dwc3_of_simple_ids,
	.probe = dwc3_of_simple_probe,
	.remove = dwc3_of_simple_remove,
	.plat_auto	= sizeof(struct dwc3_of_simple),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
