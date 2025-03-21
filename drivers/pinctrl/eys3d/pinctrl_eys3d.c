// SPDX-License-Identifier: GPL-2.0

#include <asm-generic/gpio.h>
#include <common.h>
#include <dm.h>
#include <dm/device-internal.h>
#include <dm/lists.h>
#include <dm/pinctrl.h>
#include <linux/io.h>

#include "gpio_eys3d.h"
#include "pinconf_eys3d.h"
#include "pinctrl_gpio_ops.h"
#include "pinctrl_eys3d.h"
#include "pinmux_eys3d.h"
#include "ecv5546_func_group.h"
#include "ecv5546_pins.h"
#include <dt-bindings/pinctrl/sppctl-ecv5546.h>

DECLARE_GLOBAL_DATA_PTR;

//#define PINCTRL_DEBUG

u32 *moon1_regs;
u32 *padctl1_regs;
u32 *first_regs;
u32 *padctl2_regs;
u32 *gpioxt_regs;

#if defined(SUPPORT_GPIO_AO_INT)
u32 *gpio_ao_int_regs;
u32 gpio_ao_int_prescale;
u32 gpio_ao_int_debounce;
int gpio_ao_int_pins[32];
#endif

void *pin_registered_by_udev[MAX_PINS];

#ifdef PINCTRL_DEBUG
void pinmux_grps_dump(void)
{
	struct func_t *func;
	int i = 0;
	int mask;
	int rval;
	int val;

	func = &list_funcs[i];
	for (i = 0; i < list_func_nums; i++) {
		func = &list_funcs[i];

		if (func->gnum == 0)
			continue;

		if (func->freg != F_OFF_G)
			continue;

		mask = (1 << func->blen) - 1;
		rval = GPIO_PINGRP(func->roff);
		val = (rval >> func->boff) & mask;
		if (val == 0)
			continue;

		pctl_info("%s\t=%d regval:%X\n", list_funcs[i].name, val, rval);
	}
}

void gpio_reg_dump(void)
{
	u32 gpio_value[MAX_PINS];
	int i;

	pctl_info("%s (FI MA IN II OU OI OE OD)\n", __func__);
	for (i = 0; i < MAX_PINS; i++) {
		gpio_value[i] = sp_gpio_para_get(i);
		if ((i % 8) == 7) {
			pctl_info(
				"GPIO_P%-2d 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x\n",
				(i / 8), gpio_value[i - 7], gpio_value[i - 6],
				gpio_value[i - 5], gpio_value[i - 4],
				gpio_value[i - 3], gpio_value[i - 2],
				gpio_value[i - 1], gpio_value[i]);
		} else if (i == (MAX_PINS - 1)) {
			pctl_info("GPIO_P%-2d 0x%02x 0x%02x 0x%02x 0x%02x\n",
				  (i / 8), gpio_value[i - 3], gpio_value[i - 2],
				  gpio_value[i - 1], gpio_value[i]);
		}
	}
}
#endif

static void *sp_register_pin(int pin, struct udevice *dev)
{
	// Check if pin number is within range.
	if (pin >= 0 && pin < MAX_PINS) {
		if (pin_registered_by_udev[pin] == 0) {
			// If the pin has not been registered, register it by
			// save a pointer to the registering device.
			pin_registered_by_udev[pin] = dev;
			pctl_info("Register pin %d (from node: %s).\n", pin,
				  dev->name);

			return 0;
		}
		pctl_err(
			"ERROR: Pin %d of node %s has been registered (by node: %s)!\n",
			pin, dev->name,
			((struct udevice *)pin_registered_by_udev[pin])->name);
		return dev;
	}
	pctl_err("ERROR: Invalid pin number %d from '%s'!\n", pin, dev->name);
	return (void *)-1;
}

static int sp_unregister_pin(int pin)
{
	// Check if pin number is within range.
	if (pin >= 0 && pin < MAX_PINS)
		if (pin_registered_by_udev[pin]) {
			pin_registered_by_udev[pin] = 0;
			return 0;
		}

	return -1;
}

