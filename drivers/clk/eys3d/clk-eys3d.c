// SPDX-License-Identifier: GPL-2.0

#include <asm/arch/clk-eys3d.h>

ulong extclk_rate;
struct udevice *clkc_dev;

/************************************************* CLKC *************************************************/

static int eys3d_clk_enable(struct clk *clk)
{
	sp_clk_endisable(clk, 1);
	return 0;
}

static int eys3d_clk_disable(struct clk *clk)
{
	sp_clk_endisable(clk, 0);
	return 0;
}

static ulong eys3d_clk_get_rate(struct clk *clk)
{
	return sp_clk_get_rate(clk);
}

static ulong eys3d_clk_set_rate(struct clk *clk, ulong rate)
{
	return sp_clk_set_rate(clk, rate);
}

int eys3d_clk_get_by_index(int index, struct clk *clk)
{
	clk->dev = clkc_dev;
	clk->id = index;
	return 0;
}

int eys3d_clk_request(struct udevice *dev, struct clk *clk)
{
	u32 n = (clk->id + 1) * 2;
	u32 *clkd = malloc(n * sizeof(u32));

	if (!clkd)
		return -ENOMEM;

	if (fdtdec_get_int_array(gd->fdt_blob, dev_of_offset(dev), "clocks", clkd, n)) {
		pr_err("Faild to find clocks node. Check device tree\n");
		free(clkd);
		return -ENOENT;
	}
	n = clkd[n - 1];
	free(clkd);

	return eys3d_clk_get_by_index(n, clk);
}

static struct clk_ops eys3d_clk_ops = {
	.disable	= eys3d_clk_disable,
	.enable		= eys3d_clk_enable,
	.get_rate	= eys3d_clk_get_rate,
	.set_rate	= eys3d_clk_set_rate,
};

static int eys3d_clk_probe(struct udevice *dev)
{
	int ret;
	struct eys3d_clk *priv = dev_get_priv(dev);
	struct clk extclk = { .id = 0, };
	struct udevice *extclk_dev;

	ret = uclass_get_device_by_name(UCLASS_CLK, "osc0", &extclk_dev);
	if (ret < 0) {
		pr_err("Failed to find extclk(osc0) node. Check device tree\n");
		return ret;
	}
	clk_request(extclk_dev, &extclk);
	extclk_rate = clk_get_rate(&extclk);



	priv->base = (void *)devfdt_get_addr(dev);
	clkc_dev = dev;

	return sp_clkc_init();
}

static const struct udevice_id eys3d_clk_ids[] = {
	{ .compatible = "eys3d,sp-clkc" },
	{ .compatible = "eys3d,ecv5546-clkc" },
	{ }
};

U_BOOT_DRIVER(eys3d_clk) = {
	.name		= "eys3d_clk",
	.id		= UCLASS_CLK,
	.of_match	= eys3d_clk_ids,
	.priv_auto	= sizeof(struct eys3d_clk),
	.ops		= &eys3d_clk_ops,
	.probe		= eys3d_clk_probe,
};

#ifdef SP_CLK_TEST
#include <reset.h>

void sp_clk_test(void)
{
	struct udevice *dev;
	struct clk clk;
	struct clk_bulk clks;
	struct reset_ctl_bulk resets;
	int ret;

	printf("===== SP_CLK_TEST: get & show uart0 clocks\n");
#ifdef CONFIG_TARGET_PENTAGRAM_COMMON
	ret = uclass_get_device_by_name(UCLASS_SERIAL, "serial@9c000900", &dev);
#else
	ret = uclass_get_device_by_name(UCLASS_SERIAL, "serial@f8000900", &dev);
#endif
	if (!ret) {
		ret = clk_get_bulk(dev, &clks);
		if (ret)
			pr_err("clk_get_bulk failed!");
		else
			for (int i = 0; i < clks.count; i++)
				sp_clk_dump(&clks.clks[i]);

		ret = reset_get_bulk(dev, &resets);
		if (ret)
			pr_err("reset_get_bulk failed!");
	}

	printf("===== SP_CLK_TEST: get & show clock #0\n");
	ret = eys3d_clk_get_by_index(0, &clk);
	if (!ret) {
		sp_clk_dump(&clk);
		clk_free(&clk);
	}
}
#else
#define sp_clk_test()
#endif

int set_cpu_clk_info(void)
{
	struct udevice *dev;
	struct uclass *uc;
	int ret;

	ret = uclass_get(UCLASS_CLK, &uc);
	if (ret) {
		pr_err("Failed to find clocks node. Check device tree\n");
		return ret;
	}

	uclass_foreach_dev(dev, uc) {
		device_probe(dev);
	}
	sp_plls_dump();

#ifdef CONFIG_RESET_EYS3D
	ret = uclass_get_device(UCLASS_RESET, 0, &dev);
	if (ret) {
		pr_err("Failed to find reset node. Check device tree\n");
		return ret;
	}
	printf("reset: %s\n", dev->name);
#endif
	sp_clk_test();

	return ret;
}
