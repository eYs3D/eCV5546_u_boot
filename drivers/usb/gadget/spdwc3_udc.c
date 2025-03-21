#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/usb/composite.h>
#include <asm/unaligned.h>
#include <linux/usb/otg.h>
#include <linux/usb/gadget.h>
#include <linux/usb/ch9.h>
#include <malloc.h>
#include <common.h>
#include <linux/io.h>
#include <dm/devres.h>
#include <dm.h>
#include <dwc3-uboot.h>

#include "../dwc3/core.h"

#define DRIVER_NAME "spdwc3-udc"

#define eCV5546_REG_BASE			(0xf8000000)
#define eCV5546_RF_GRP(_grp, _reg)	((((_grp)*32+(_reg))*4)+eCV5546_REG_BASE)
#define eCV5546_RF_AMBA(_grp, _reg) 	((((_grp)*1024+(_reg))*4)+eCV5546_REG_BASE)
#define eCV5546_REG_BASE_AO		(0xf8800000)
#define eCV5546_RF_GRP_AO(_grp, _reg)	((((_grp)*32+(_reg))*4)+eCV5546_REG_BASE_AO)
#define eCV5546_RF_MASK_V_SET(_mask)       (((_mask)<<16) | (_mask))
#define eCV5546_RF_MASK_V_CLR(_mask)	(((_mask)<<16)| 0)

struct eCV5546_moon0_regs_ao {
	unsigned int stamp;            // 0.0
	unsigned int reset[12];        // 0.1 -  0.12
	unsigned int rsvd[18];         // 0.13 - 0.30
	unsigned int hw_cfg;           // 0.31
};
#define eCV5546_MOON0_REG_AO ((volatile struct eCV5546_moon0_regs_ao *)eCV5546_RF_GRP_AO(0,0))

struct eCV5546_moon1_regs_ao{
	unsigned int sft_cfg[32];
};
#define eCV5546_MOON1_REG_AO ((volatile struct eCV5546_moon1_regs_ao *)eCV5546_RF_GRP_AO(1,0))

struct eCV5546_moon2_regs_ao {
	unsigned int rsvd1;		// 2.0
	unsigned int clken[12];		// 2.1 - 2.12
	unsigned int rsvd2[2];		// 2.13 - 2.14
	unsigned int gclken[12];	// 2.15 - 2.26
	unsigned int rsvd3[5];		// 2.27 - 2.31
};
#define eCV5546_MOON2_REG_AO ((volatile struct eCV5546_moon2_regs_ao *)eCV5546_RF_GRP_AO(2, 0))

struct eCV5546_moon4_regs_ao{
	unsigned int sft_cfg[32];
};
#define eCV5546_MOON4_REG_AO ((volatile struct eCV5546_moon4_regs_ao *)eCV5546_RF_GRP_AO(4,0))

struct uphy_u3_regs {
	unsigned int cfg[32];		// 189.0
};

#define UPHY0_U3_REG ((volatile struct uphy_u3_regs *)eCV5546_RF_AMBA(189, 0))
#define UPHY0_ECO_REG ((volatile struct uphy_u3_regs *)eCV5546_RF_GRP(149, 0))

static struct dwc3_device dwc3_device_data = {
	.maximum_speed = USB_SPEED_SUPER,
	.dr_mode = USB_DR_MODE_PERIPHERAL,
	.base = 0xf80a1000,
	.index = 0,
	.dis_u2_susphy_quirk = 1,
	.hsphy_mode = USBPHY_INTERFACE_MODE_UTMIW,
};

void uphy_init(void)
{
	volatile struct uphy_u3_regs *dwc3phy_reg;
	u32 result, i = 0;

	UPHY0_ECO_REG->cfg[29] &= ~(0x1 << 30); // for usb3 vbus eco setting

	eCV5546_MOON2_REG_AO->clken[5] = eCV5546_RF_MASK_V_SET(1 << 14);
	eCV5546_MOON2_REG_AO->clken[5] = eCV5546_RF_MASK_V_SET(1 << 13);

	eCV5546_MOON0_REG_AO->reset[5] = eCV5546_RF_MASK_V_SET(1 << 14);
	eCV5546_MOON0_REG_AO->reset[5] = eCV5546_RF_MASK_V_SET(1 << 13);
	mdelay(1);
	eCV5546_MOON0_REG_AO->reset[5] = eCV5546_RF_MASK_V_CLR(1 << 14);
	eCV5546_MOON0_REG_AO->reset[5] = eCV5546_RF_MASK_V_CLR(1 << 13);

	dwc3phy_reg = (volatile struct uphy_u3_regs *) UPHY0_U3_REG;
	dwc3phy_reg->cfg[1] |= 0x03;

	for (;;) {
		result = dwc3phy_reg->cfg[2] & 0x3;
		if (result == 0x01)
			break;

		if (i++ > 10) {
			debug("PHY0_TIMEOUT_ERR0 ");
			i = 0;
			break;
		}
		mdelay(1);
	}

	dwc3phy_reg->cfg[2] |= 0x01;
	//if (dm_gpio_get_value(gpiodir))
	//	dwc3phy_reg->cfg[5] = (dwc3phy_reg->cfg[5] & 0xFFE0) | 0x15;
	//else
		dwc3phy_reg->cfg[5] = (dwc3phy_reg->cfg[5] & 0xFFE0) | 0x11;

	for (;;) {
		result = dwc3phy_reg->cfg[2] & 0x3;
		if (result == 0x01)
			break;

		if (i++ > 10) {
			debug("PHY0_TIMEOUT_ERR1 ");
			i = 0;
			break;
		}

		mdelay(1);
	}
}

static int spdwc3_udc_probe(struct udevice *udev)
{
	uphy_init();

	return dwc3_uboot_init(&dwc3_device_data);
}

static int spdwc3_udc_remove(struct udevice *udev)
{
	dwc3_uboot_exit(dwc3_device_data.index);

	return 0;
}

static const struct udevice_id spdwc3_udc_ids[] = {
	{ .compatible = "eys3d,dwc3" },
	{ },
};

U_BOOT_DRIVER(spdwc3_udc) = {
	.name = "spdwc3-udc",
	.id = UCLASS_USB_GADGET_GENERIC,
	.of_match = spdwc3_udc_ids,
	.probe = spdwc3_udc_probe,
	.remove = spdwc3_udc_remove,
	.priv_auto = sizeof(struct dwc3),
};

