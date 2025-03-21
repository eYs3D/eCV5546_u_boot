// SPDX-License-Identifier: GPL-2.0

#include <asm/global_data.h>
#include <dm/device.h>
#include <dm/pinctrl.h>
#include <linux/errno.h>
#include "pinctrl_gpio_ops.h"
#include "pinctrl_eys3d.h"
#include "pinconf_eys3d.h"
#include "ecv5546_func_group.h"

static inline int eys3d_pinconf_output_enable(struct udevice *dev,
						unsigned int offset,
						unsigned int argument)
{
	if (argument == 1)
		return eys3d_gpio_output_enable_set(dev, offset, 1);

	return eys3d_gpio_output_enable_set(dev, offset, 0);
}

static inline int eys3d_pinconf_output(struct udevice *dev,
					 unsigned int offset,
					 unsigned int argument)
{
	eys3d_gpio_output_enable_set(dev, offset, 1);

	if (argument == 1)
		return eys3d_gpio_output_set(dev, offset, 1);

	return eys3d_gpio_output_set(dev, offset, 0);
}

static inline int eys3d_pinconf_output_invert(struct udevice *dev,
						unsigned int offset,
						unsigned int argument)
{
	if (argument == 1)
		return eys3d_gpio_output_invert_set(dev, offset, 1);

	return eys3d_gpio_output_invert_set(dev, offset, 0);
}

static inline int eys3d_pinconf_input_invert(struct udevice *dev,
					       unsigned int offset,
					       unsigned int argument)
{
	if (argument == 1)
		return eys3d_gpio_input_invert_set(dev, offset, 1);

	return eys3d_gpio_input_invert_set(dev, offset, 0);
}

static inline int eys3d_pinconf_open_drain(struct udevice *dev,
					     unsigned int offset)
{
	return eys3d_gpio_open_drain_set(dev, offset, 1);
}

static inline int eys3d_pinconf_drive_strength(struct udevice *dev,
						 unsigned int offset,
						 unsigned int argument)
{
	return eys3d_gpio_drive_strength_set(dev, offset, argument);
}

static inline int eys3d_pinconf_input_enable(struct udevice *dev,
					       unsigned int offset,
					       unsigned int argument)
{
	if (argument == 1)
		return eys3d_gpio_input_enable_set(dev, offset, 1);

	return eys3d_gpio_input_enable_set(dev, offset, 0);
}

static inline int eys3d_pinconf_schmitt_trigger_enable(struct udevice *dev,
							 unsigned int offset,
							 unsigned int argument)
{
	if (argument == 0)
		return eys3d_gpio_schmitt_trigger_set(dev, offset, 0);

	return eys3d_gpio_schmitt_trigger_set(dev, offset, 1);
}

static inline int eys3d_pinconf_high_impedance(struct udevice *dev,
						 unsigned int offset)
{
	return eys3d_gpio_high_impedance(dev, offset);
}

static inline int eys3d_pinconf_bias_pull_up(struct udevice *dev,
					       unsigned int offset,
					       unsigned int argument)
{
	return eys3d_gpio_pull_up(dev, offset);
}

static inline int eys3d_pinconf_bias_strong_pull_up(struct udevice *dev,
						      unsigned int offset,
						      unsigned int argument)
{
	return eys3d_gpio_strong_pull_up(dev, offset);
}

static inline int eys3d_pinconf_bias_pull_down(struct udevice *dev,
						 unsigned int offset,
						 unsigned int argument)
{
	return eys3d_gpio_pull_down(dev, offset);
}

static inline int eys3d_pinconf_bias_disable(struct udevice *dev,
					       unsigned int offset)
{
	return eys3d_gpio_bias_disable(dev, offset);
}

