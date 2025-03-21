// SPDX-License-Identifier: GPL-2.0
/*
 * Reset Controller driver
 *
 * Author: qinjian <eys3d@eys3d.com>
 */

#include <common.h>
#include <asm/io.h>
#include <dm.h>
#include <reset-uclass.h>
#include <dt-bindings/reset/ecv5546.h>

#define BITASSERT(id, val)	(BIT(16 + (id)) | ((val) << (id)))

struct sp_reset_priv {
	void *membase;
};

static int sp_reset_update(struct reset_ctl *reset_ctl, int assert)
{
	struct sp_reset_priv *priv = dev_get_priv(reset_ctl->dev);
	int reg_width = sizeof(u32)/2;
	int bank = reset_ctl->id / (reg_width * BITS_PER_BYTE);
	int offset = reset_ctl->id % (reg_width * BITS_PER_BYTE);
	void *addr;

	addr = priv->membase + (bank * 4);
	writel(BITASSERT(offset, assert), addr);

	return 0;
}

static int sp_reset_status(struct reset_ctl *reset_ctl)
{
	struct sp_reset_priv *priv = dev_get_priv(reset_ctl->dev);
	int reg_width = sizeof(u32)/2;
	int bank = reset_ctl->id / (reg_width * BITS_PER_BYTE);
	int offset = reset_ctl->id % (reg_width * BITS_PER_BYTE);
	u32 reg;

	reg = readl(priv->membase + (bank * 4));

	return !!(reg & BIT(offset));
}

static int sp_reset_assert(struct reset_ctl *reset_ctl)
{
	return sp_reset_update(reset_ctl, 1);
}

static int sp_reset_deassert(struct reset_ctl *reset_ctl)
{
	return sp_reset_update(reset_ctl, 0);
}

static int sp_reset_request(struct reset_ctl *reset_ctl)
{
	return 0;
}

static int sp_reset_free(struct reset_ctl *reset_ctl)
{
	return 0;
}

struct reset_ops sp_reset_ops = {
	.request = sp_reset_request,
	.rfree = sp_reset_free,
	.rst_assert   = sp_reset_assert,
	.rst_deassert = sp_reset_deassert,
	.rst_status   = sp_reset_status,
};

static const struct udevice_id sp_reset_ids[] = {
	{ .compatible = "eys3d,sp-reset" },
	{ .compatible = "eys3d,ecv5546-reset" },
	{ /* sentinel */ }
};

static int sp_reset_probe(struct udevice *dev)
{
	struct sp_reset_priv *priv = dev_get_priv(dev);

	priv->membase = (void *)devfdt_get_addr(dev);

	//printf("!!! %s: %p\n", __FUNCTION__, priv->membase);
	return 0;
}

U_BOOT_DRIVER(sp_reset) = {
	.name = "sp_reset",
	.id = UCLASS_RESET,
	.of_match = sp_reset_ids,
	.probe = sp_reset_probe,
	.ops = &sp_reset_ops,
	.priv_auto = sizeof(struct sp_reset_priv),
};
