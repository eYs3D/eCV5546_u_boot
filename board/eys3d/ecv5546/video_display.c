// SPDX-License-Identifier: GPL-2.0+
/*
 * Hammer Hsieh <eys3d@eys3d.com>
 */

#include <common.h>
#include <dm.h>
#include <version.h>
#include <video.h>
#include <video_console.h>
#include <asm/io.h>

#ifdef CONFIG_VIDEO_BMP_LOGO_MANUAL
#include <ecv5546_logo_data.h>
#else
#ifdef CONFIG_DM_VIDEO_eCV5546_LOGO
#include <dm_video_logo.h>
#include <dm_video_logo_data.h>
#endif
#endif

#include <mapmem.h>
#include <bmp_layout.h>
#include <command.h>
#include <asm/byteorder.h>
#include <malloc.h>
#include <mapmem.h>
#include <splash.h>

DECLARE_GLOBAL_DATA_PTR;

extern int ecv5546_video_show_board_info(void);

int ecv5546_video_show_board_info(void)
{
#if defined(CONFIG_CMD_BMP) && defined(CONFIG_VIDEO_eCV5546) && defined(CONFIG_DM_VIDEO_eCV5546_LOGO)
	struct vidconsole_priv *priv;
#endif
	ulong dram_size;
	int i;
	u32 len = 0;
	char buf[255];
	char *corp = "2024 eYs3D Technology Inc.\n";
	struct udevice *dev, *con;
#if defined(CONFIG_CMD_BMP) && defined(CONFIG_VIDEO_eCV5546) && defined(CONFIG_DM_VIDEO_eCV5546_LOGO)
	const char *s;
#endif
	vidinfo_t logo_info;
	int ret;

	logo_info.logo_width = 0;
	logo_info.logo_height = 0;
	logo_info.logo_x_offset = 0;
	logo_info.logo_y_offset = 0;

	len += sprintf(&buf[len], "%s\n", U_BOOT_VERSION);
	memcpy(&buf[len], corp, strlen(corp));
	len += strlen(corp);

	dram_size = 0;
	for (i = 0; i < CONFIG_NR_DRAM_BANKS; i++)
		dram_size += gd->bd->bi_dram[i].size;
	
	len += sprintf(&buf[len], "eCV5546 EVB BOARD V1.0 \n");

	len += sprintf(&buf[len], "%ld MB SDRAM \n",
		       dram_size >> 20);

	ret = uclass_get_device(UCLASS_VIDEO, 0, &dev);
	if (ret)
		return ret;

	ecv5546_logo_info(&logo_info);

#if defined(CONFIG_CMD_BMP) && defined(CONFIG_VIDEO_eCV5546) && defined(CONFIG_DM_VIDEO_eCV5546_LOGO)
	gd->bmp_logo_addr = (long long)ecv5546_uboot_logo;
	//debug("bmp addr = 0x%08x \n", (long long)ecv5546_uboot_logo);
#ifdef CONFIG_DM_VIDEO_eCV5546_LOGO_ALIGN
	bmp_display((long long)ecv5546_uboot_logo,
		    BMP_ALIGN_CENTER,
			BMP_ALIGN_CENTER);
#else
	bmp_display((long long)ecv5546_uboot_logo,
		    logo_info.logo_x_offset,
			logo_info.logo_y_offset);
#endif /* CONFIG_DM_VIDEO_eCV5546_LOGO_ALIGN */
#endif

	ret = uclass_get_device(UCLASS_VIDEO_CONSOLE, 0, &con);
	if (ret)
		return ret;
	
#if defined(CONFIG_CMD_BMP) && defined(CONFIG_VIDEO_eCV5546)

#ifdef CONFIG_VIDEO_BMP_LOGO_MANUAL
	priv = dev_get_uclass_priv(con);
	vidconsole_position_cursor(con, 0, ( logo_info.logo_height +
					priv->y_charsize * 4 - 1) / priv->y_charsize);
	for (s = buf, i = 0; i < len; s++, i++)
		vidconsole_put_char(con, *s);
#else
#if defined(CONFIG_DM_VIDEO_eCV5546_LOGO)
	priv = dev_get_uclass_priv(con);
	vidconsole_position_cursor(con, 0, ( logo_info.logo_height +
				   priv->y_charsize - 1) / priv->y_charsize);
	for (s = buf, i = 0; i < len; s++, i++)
		vidconsole_put_char(con, *s);
#endif /* CONFIG_DM_VIDEO_eCV5546_LOGO */
#endif /* CONFIG_VIDEO_BMP_LOGO_MANUAL */
#endif

	return 0;
}

void ecv5546_logo_info(vidinfo_t *info)
{
#ifdef CONFIG_DM_VIDEO_eCV5546_LOGO
	info->logo_width = eCV5546_LOGO_BMP_WIDTH;
	info->logo_height = eCV5546_LOGO_BMP_HEIGHT;
	info->logo_x_offset = eCV5546_LOGO_BMP_X_OFFSET;
	info->logo_y_offset = eCV5546_LOGO_BMP_Y_OFFSET;
#endif
}
