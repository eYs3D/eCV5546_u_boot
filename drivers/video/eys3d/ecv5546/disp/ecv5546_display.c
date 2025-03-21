// SPDX-License-Identifier: GPL-2.0+
/*
 * MIPI CSI/DSI TX display driver
 *
 * hammer.hsieh <eys3d@eys3d.com>
 */

#include <common.h>
#include <display.h>
#include <dm.h>
#include <malloc.h>
#include <video.h>
#include <dm/uclass-internal.h>
#include <linux/io.h>
#include "display2.h"
#include "reg_disp.h"
#include "disp_dmix.h"
#include "disp_tgen.h"
#include "disp_tcon.h"
#include "disp_vpp.h"
#include "disp_osd.h"
#include "disp_mipitx.h"
#include <asm/gpio.h>

#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
#include "disp_i2c_lt8912b.h"
#elif CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_RASPBERRYPI_DSI_PANEL)
#include "disp_i2c_raspberrypi.h"
#endif

//#define debug printf

extern u32 osd0_header[];
struct ecv5546_disp_priv *sp_gpio;

#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_RASPBERRYPI_DSI_PANEL)
extern int raspberrypi_check_exist(struct udevice *p1);
extern void raspberrypi_pre_init(struct udevice *p1);
extern void raspberrypi_post_init(struct udevice *p1);
#endif
#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
extern void lt8912_write_init_config(struct udevice *p1);
extern void lt8912_write_mipi_basic_config(struct udevice *p2);
extern void lt8912_write_param_by_resolution(struct udevice *p2, int w, int h);
extern void lt8912_write_dds_config(struct udevice *p2);
extern void lt8912_write_rxlogicres_config(struct udevice *p1);
extern void lt8912_write_lvds_config(struct udevice *p2);
extern void lt8912_hdmi_mode_setup(struct udevice *p1, int hdmi_dvi_sel);
extern void lt8912_audio_setup(struct udevice *p1, struct udevice *p3, int w, int h);
#endif

DECLARE_GLOBAL_DATA_PTR;

static int ecv5546_display_probe(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);
	struct video_priv *uc_priv = dev_get_uclass_priv(dev);
	int max_bpp = 8; //default 8BPP format
	int is_mipi_dsi_tx = 1; //default MIPI_DSI_TX out
	int width, height;
	void *fb_alloc;
	u32 osd_base_addr;
#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
	int i;
	unsigned int lt8912b_hdmi_dvi_sel = 0;
#elif CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_RASPBERRYPI_DSI_PANEL)
	int i;
#endif
	struct ecv5546_disp_priv *priv = dev_get_priv(dev);
	int ret;

	width = CONFIG_VIDEO_eCV5546_MAX_XRES;
	height = CONFIG_VIDEO_eCV5546_MAX_YRES;

	printf("Disp: probe ... \n");

	DISP_MOON2_REG->sft_cfg[3] = 0xffffffff; //enable all disp clken

	printf("Disp: init %dx%d settings\n", width, height);
#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
	if ((CONFIG_VIDEO_eCV5546_MAX_XRES == 480) && (CONFIG_VIDEO_eCV5546_MAX_YRES == 272)){
		width = 720;
		height = 480;
	}
#endif

	#if 0
	/*
	 * refer to kernel dts reserved-memory disp_reserve
	 */
	fb_alloc = (void *)0x5c000000;
	#else
	/*
	 * alloc memory in uboot
	 */
	fb_alloc = malloc((width*height*
					(CONFIG_VIDEO_eCV5546_MAX_BPP >> 3)) + 64 );
	#endif

	//printf("Disp: fb_alloc = 0x%p\n", fb_alloc);
	//printf("Disp: fb_alloc = %ld\n", sizeof(fb_alloc));

	if (fb_alloc == NULL) {
		printf("Error: malloc in %s failed! \n",__func__);
		return -EINVAL;
	}

	if(((uintptr_t)fb_alloc & 0xffffffff00000000) != 0) {
		printf("Error: malloc addr over 4GB(%s)\n",__func__);
		return -EINVAL;
	}

	if(((uintptr_t)fb_alloc & 0x3f) != 0)
		fb_alloc = (void *)(((uintptr_t)fb_alloc + 64 ) & ~0x3f);

	DRV_DMIX_Init();
	DRV_TGEN_Init(width, height);
	DRV_TCON_Init(width, height);
	DRV_VPP_Init(width, height);
	DRV_OSD_Init(width, height);