static int eys3d_pinctrl_pins(struct udevice *dev)
{
	int offset = dev_of_offset(dev);
	u32 pin_mux[MAX_PINS];
	int len, i;

	// Get property: "eys3d,pins"
	len = fdtdec_get_int_array_count(gd->fdt_blob, offset, "eys3d,pins",
					 pin_mux, ARRAY_SIZE(pin_mux));
	if (len > 0)
		goto found_pins;

#ifndef DISABLE_CONFLICT_CODE_WITH_GENERIC_USAGE
	// Get property: "sppctl,pins"
	len = fdtdec_get_int_array_count(gd->fdt_blob, offset, "sppctl,pins",
					 pin_mux, ARRAY_SIZE(pin_mux));
	if (len > 0)
		goto found_pins;

	// Get property: "pins"
	len = fdtdec_get_int_array_count(gd->fdt_blob, offset, "pins", pin_mux,
					 ARRAY_SIZE(pin_mux));
#endif

	if (len <= 0)
		return 0;

found_pins:
	pctl_info("Number of entries of 'pins' = %d\n", len);

	// Register all pins.
	for (i = 0; i < len; i++) {
		int pin = (pin_mux[i] >> 24) & 0xff;

		if (sp_register_pin(pin, dev) != 0)
			break;
	}

	// If any pin was not registered successfully, return -1.
	if (len > 0 && i != len)
		return -1;

	// All pins were registered successfully, set up all pins.
	for (i = 0; i < len; i++) {
		int pins = pin_mux[i];
		int pin = (pins >> 24) & 0xff;
		int type = (pins >> 16) & 0xff;
		int flag = pins & 0xff;

		pctl_info("pins = 0x%08x\n", pins);

		if (type == SPPCTL_PCTL_G_IOPP) {
			// It's a IOP pin.
			sp_gpio_first_set(pin, 1);
			sp_gpio_master_set(pin, 0);
		} else if (type == SPPCTL_PCTL_G_GPIO) {
			// It's a GPIO pin.
			sp_gpio_first_set(pin, 1);
			sp_gpio_master_set(pin, 1);

			if (flag & (SPPCTL_PCTL_L_OUT | SPPCTL_PCTL_L_OU1)) {
				if (flag & SPPCTL_PCTL_L_OUT)
					sp_gpio_out_set(pin, 0);

				if (flag & SPPCTL_PCTL_L_OU1)
					sp_gpio_out_set(pin, 1);

				sp_gpio_oe_set(pin, 1);
			} else if (flag & SPPCTL_PCTL_L_ODR) {
				sp_gpio_open_drain_set(pin, 1);
			}

			if (flag & SPPCTL_PCTL_L_INV)
				sp_gpio_input_invert_set(pin, 1);
			else
				sp_gpio_input_invert_set(pin, 0);

			if (flag & SPPCTL_PCTL_L_ONV)
				sp_gpio_output_invert_set(pin, 1);
			else
				sp_gpio_output_invert_set(pin, 0);
		}
	}

	return 0;
}

static int eys3d_pinctrl_zero(struct udevice *dev)
{
	int offset = dev_of_offset(dev);
	u32 pin_mux[MAX_PINS];
	int len, i, mask;

	// Get property: "eys3d,zerofunc"
	len = fdtdec_get_int_array_count(gd->fdt_blob, offset,
					 "eys3d,zerofunc", pin_mux,
					 ARRAY_SIZE(pin_mux));
	if (len > 0)
		goto found_zero_func;

	// Get property: "sppctl,zero_func"
	len = fdtdec_get_int_array_count(gd->fdt_blob, offset,
					 "sppctl,zero_func", pin_mux,
					 ARRAY_SIZE(pin_mux));
	if (len > 0)
		goto found_zero_func;

	// Get property: "zero_func"
	len = fdtdec_get_int_array_count(gd->fdt_blob, offset, "zero_func",
					 pin_mux, ARRAY_SIZE(pin_mux));
	if (len <= 0)
		return 0;

found_zero_func:
	pctl_info("Number of entries of 'zero_func' = %d\n", len);

	// All pins were registered successfully, set up all pins.
	for (i = 0; i < len; i++) {
		int func = pin_mux[i];

		pctl_info("zero_func = 0x%08x\n", func);

		// Set it to no use.
		if (func >= list_func_nums) {
			pctl_info("func=%d is out of range, skipping...\n",
				  func);
			continue;
		}

		struct func_t *f = &list_funcs[func];

		switch (f->freg) {
		case F_OFF_G:
			mask = (1 << f->blen) - 1;
			GPIO_PINGRP(f->roff) =
				(mask << (f->boff + 16)) | (0 << f->boff);
			pctl_info("group %s set to 0\n", f->name);
			break;

		default:
			pctl_info("bad zero func/group idx:%d, skipped\n",
				  func);
			break;
		}
	}

	return 0;
}

