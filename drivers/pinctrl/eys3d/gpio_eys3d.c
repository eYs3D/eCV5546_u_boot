// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <dm/lists.h>
#include <dm/device-internal.h>
#include <asm/global_data.h>
#include <dm/pinctrl.h>
#include <asm/gpio.h>

#include "pinctrl_gpio_ops.h"
#include "pinctrl_eys3d.h"
#include "gpio_eys3d.h"

static inline int eys3d_gpio_request(struct udevice *dev, unsigned int offset,
				       const char *label)
{
	int gmaster;
	int gfirst;

	gfirst = eys3d_gpio_mode_query(dev, offset);

	gmaster = eys3d_gpio_master_query(dev, offset);

	if (1 == gfirst && 1 == gmaster)
		return 0;

	eys3d_gpio_mode_set(dev, offset, 1);
	eys3d_gpio_master_set(dev, offset, 1);

	return 0;
}

static inline int eys3d_gpio_rfree(struct udevice *dev, unsigned int offset)
{
	return 0;
}

static inline int eys3d_gpio_direction_input(struct udevice *dev,
					       unsigned int offset)
{
	return eys3d_gpio_set_direction_input(dev, offset);
}

static inline int eys3d_gpio_direction_output(struct udevice *dev,
						unsigned int offset, int value)
{
	eys3d_gpio_output_enable_set(dev, offset, 1);

	return eys3d_gpio_output_set(dev, offset, value);
}

static inline int eys3d_gpio_get_value(struct udevice *dev,
					 unsigned int offset)
{
	return eys3d_gpio_input_query(dev, offset);
}

static inline int eys3d_gpio_set_value(struct udevice *dev,
					 unsigned int offset, int value)
{
	return eys3d_gpio_output_set(dev, offset, value);
}

static inline int eys3d_gpio_get_function(struct udevice *dev,
					    unsigned int offset)
{
	int direction = eys3d_gpio_direction_query(dev, offset);

	return direction ? GPIOF_INPUT : GPIOF_OUTPUT;
}

#if 0

static int sp_gpio_request(struct udevice *dev, unsigned int offset, const char *label)
{
	int err;

	pctl_info("%s: offset = %u, label = %s\n", __func__, offset, label);

	err = sp_gpio_first_set(offset, 1);
	if (err)
		return err;

	err = sp_gpio_master_set(offset, 1);
	if (err)
		return err;

	return 0;
}

static int sp_gpio_rfree(struct udevice *dev, unsigned int offset)
{
	pctl_info("%s: offset = %u\n", __func__, offset);

	//sp_gpio_first_set(offset, 0);
	//sp_gpio_master_set(offset, 0);

	return 0;
}

static int sp_gpio_get_value(struct udevice *dev, unsigned int offset)
{
	u32 val;
	int err;

	pctl_info("%s: offset = %u\n", __func__, offset);

	err = sp_gpio_in(offset, &val);
	if (err)
		return err;

	return !!val;
}

static int sp_gpio_set_value(struct udevice *dev, unsigned int offset, int val)
{
	pctl_info("%s: offset = %u, val = %d\n", __func__, offset, val);

	return sp_gpio_out_set(offset, !!val);
}

static int sp_gpio_get_function(struct udevice *dev, unsigned int offset)
{
	u32 val;
	int err;

	pctl_info("%s: offset = %u\n", __func__, offset);

	err = sp_gpio_oe_get(offset, &val);
	if (err)
		return err;

	return val ? GPIOF_OUTPUT : GPIOF_INPUT;
}

static int sp_gpio_direction_input(struct udevice *dev, unsigned int offset)
{
	pctl_info("%s: offset = %u\n", __func__, offset);

	return sp_gpio_oe_set(offset, 0);
}

static int sp_gpio_direction_output(struct udevice *dev, unsigned int offset, int val)
{
	pctl_info("%s: offset = %u, val = %d\n", __func__, offset, val);

	sp_gpio_out_set(offset, val);

	return sp_gpio_oe_set(offset, 1);
}

static int sp_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv;

	uc_priv = dev_get_uclass_priv(dev);
	uc_priv->bank_name = "GPIO";
	uc_priv->gpio_count = MAX_PINS;

	return 0;
}

static const struct dm_gpio_ops eys3d_gpio_ops = {
	.request = sp_gpio_request,
	.rfree = sp_gpio_rfree,
	.set_value = sp_gpio_set_value,
	.get_value = sp_gpio_get_value,
	.get_function = sp_gpio_get_function,
	.direction_input = sp_gpio_direction_input,
	.direction_output = sp_gpio_direction_output,
};
#else

static int eys3d_gpio_probe(struct udevice *dev)
{
	struct gpio_dev_priv *uc_priv;

	uc_priv = dev_get_uclass_priv(dev);
	uc_priv->bank_name = "GPIO";
	uc_priv->gpio_count = MAX_PINS;

	return 0;
}

static struct dm_gpio_ops eys3d_gpio_ops = {
	.request = eys3d_gpio_request,
	.rfree = eys3d_gpio_rfree,
	.set_value = eys3d_gpio_set_value,
	.get_value = eys3d_gpio_get_value,
	.get_function = eys3d_gpio_get_function,
	.direction_input = eys3d_gpio_direction_input,
	.direction_output = eys3d_gpio_direction_output,
};

#endif

static struct driver eys3d_gpio_driver = {
	.name = "eys3d_gpio",
	.id = UCLASS_GPIO,
	.probe = eys3d_gpio_probe,
	.ops = &eys3d_gpio_ops,
};

int eys3d_gpio_driver_init(struct udevice *dev)
{
	struct udevice *cdev;
	ofnode node;

	node = dev_ofnode(dev);
	if (ofnode_read_bool(node, "gpio-controller")) {
		if (!lists_uclass_lookup(UCLASS_GPIO)) {
			pctl_err("Cannot find GPIO driver\n");
			return -ENOENT;
		}

		return device_bind(dev, &eys3d_gpio_driver, "eys3d_gpio", 0,
				   node, &cdev);
	}

	return 0;
}
