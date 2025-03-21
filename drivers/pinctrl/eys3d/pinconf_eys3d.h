/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __PINCONF_EYS3D_H
#define __PINCONF_EYS3D_H

#define PIN_CONFIG_INPUT_INVERT (PIN_CONFIG_END + 1)
#define PIN_CONFIG_OUTPUT_INVERT (PIN_CONFIG_END + 2)
#define PIN_CONFIG_SLEW_RATE_CTRL (PIN_CONFIG_END + 3)
#define PIN_CONFIG_BIAS_STRONG_PULL_UP (PIN_CONFIG_END + 4)

int eys3d_pinconf_set(struct udevice *dev, unsigned int offset,
			unsigned int param, unsigned int argument);

int eys3d_pinconf_group_set(struct udevice *dev, unsigned int group,
			      unsigned int param, unsigned int argument);

#endif /* __PINCONF_EYS3D_H */