#ifndef DISABLE_CONFLICT_CODE_WITH_GENERIC_USAGE
static int eys3d_pinctrl_function(struct udevice *dev)
{
	int offset = dev_of_offset(dev);
	const char *pin_group;
	const char *pin_func;
	int len, i;

	// Get property: 'sppctl,function'
	pin_func = fdt_getprop(gd->fdt_blob, offset, "sppctl,function", &len);
	if (pin_func)
		goto found_function;

	// Get property: 'function'
	pin_func = fdt_getprop(gd->fdt_blob, offset, "function", &len);
	if (!pin_func)
		return 0;

found_function:
	pctl_info("function = %s (%d)\n", pin_func, len);
	if (len > 1) {
		// Find 'function' string in list: only groups
		for (i = 0; i < list_func_nums; i++) {
			if (list_funcs[i].gnum == 0)
				continue;

			if (list_funcs[i].freg != F_OFF_G)
				continue;

			if (strcmp(pin_func, list_funcs[i].name) == 0)
				break;
		}
		if (i == list_func_nums) {
			pctl_err(
				"Error: Invalid 'function' in node %s! Cannot find \"%s\"!\n",
				dev->name, pin_func);
			return -1;
		}

		// 'function' is found! Next, find its 'groups'.
		// Get property: 'sppctl,groups'
		pin_group = fdt_getprop(gd->fdt_blob, offset, "sppctl,groups",
					&len);
		if (pin_group)
			goto found_groups;

		// Get property: 'groups'
		pin_group = fdt_getprop(gd->fdt_blob, offset, "groups", &len);
found_groups:
		pctl_info("groups = %s (%d)\n", pin_group, len);
		if (len > 1) {
			struct func_t *func = &list_funcs[i];

			// Find 'pin_group' string in list.
			for (i = 0; i < func->gnum; i++)
				if (strcmp(pin_group, func->grps[i].name) == 0)
					break;

			if (i == func->gnum) {
				pctl_err(
					"Error: Invalid 'groups' in node %s! Cannot find \"%s\"!\n",
					dev->name, pin_group);
				return -1;
			}

			// 'pin_group' is found!
			const struct sppctlgrp_t *grp = &func->grps[i];

			// Register all pins of the group.
			for (i = 0; i < grp->pnum; i++)
				if (sp_register_pin(grp->pins[i], dev) != 0)
					break;

			// Check if all pins of the group register successfully
			if (i == grp->pnum) {
				int mask;

				// All pins of the group was registered successfully.
				// Enable the pin-group.
				mask = (1 << func->blen) - 1;
				GPIO_PINGRP(func->roff) =
					(mask << (func->boff + 16)) |
					(grp->gval << func->boff);
				pctl_info("GPIO_PINGRP[%d] <= 0x%08x\n",
					  func->roff,
					  (mask << (func->boff + 16)) |
						  (grp->gval << func->boff));
			} else {
				return -1;
			}
		} else if (len <= 1) {
			pctl_err("Error: Invalid 'groups' in node %s!\n",
				 dev->name);
			return -1;
		}
	} else if (len == 1) {
		pctl_err("Error: Invalid 'function' in node %s!\n", dev->name);
		return -1;
	}

	return 0;
}
#endif

static int eys3d_pinctrl_get_pins_count(struct udevice *dev)
{
	return eys3d_get_pins_count();
}

static const char *eys3d_pinctrl_get_pin_name(struct udevice *dev,
						unsigned int selector)
{
	return eys3d_get_pin_name_by_selector(selector);
}

static int eys3d_pinctrl_get_groups_count(struct udevice *dev)
{
	return eys3d_get_groups_count();
}

static const char *eys3d_pinctrl_get_group_name(struct udevice *dev,
						  unsigned int selector)
{
	return eys3d_get_group_name_by_selector(selector);
}

static int eys3d_pinctrl_get_functions_count(struct udevice *dev)
{
	return eys3d_get_function_count();
}