int eys3d_pinconf_set(struct udevice *dev, unsigned int offset,
			unsigned int param, unsigned int argument)
{
	int ret = 0;

	switch (param) {
	case PIN_CONFIG_OUTPUT_ENABLE:
		pctl_info("GPIO[%d]:output %s\n", offset,
			  argument == 1 ? "enable" : "disable");
		ret = eys3d_pinconf_output_enable(dev, offset, argument);
		break;
	case PIN_CONFIG_OUTPUT:
		pctl_info("GPIO[%d]:output %s\n", offset,
			  argument == 1 ? "high" : "low");
		ret = eys3d_pinconf_output(dev, offset, argument);
		break;
	case PIN_CONFIG_OUTPUT_INVERT:
		pctl_info("GPIO[%d]: %s output\n", offset,
			  argument == 1 ? "invert" : "normalize");
		ret = eys3d_pinconf_output_invert(dev, offset, argument);
		break;
	case PIN_CONFIG_DRIVE_OPEN_DRAIN:
		pctl_info("GPIO[%d]:open drain\n", offset);
		ret = eys3d_pinconf_open_drain(dev, offset);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH:
		pctl_info("GPIO[%d]:drive strength %dmA\n", offset, argument);
		ret = eys3d_pinconf_drive_strength(dev, offset,
						     argument * 1000);
		break;
	case PIN_CONFIG_DRIVE_STRENGTH_UA:
		pctl_info("GPIO[%d]:drive strength %duA\n", offset, argument);
		ret = eys3d_pinconf_drive_strength(dev, offset, argument);
		break;
	case PIN_CONFIG_INPUT_ENABLE:
		pctl_info("GPIO[%d]:input %s\n", offset,
			  argument == 0 ? "disable" : "enable");
		ret = eys3d_pinconf_input_enable(dev, offset, argument);
		break;
	case PIN_CONFIG_INPUT_INVERT:
		pctl_info("GPIO[%d]: %s input\n", offset,
			  argument == 1 ? "invert" : "normalize");
		ret = eys3d_pinconf_input_invert(dev, offset, argument);
		break;
	case PIN_CONFIG_INPUT_SCHMITT_ENABLE:
		pctl_info("GPIO[%d]:%s schmitt trigger\n", offset,
			  argument == 0 ? "disable" : "enable");
		ret = eys3d_pinconf_schmitt_trigger_enable(dev, offset,
							     argument);
		break;
	case PIN_CONFIG_SLEW_RATE_CTRL:
		pctl_info("GPIO[%d]:%s slew rate control\n", offset,
			  argument == 0 ? "disable" : "enable");
		eys3d_gpio_slew_rate_control_set(dev, offset, argument);
		break;
	case PIN_CONFIG_BIAS_HIGH_IMPEDANCE:
		pctl_info("GPIO[%d]:high-Z\n", offset);
		ret = eys3d_pinconf_high_impedance(dev, offset);
		break;
	case PIN_CONFIG_BIAS_PULL_UP:
		pctl_info("GPIO[%d]:pull up\n", offset);
		ret = eys3d_pinconf_bias_pull_up(dev, offset, argument);
		break;
	case PIN_CONFIG_BIAS_STRONG_PULL_UP:
		pctl_info("GPIO[%d]:strong pull up\n", offset);
		ret = eys3d_pinconf_bias_strong_pull_up(dev, offset,
							  argument);
		break;
	case PIN_CONFIG_BIAS_PULL_DOWN:
		pctl_info("GPIO[%d]:pull down\n", offset);
		ret = eys3d_pinconf_bias_pull_down(dev, offset, argument);
		break;
	case PIN_CONFIG_BIAS_DISABLE:
		pctl_info("GPIO[%d]:bias disable\n", offset);
		ret = eys3d_pinconf_bias_disable(dev, offset);
		break;
	default:
		pctl_err("GPIO[%d]:param:0x%x is not supported\n", offset,
			 param);
		break;
	}

	return ret;
}

int eys3d_pinconf_group_set(struct udevice *dev, unsigned int group,
			      unsigned int param, unsigned int argument)
{
	const char *group_name;
	struct func_t *func;
	int func_list_nums;
	unsigned int pin;
	int i;
	int j;
	int k;

	func_list_nums = eys3d_get_function_count();

	for (i = 0; i < func_list_nums; i++) {
		func = eys3d_get_function_by_selector(i);
		for (j = 0; j < func->gnum; j++) {
			group_name = eys3d_get_group_name_by_selector(group);
			if (strcmp(func->grps[j].name, group_name) == 0) {
				for (k = 0; k < func->grps[j].pnum; k++) {
					pctl_info(
						"(%s:%d)grp%d.name[%s]pin[%d]\n",
						__func__, __LINE__, group,
						func->grps[j].name,
						func->grps[j].pins[k]);

					pin = func->grps[j].pins[k];
					eys3d_pinconf_set(dev, pin, param,
							    argument);
				}
			}
		}
	}

	return 0;
}