#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
	lt8912b_hdmi_dvi_sel = dev_read_u32_default(dev,"eys3d,lt8912b_hdmi_mode", 0);
	//printf("Disp: eys3d,lt8912b_hdmi_mode %d \n", lt8912b_hdmi_dvi_sel);
#elif CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_RASPBERRYPI_DSI_PANEL)
#else
	if (is_mipi_dsi_tx) {
		ret = gpio_request_by_name(dev, "reset-gpios", 0, &priv->reset, GPIOD_IS_OUT);
		if (ret) {
			printf("reset-gpios not found\n");
			if (ret != -ENOENT)
				return ret;
		}
	}
#endif

	sp_gpio = priv;

#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
	DRV_mipitx_Init_1(is_mipi_dsi_tx, width, height);
#else
	DRV_mipitx_Init(is_mipi_dsi_tx, width, height);
#endif

	//osd_base_addr = (u32)((uintptr_t)fb_alloc & 0x00000000ffffffff);
	osd_base_addr = (u32)((uintptr_t)fb_alloc);

	if(CONFIG_VIDEO_eCV5546_MAX_BPP == 8) {
		API_OSD_UI_Init(width ,height, osd_base_addr, DRV_OSD_REGION_FORMAT_8BPP);
		max_bpp = 8;
	}
	else if(CONFIG_VIDEO_eCV5546_MAX_BPP == 16) {
		API_OSD_UI_Init(width ,height, osd_base_addr, DRV_OSD_REGION_FORMAT_RGB_565);
		max_bpp = 16;
	}
	else if(CONFIG_VIDEO_eCV5546_MAX_BPP == 32) {
		API_OSD_UI_Init(width ,height, osd_base_addr, DRV_OSD_REGION_FORMAT_ARGB_8888);
		max_bpp = 32;
	}
	else {
		//printf("only support 8/16/32\n",);
		API_OSD_UI_Init(width ,height, osd_base_addr, DRV_OSD_REGION_FORMAT_8BPP);
		max_bpp = 8;
	}

	//printf("G189.02 0x%08x\n", G189_OSD0_REG->osd_base_addr);
#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
	if ((CONFIG_VIDEO_eCV5546_MAX_XRES == 480) && (CONFIG_VIDEO_eCV5546_MAX_YRES == 272)){
		width = 480;
		height = 272;
	}
#endif

	uc_plat->base = (ulong)fb_alloc;
	uc_plat->size = width * height * (max_bpp >> 3);

	uc_priv->xsize = width;
	uc_priv->ysize = height;
	uc_priv->rot = CONFIG_VIDEO_eCV5546_ROTATE;

	if(CONFIG_VIDEO_eCV5546_MAX_BPP == 16) {
		uc_priv->bpix = VIDEO_BPP16;
		uc_priv->format = VIDEO_FMT_RGB565;
	}
	else if(CONFIG_VIDEO_eCV5546_MAX_BPP == 32) {
		uc_priv->bpix = VIDEO_BPP32;
		uc_priv->format = VIDEO_FMT_ARGB8888;
	}
	else {
		uc_priv->bpix = VIDEO_BPP8;
		uc_priv->format = VIDEO_FMT_8BPP_ARGB;
	}

	#ifdef CONFIG_BMP_8BPP_UPDATE_CMAP
	if(uc_priv->bpix == VIDEO_BPP8) {
		uc_priv->cmap = (u32 *)(uintptr_t)(osd0_header+32);
	}
	#endif

