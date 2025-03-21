// SPDX-License-Identifier: GPL-2.0

#include <common.h>
#include <dm.h>
#include <dm/devres.h>

#include "pinctrl_eys3d.h"
#include <mach/gpio_drv.h>
#include <dt-bindings/pinctrl/sppctl-ecv5546.h>

int sp_gpio_input_invert_set(u32 offset, u32 value)
{
	u32 idx, pos;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	pos = offset & 0x0f;
	if (value)
		GPIO_I_INV(idx) = 0x10001 << pos;
	else
		GPIO_I_INV(idx) = 0x10000 << pos;

	return 0;
}

int sp_gpio_input_invert_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	mask = 1 << (offset & 0x0f);
	*value = (GPIO_I_INV(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_output_invert_set(u32 offset, u32 value)
{
	u32 idx, pos;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	pos = offset & 0x0f;
	if (value)
		GPIO_O_INV(idx) = 0x10001 << pos;
	else
		GPIO_O_INV(idx) = 0x10000 << pos;

	return 0;
}

int sp_gpio_output_invert_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	mask = 1 << (offset & 0x0f);
	*value = (GPIO_O_INV(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_open_drain_set(u32 offset, u32 value)
{
	u32 idx, pos;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	pos = offset & 0x0f;
	if (value)
		GPIO_OD(idx) = 0x10001 << pos;
	else
		GPIO_OD(idx) = 0x10000 << pos;

	return 0;
}

int sp_gpio_open_drain_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	mask = 1 << (offset & 0x0f);
	*value = (GPIO_OD(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_first_set(u32 offset, u32 value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 32;
	mask = 1 << (offset & 0x1f);
	if (value)
		GPIO_FIRST(idx) |= mask;
	else
		GPIO_FIRST(idx) &= ~mask;

	return 0;
}

int sp_gpio_first_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 32;
	mask = 1 << (offset & 0x1f);
	*value = (GPIO_FIRST(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_master_set(u32 offset, u32 value)
{
	u32 idx, pos;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	pos = offset & 0x0f;
	if (value)
		GPIO_MASTER(idx) = 0x10001 << pos;
	else
		GPIO_MASTER(idx) = 0x10000 << pos;

	return 0;
}

int sp_gpio_master_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	mask = 1 << (offset & 0x0f);
	*value = (GPIO_MASTER(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_oe_set(u32 offset, u32 value)
{
	u32 idx, pos;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	pos = offset & 0x0f;
	if (value)
		GPIO_OE(idx) = 0x10001 << pos;
	else
		GPIO_OE(idx) = 0x10000 << pos;

	return 0;
}

int sp_gpio_oe_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	mask = 1 << (offset & 0x0f);
	*value = (GPIO_OE(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_out_set(u32 offset, u32 value)
{
	u32 idx, pos;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	pos = offset & 0x0f;
	if (value)
		GPIO_OUT(idx) = 0x10001 << pos;
	else
		GPIO_OUT(idx) = 0x10000 << pos;

	return 0;
}

int sp_gpio_out_get(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 16;
	mask = 1 << (offset & 0x0f);
	*value = (GPIO_OUT(idx) & mask) ? 1 : 0;

	return 0;
}

int sp_gpio_in(u32 offset, u32 *value)
{
	u32 idx, mask;

	if (offset >= MAX_PINS)
		return -EINVAL;

	idx = offset / 32;
	mask = 1 << (offset & 0x1f);
	*value = (GPIO_IN(idx) & mask) ? 1 : 0;

	return 0;
}

u32 sp_gpio_para_get(u32 offset)
{
	u32 value_tmp = 0;
	u32 value;

	//F M I II O OI OE OD
	sp_gpio_first_get(offset, &value); // First value
	value_tmp |= (value << 7);
	sp_gpio_master_get(offset, &value); // Master value
	value_tmp |= (value << 6);
	sp_gpio_in(offset, &value); // Input value
	value_tmp |= (value << 5);
	sp_gpio_input_invert_get(offset, &value); // Input invert value
	value_tmp |= (value << 4);
	sp_gpio_out_get(offset, &value); // Output value
	value_tmp |= (value << 3);
	sp_gpio_output_invert_get(offset, &value); // Output invert value
	value_tmp |= (value << 2);
	sp_gpio_oe_get(offset, &value); // Output Enable value
	value_tmp |= (value << 1);
	sp_gpio_open_drain_get(offset, &value); // Open Drain value
	value_tmp |= (value << 0);

	return value_tmp;
}
