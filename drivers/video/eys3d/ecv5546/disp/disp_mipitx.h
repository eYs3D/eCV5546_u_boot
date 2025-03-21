// SPDX-License-Identifier: GPL-2.0+
/*
 * Author(s): Hammer Hsieh <eys3d@eys3d.com>
 */

#ifndef __DISP_MIPITX_H__
#define __DISP_MIPITX_H__

void DRV_mipitx_pllclk_init(void);
void DRV_mipitx_pllclk_set(int mode, int width, int height);
void DRV_mipitx_Init(int is_mipitx, int width, int height);
void DRV_mipitx_Init_1(int is_mipitx, int width, int height);
void DRV_mipitx_panel_Init(int is_mipi_dsi_tx, int width, int height);

void DRV_mipitx_gpio_set(struct ecv5546_disp_priv *priv);

#endif	//__DISP_MIPITX_H__
