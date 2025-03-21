/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __PINMUX_EYS3D_H
#define __PINMUX_EYS3D_H

int eys3d_pinmux_pin_set(struct udevice *dev, unsigned int pin_selector,
			   unsigned int func_selector);
int eys3d_pinmux_group_set(struct udevice *dev, unsigned int group_selector,
			     unsigned int func_selector);
const char *eys3d_pinmux_get_pin_muxing_name(struct udevice *dev,
					       unsigned int pin_selector);

int eys3d_pinmux_resource_init(struct udevice *dev);

#endif /* __PINMUX_EYS3D_H */