static const char *eys3d_pinctrl_get_function_name(struct udevice *dev,
						     unsigned int selector)
{
	return eys3d_get_function_name_by_selector(selector);
}

static int eys3d_pinctrl_pinmux_set(struct udevice *dev,
				      unsigned int pin_selector,
				      unsigned int func_selector)
{
	return eys3d_pinmux_pin_set(dev, pin_selector, func_selector);
}

static int eys3d_pinctrl_pinmux_group_set(struct udevice *dev,
					    unsigned int group_selector,
					    unsigned int func_selector)
{
	return eys3d_pinmux_group_set(dev, group_selector, func_selector);
}

int eys3d_pinctrl_pinconf_set(struct udevice *dev, unsigned int pin_selector,
				unsigned int param, unsigned int argument)
{
	return eys3d_pinconf_set(dev, pin_selector, param, argument);
}

int eys3d_pinctrl_pinconf_group_set(struct udevice *dev,
				      unsigned int group_selector,
				      unsigned int param, unsigned int argument)
{
	return eys3d_pinconf_group_set(dev, group_selector, param, argument);
}

int eys3d_pinctrl_gpio_request_enable(struct udevice *dev,
					unsigned int selector)
{
#ifndef DISABLE_CONFLICT_CODE_WITH_GENERIC_USAGE
	int gfirst, gmaster, offset;
	int ret = 0;

	offset = selector;
	ret = sp_gpio_first_get(offset, &gfirst);
	if (ret)
		return ret;

	ret = sp_gpio_first_get(offset, &gmaster);
	if (ret)
		return ret;

	if (1 == gfirst && 1 == gmaster)
		return 0;

	sp_gpio_first_set(offset, 1);
	sp_gpio_master_set(offset, 1);
#else
	int gmaster;
	int gfirst;
	int offset;

	offset = selector;
	gfirst = eys3d_gpio_mode_query(dev, offset);

	gmaster = eys3d_gpio_master_query(dev, offset);

	if (1 == gfirst && 1 == gmaster)
		return 0;

	eys3d_gpio_mode_set(dev, offset, 1);
	eys3d_gpio_master_set(dev, offset, 1);
#endif
	return 0;
}

int eys3d_pinctrl_gpio_disable_free(struct udevice *dev,
				      unsigned int selector)
{
	return 0;
}

int eys3d_pinctrl_get_pin_muxing(struct udevice *dev, unsigned int selector,
				   char *buf, int size)
{
	size_t length;
	int gmaster;
	int gfirst;

	gfirst = eys3d_gpio_mode_query(dev, selector);
	gmaster = eys3d_gpio_master_query(dev, selector);

	if (1 == gfirst && 1 == gmaster) {
		const char *pinmux = "GPIO";
		size_t length = strlen(pinmux) + 1;

		if (length > size)
			return -EINVAL;

		strncpy(buf, pinmux, length);
		return 0;
	}

	const char *pinmux = eys3d_pinmux_get_pin_muxing_name(dev, selector);

	if (!pinmux)
		return -EINVAL;

	length = strlen(pinmux) + 1;

	if (length > size)
		return -EINVAL;

	strncpy(buf, pinmux, length);

	return 0;
}

static int eys3d_pinctrl_mode_select(struct udevice *dev)
{
	unsigned int value;
	const char *ms_val;
	int ret;

	ret = dev_read_string_index(dev, "eys3d,ms-dvio-group-0", 0,
				    &ms_val);
	if (!ret) {
		if (!strcmp(ms_val, "3V0"))
			value = 0;
		else
			value = 1;

		eys3d_gpio_voltage_mode_select_set(dev, G_MX_MS_TOP_0, value);
	}

	ret = dev_read_string_index(dev, "eys3d,ms-dvio-group-1", 0,
				    &ms_val);
	if (!ret) {
		if (!strcmp(ms_val, "3V0"))
			value = 0;
		else
			value = 1;

		eys3d_gpio_voltage_mode_select_set(dev, G_MX_MS_TOP_1, value);
	}

	ret = dev_read_string_index(dev, "eys3d,ms-dvio-ao-group-0", 0,
				    &ms_val);
	if (!ret) {
		if (!strcmp(ms_val, "3V0"))
			value = 0;
		else
			value = 1;

		eys3d_gpio_voltage_mode_select_set(dev, AO_MX_MS_TOP_0,
						     value);
	}

	ret = dev_read_string_index(dev, "eys3d,ms-dvio-ao-group-1", 0,
				    &ms_val);
	if (!ret) {
		if (!strcmp(ms_val, "3V0"))
			value = 0;
		else
			value = 1;

		eys3d_gpio_voltage_mode_select_set(dev, AO_MX_MS_TOP_1,
						     value);
	}

	ret = dev_read_string_index(dev, "eys3d,ms-dvio-ao-group-2", 0,
				    &ms_val);
	if (!ret) {
		if (!strcmp(ms_val, "3V0"))
			value = 0;
		else
			value = 1;

		eys3d_gpio_voltage_mode_select_set(dev, AO_MX_MS_TOP_2,
						     value);
	}

	return 0;
}