#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_RASPBERRYPI_DSI_PANEL)
	for (i = 0; i < 8; i++) {
		ret = i2c_get_chip_for_busnum(i, ATMEL_MCU_I2C_ADDR, 1, &priv->chip1);
		//printf("i2c bus%d chip1 scan ret %d\n", i, ret);
		if (ret) {
			//printf("i2c bus%d chip1 scan ret %d\n", i, ret);
			if (i == 7) {
				printf("Disp: dsi panel not found\n");
				goto skip_attiny88;
			}
		} else {
			break;
		}
	}

	ret = raspberrypi_check_exist(priv->chip1);
	//if (ret)
	//	goto skip_attiny88;

	raspberrypi_pre_init(priv->chip1);

	DRV_mipitx_Init(is_mipi_dsi_tx, width, height);

	raspberrypi_post_init(priv->chip1);

	printf("Disp: init dsi panel RASPBERRYPI_DSI_PANEL\n");
	//printf("MIPITX DSI Panel : RASPBERRYPI_DSI_PANEL(%dx%d)\n", width, height);

skip_attiny88:

#endif
#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_LT8912B_BRIDGE)
	for (i = 0; i < 8; i++) {
		ret = i2c_get_chip_for_busnum(i, LT8912B_I2C_ADDR_MAIN, 1, &priv->chip1);
		if (ret) {
			//printf("i2c bus%d chip1 scan ret %d\n", i, ret);
			if (i == 7) {
				printf("Disp: lt8912b bridge not found\n");
				goto skip_lt8912b;
			} else {
				continue;
			}
		}

		ret = i2c_get_chip_for_busnum(i, LT8912B_I2C_ADDR_CEC_DSI, 1, &priv->chip2);
		if (ret) {
			//printf("i2c bus%d chip2 scan ret %d\n", i, ret);
			return ret;
		} else {
			;//break;
		}

		ret = i2c_get_chip_for_busnum(i, LT8912B_I2C_ADDR_HDMITX_DSI, 1, &priv->chip3);
		if (ret) {
			//printf("i2c bus%d chip3 scan ret %d\n", i, ret);
			return ret;
		} else {
			break;
		}
	}

	if ((CONFIG_VIDEO_eCV5546_MAX_XRES == 480) && (CONFIG_VIDEO_eCV5546_MAX_YRES == 272)){
		lt8912b_hdmi_dvi_sel = 0; //dvi mode only
		width = 720;
		height = 480;
	}

	if (lt8912b_hdmi_dvi_sel)
		printf("Disp:: init lt8912b bridge ic\n");
	else
		printf("Disp: init lt8912b bridge ic\n");

	lt8912_write_init_config(priv->chip1);

	lt8912_write_mipi_basic_config(priv->chip2);

	lt8912_write_param_by_resolution(priv->chip2, width, height);

	lt8912_write_dds_config(priv->chip2);

	lt8912_write_rxlogicres_config(priv->chip1);

	lt8912_write_lvds_config(priv->chip2);

	if (lt8912b_hdmi_dvi_sel)
		lt8912_hdmi_mode_setup(priv->chip1, 1); //0: DVI Mode, (1: HDMI Mode)
	else
		lt8912_hdmi_mode_setup(priv->chip1, 0); //(0: DVI Mode), 1: HDMI Mode

	lt8912_audio_setup(priv->chip1, priv->chip3, width, height);

skip_lt8912b:

#endif

	video_set_flush_dcache(dev, true);

	printf("Disp: probe done \n");

	return 0;
}

static int ecv5546_display_bind(struct udevice *dev)
{
	struct video_uc_plat *uc_plat = dev_get_uclass_plat(dev);

	uc_plat->size = 0;

	return 0;
}

static const struct video_ops ecv5546_display_ops = {
};

static const struct udevice_id ecv5546_display_ids[] = {
	{ .compatible = "eys3d,ecv5546-display" },
	{ .compatible = "eys3d,ecv5546-display-subsystem" },
	{ }
};

U_BOOT_DRIVER(ecv5546_display) = {
	.name	= "ecv5546_display",
	.id	= UCLASS_VIDEO,
	.ops	= &ecv5546_display_ops,
	.of_match = ecv5546_display_ids,
	.bind	= ecv5546_display_bind,
	.probe	= ecv5546_display_probe,
	.priv_auto	= sizeof(struct ecv5546_disp_priv),
};

