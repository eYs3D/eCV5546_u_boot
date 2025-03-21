/*
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <version.h>
#include <common.h>
#include <asm/global_data.h>
#include <net.h>
#include <dm.h>

#ifdef CONFIG_SP_SPINAND
extern void board_spinand_init(void);
#endif

#ifdef CONFIG_SP_PARANAND
extern void board_paranand_init(void);
#endif

DECLARE_GLOBAL_DATA_PTR;

int board_init(void)
{
	return 0;
}

int board_eth_init(struct bd_info *bis)
{
	return 0;
}

int misc_init_r(void)
{
	return 0;
}

void board_nand_init(void)
{
#ifdef CONFIG_SP_SPINAND
	board_spinand_init();
#endif
#ifdef CONFIG_SP_PARANAND
	board_paranand_init();
#endif
}

__weak int ecv5546_video_show_board_info(void);

#ifdef CONFIG_BOARD_LATE_INIT
int board_late_init(void)
{
#ifdef CONFIG_VIDEO_eCV5546
	ecv5546_video_show_board_info();
#endif

#ifdef CONFIG_USB_ETHER
	usb_ether_init();
#endif

	return 0;
}
#endif

#if defined(CONFIG_DM_USB_GADGET)
#include <clk.h>
#include <dwc3-uboot.h>
#include <sp_udc.h>

int dm_usb_gadget_handle_interrupts(struct udevice *dev)
{
	#if defined(CONFIG_USB_GADGET_EYS3D_UDC)
	if (!strncmp(dev->name, "usb", 3)) {
		struct sp_udc *udc = dev_get_priv(dev);

		return sp_udc_irq(udc);
	}
	#endif

	#if defined(CONFIG_USB_DWC3_GADGET)
	if (!strncmp(dev->name, "spdwc3", 6)) {
		dwc3_uboot_handle_interrupt(0);

		return 1;
	}
	#endif

	return 0;
}
#endif