static int eys3d_pinctrl_set_state(struct udevice *dev,
				     struct udevice *config)
{
	int ret;

	pctl_info("\nConfig node: %s\n", config->name);

#ifdef DISABLE_CONFLICT_CODE_WITH_GENERIC_USAGE
	/* generic usage */
	ret = pinctrl_generic_set_state(dev, config);
	if (ret != 0)
		return ret;
#endif

	/* eys3d usage */
	ret = eys3d_pinctrl_pins(config);
	if (ret != 0)
		return ret;

	eys3d_pinctrl_zero(config);

#ifndef DISABLE_CONFLICT_CODE_WITH_GENERIC_USAGE
	ret = eys3d_pinctrl_function(config);
	if (ret != 0)
		return ret;
#endif

#ifdef PINCTRL_DEBUG
	gpio_reg_dump();
	pinmux_grps_dump();
#endif

	return 0;
}

static const struct pinconf_param eys3d_pinconf_params[] = {
	{ "bias-disable", PIN_CONFIG_BIAS_DISABLE, 0 },
	{ "bias-high-impedance", PIN_CONFIG_BIAS_HIGH_IMPEDANCE, 0 },
	{ "bias-pull-up", PIN_CONFIG_BIAS_PULL_UP, 1 },
	{ "bias-pull-down", PIN_CONFIG_BIAS_PULL_DOWN, 1 },
	{ "drive-open-drain", PIN_CONFIG_DRIVE_OPEN_DRAIN, 0 },
	{ "drive-strength", PIN_CONFIG_DRIVE_STRENGTH, 0 },
	{ "drive-strength-microamp", PIN_CONFIG_DRIVE_STRENGTH_UA, 0 },
	{ "input-disable", PIN_CONFIG_INPUT_ENABLE, 0 },
	{ "input-enable", PIN_CONFIG_INPUT_ENABLE, 1 },
	{ "input-schmitt-disable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 0 },
	{ "input-schmitt-enable", PIN_CONFIG_INPUT_SCHMITT_ENABLE, 1 },
	{ "output-disable", PIN_CONFIG_OUTPUT_ENABLE, 0 },
	{ "output-enable", PIN_CONFIG_OUTPUT_ENABLE, 1 },
	{ "output-high", PIN_CONFIG_OUTPUT, 1 },
	{ "output-low", PIN_CONFIG_OUTPUT, 0 },
	{ "eys3d,input-invert-enable", PIN_CONFIG_INPUT_INVERT, 1 },
	{ "eys3d,output-invert-enable", PIN_CONFIG_OUTPUT_INVERT, 1 },
	{ "eys3d,input-invert-disable", PIN_CONFIG_INPUT_INVERT, 0 },
	{ "eys3d,output-invert-disable", PIN_CONFIG_OUTPUT_INVERT, 0 },
	{ "eys3d,slew-rate-control-disable", PIN_CONFIG_SLEW_RATE_CTRL, 0 },
	{ "eys3d,slew-rate-control-enable", PIN_CONFIG_SLEW_RATE_CTRL, 1 },
	{ "eys3d,bias-strong-pull-up", PIN_CONFIG_BIAS_STRONG_PULL_UP, 1 },
};

static struct pinctrl_ops eys3d_pinctrl_ops = {
	.set_state = eys3d_pinctrl_set_state,
	.get_pins_count = eys3d_pinctrl_get_pins_count,
	.get_pin_name = eys3d_pinctrl_get_pin_name,
	.get_groups_count = eys3d_pinctrl_get_groups_count,
	.get_group_name = eys3d_pinctrl_get_group_name,
	.get_functions_count = eys3d_pinctrl_get_functions_count,
	.get_function_name = eys3d_pinctrl_get_function_name,
	.pinmux_set = eys3d_pinctrl_pinmux_set,
	.pinmux_group_set = eys3d_pinctrl_pinmux_group_set,
	.pinconf_num_params = ARRAY_SIZE(eys3d_pinconf_params),
	.pinconf_params = eys3d_pinconf_params,
	.pinconf_set = eys3d_pinctrl_pinconf_set,
	.pinconf_group_set = eys3d_pinctrl_pinconf_group_set,
	.gpio_request_enable = eys3d_pinctrl_gpio_request_enable,
	.gpio_disable_free = eys3d_pinctrl_gpio_disable_free,
	.get_pin_muxing = eys3d_pinctrl_get_pin_muxing,
};

static int eys3d_pinctrl_probe(struct udevice *dev)
{
	int i;
	int ret;

	// Get address of GPIOXT registers.
	gpioxt_regs = (void *)devfdt_get_addr_name(dev, "gpioxt");
	pctl_info("gpioxt_regs = %p\n", gpioxt_regs);
	if (gpioxt_regs == (void *)-1) {
		pctl_err("Failed to get base address of GPIOXT!\n");
		return -EINVAL;
	}

	// Get address of PAD_CTL_2 registers.
	padctl2_regs = (void *)devfdt_get_addr_name(dev, "pad_ctl_2");
	pctl_info("padctl2_regs = %p\n", padctl2_regs);
	if (padctl2_regs == (void *)-1) {
		pctl_err("Failed to get base address of PAD_CTL_2!\n");
		return -EINVAL;
	}

	// Get address of FIRST registers.
	first_regs = (void *)devfdt_get_addr_name(dev, "first");
	pctl_info("first_regs = %p\n", first_regs);
	if (first_regs == (void *)-1) {
		pctl_err("Failed to get base address of FIRST!\n");
		return -EINVAL;
	}

	// Get address of PAD_CTL_1 registers.
	padctl1_regs = (void *)devfdt_get_addr_name(dev, "pad_ctl_1");
	pctl_info("padctl1_regs = %p\n", padctl1_regs);
	if (padctl1_regs == (void *)-1) {
		pctl_err("Failed to get base address of PAD_CTL_1!\n");
		return -EINVAL;
	}

	// Get address of MOON1 registers.
	moon1_regs = (void *)devfdt_get_addr_name(dev, "moon1");
	pctl_info("moon1_regs = %p\n", moon1_regs);
	if (moon1_regs == (void *)-1) {
		pctl_err("Failed to get base address of MOON1!\n");
		return -EINVAL;
	}

	// Unregister all pins.
	for (i = 0; i < MAX_PINS; i++)
		sp_unregister_pin(i);

	// Bind gpio driver.
	ret = eys3d_gpio_driver_init(dev);
	if (ret)
		return ret;

#if defined(SUPPORT_GPIO_AO_INT)
	eys3d_reg_base_addr_set((void *)moon1_regs, (void *)padctl1_regs,
				  (void *)first_regs, (void *)padctl2_regs,
				  (void *)gpioxt_regs,
				  (void *)gpio_ao_int_regs);
#else
	eys3d_reg_base_addr_set((void *)moon1_regs, (void *)padctl1_regs,
				  (void *)first_regs, (void *)padctl2_regs,
				  (void *)gpioxt_regs, (void *)NULL);
#endif

	eys3d_pinmux_resource_init(dev);

	ret = eys3d_pinctrl_mode_select(dev);
	if (ret != 0)
		return ret;

	return 0;
}

static const struct udevice_id eys3d_pinctrl_ids[] = {
	{ .compatible = "eys3d,ecv5546-pctl" },
	{ /* zero */ }
};

U_BOOT_DRIVER(pinctrl_eys3d) = {
	.name = "eys3d_pinctrl",
	.id = UCLASS_PINCTRL,
	.probe = eys3d_pinctrl_probe,
	.of_match = eys3d_pinctrl_ids,
	.ops = &eys3d_pinctrl_ops,
	.bind = dm_scan_fdt_dev,
};
