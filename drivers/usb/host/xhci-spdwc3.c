// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * DWC3 controller driver
 *
 * Author: ChingChouHuang<eys3d@eys3d.com>
 */

#include <common.h>
#include <dm.h>
#include <generic-phy.h>
#include <log.h>
#include <usb.h>
#include <dwc3-uboot.h>
#include <linux/delay.h>
#include <asm/gpio.h>
#include <usb/xhci.h>
#include <asm/io.h>
#include <linux/usb/dwc3.h>
#include <linux/usb/otg.h>

struct xhci_dwc3_plat {
	struct phy_bulk phys;
};

#define RF_GRP(_grp, _reg) ((((_grp) * 32 + (_reg)) * 4) + REG_BASE)
#define RF_AMBA(_grp, _reg) ((((_grp) * 1024 + (_reg)) * 4) + REG_BASE)
#define RF_MASK_V(_mask, _val)       (((_mask) << 16) | (_val))
#define RF_MASK_V_SET(_mask)         (((_mask) << 16) | (_mask))
#define RF_MASK_V_CLR(_mask)         (((_mask) << 16) | 0)
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
#define REG_BASE           0xF8000000
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
#define RF_GRP_AO(_grp, _reg)           ((((_grp) * 32 + (_reg)) * 4) + REG_BASE_AO)
#define REG_BASE_AO        0xF8800000

struct moon0_regs {
	unsigned int stamp;            // 0.0
	unsigned int reset[12];        // 0.1 -  0.12
	unsigned int rsvd[18];         // 0.13 - 0.30
	unsigned int hw_cfg;           // 0.31
};
#define MOON0_REG ((volatile struct moon0_regs *)RF_GRP_AO(0, 0))

struct moon2_regs {
	unsigned int rsvd1;            // 2.0
	unsigned int clken[12];        // 2.1 - 2.12
	unsigned int rsvd2[2];         // 2.13 - 2.14
	unsigned int gclken[12];       // 2.15 - 2.26
	unsigned int rsvd3[5];         // 2.27 - 2.31
};
#define MOON2_REG ((volatile struct moon2_regs *)RF_GRP_AO(2, 0))
#endif
/* start of xhci */
struct uphy_u3_regs {
	unsigned int cfg[32];		       // 189.0
};

struct gpio_desc *gpiodir;
struct udevice *phydev;

#define UPHY0_U3_REG ((volatile struct uphy_u3_regs *)RF_AMBA(189, 0))
#define UPHY0_ECO_REG ((volatile struct uphy_u3_regs *)RF_GRP(149, 0))
#endif

static void uphy_init(void)
{
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
#ifndef CONFIG_BOOT_ON_ZEBU
	volatile struct uphy_u3_regs *dwc3phy_reg;
	u32 result, dir, i = 0;

	UPHY0_ECO_REG->cfg[29] |= (0x1 << 30); // for usb3 vbus eco setting
#endif
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
	MOON2_REG->clken[5] = RF_MASK_V_SET(1 << 14); // U3PHY0_CLKEN=1

	MOON0_REG->reset[5] = RF_MASK_V_SET(1 << 14); // U3PHY0_RESET=1
	MOON0_REG->reset[5] = RF_MASK_V_SET(1 << 13); // USB30C0_RESET=1
	mdelay(1);
	MOON0_REG->reset[5] = RF_MASK_V_CLR(1 << 14); // U3PHY0_RESET=0
	MOON0_REG->reset[5] = RF_MASK_V_CLR(1 << 13); // USB30C0_RESET=0
#endif

#ifndef CONFIG_BOOT_ON_ZEBU
	dwc3phy_reg = (volatile struct uphy_u3_regs *) UPHY0_U3_REG;
	dwc3phy_reg->cfg[1] |= 0x03;
	for (;;)
	{
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
	dir = IS_ERR(gpiodir) ? 0 : dm_gpio_get_value(gpiodir);
	dwc3phy_reg->cfg[2] |= 0x01;
	if (dir)
		dwc3phy_reg->cfg[5] = (dwc3phy_reg->cfg[5] & 0xFFE0) | 0x15;
	else
		dwc3phy_reg->cfg[5] = (dwc3phy_reg->cfg[5] & 0xFFE0) | 0x11;

	for (;;)
	{
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
#endif
#endif
}

void dwc3_set_mode(struct dwc3 *dwc3_reg, u32 mode)
{
	clrsetbits_le32(&dwc3_reg->g_ctl,
			DWC3_GCTL_PRTCAPDIR(DWC3_GCTL_PRTCAP_OTG),
			DWC3_GCTL_PRTCAPDIR(mode));
}

static void dwc3_phy_reset(struct dwc3 *dwc3_reg)
{
	/* Assert USB3 PHY reset */
	setbits_le32(&dwc3_reg->g_usb3pipectl[0], DWC3_GUSB3PIPECTL_PHYSOFTRST);

	/* Assert USB2 PHY reset */
	setbits_le32(&dwc3_reg->g_usb2phycfg, DWC3_GUSB2PHYCFG_PHYSOFTRST);

	mdelay(100);

	/* Clear USB3 PHY reset */
	clrbits_le32(&dwc3_reg->g_usb3pipectl[0], DWC3_GUSB3PIPECTL_PHYSOFTRST);

	/* Clear USB2 PHY reset */
	clrbits_le32(&dwc3_reg->g_usb2phycfg, DWC3_GUSB2PHYCFG_PHYSOFTRST);
}

void dwc3_core_soft_reset(struct dwc3 *dwc3_reg)
{
	/* Before Resetting PHY, put Core in Reset */
	setbits_le32(&dwc3_reg->g_ctl, DWC3_GCTL_CORESOFTRESET);

	/* reset USB3 phy - if required */
	dwc3_phy_reset(dwc3_reg);

	mdelay(100);

	/* After PHYs are stable we can take Core out of reset state */
	clrbits_le32(&dwc3_reg->g_ctl, DWC3_GCTL_CORESOFTRESET);
}

int dwc3_core_init(struct dwc3 *dwc3_reg)
{
	u32 reg;
	u32 revision;
	unsigned int dwc3_hwparams1;

	revision = readl(&dwc3_reg->g_snpsid);
	/* This should read as U3 followed by revision number */
	if ((revision & DWC3_GSNPSID_MASK) != 0x55330000) {
		puts("this is not a DesignWare USB3 DRD Core\n");
		return -1;
	}

	dwc3_core_soft_reset(dwc3_reg);

	dwc3_hwparams1 = readl(&dwc3_reg->g_hwparams1);

	reg = readl(&dwc3_reg->g_ctl);
	reg &= ~DWC3_GCTL_SCALEDOWN_MASK;
	reg &= ~DWC3_GCTL_DISSCRAMBLE;
	switch (DWC3_GHWPARAMS1_EN_PWROPT(dwc3_hwparams1)) {
	case DWC3_GHWPARAMS1_EN_PWROPT_CLK:
		reg &= ~DWC3_GCTL_DSBLCLKGTNG;
		break;
	default:
		debug("No power optimization available\n");
	}

	/*
	 * WORKAROUND: DWC3 revisions <1.90a have a bug
	 * where the device can fail to connect at SuperSpeed
	 * and falls back to high-speed mode which causes
	 * the device to enter a Connect/Disconnect loop
	 */
	if ((revision & DWC3_REVISION_MASK) < 0x190a)
		reg |= DWC3_GCTL_U2RSTECN;

	writel(reg, &dwc3_reg->g_ctl);

	return 0;
}

#if CONFIG_IS_ENABLED(DM_USB)
static int xhci_dwc3_probe(struct udevice *dev)
{
	struct xhci_hcor *hcor;
	struct xhci_hccr *hccr;
	struct dwc3 *dwc3_reg;
	enum usb_dr_mode dr_mode;
	//struct xhci_dwc3_plat *plat = dev_get_plat(dev);
	const char *phy;
	u32 reg;
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
	ofnode node;
	int ret = 0;
#endif
	hccr = (struct xhci_hccr *)((uintptr_t)dev_remap_addr(dev));
	hcor = (struct xhci_hcor *)((uintptr_t)hccr + HC_LENGTH(xhci_readl(&(hccr)->cr_capbase)));
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
	node = ofnode_by_compatible(ofnode_null(), "eys3d,usb3-phy");
	if (!ofnode_valid(node))
		pr_err("%s phy node failed\n", __func__);

	ret = uclass_get_device_by_ofnode(UCLASS_PHY, node, &phydev);
	if (ret)
		pr_err("get phy device failed\n");
	gpiodir = devm_gpiod_get(phydev, "typec", GPIOD_IS_IN);
	if (IS_ERR(gpiodir))
		pr_info("[USB3] No typec gpio\n");
#endif
	uphy_init();

	dwc3_reg = (struct dwc3 *)((char *)(hccr) + DWC3_REG_OFFSET);

	dwc3_core_init(dwc3_reg);

	/* Set dwc3 usb2 phy config */
	reg = readl(&dwc3_reg->g_usb2phycfg[0]);

	phy = dev_read_string(dev, "phy_type");
	if (phy && strcmp(phy, "utmi_wide") == 0) {
		reg |= DWC3_GUSB2PHYCFG_PHYIF;
		reg &= ~DWC3_GUSB2PHYCFG_USBTRDTIM_MASK;
		reg |= DWC3_GUSB2PHYCFG_USBTRDTIM_16BIT;
	}

	if (dev_read_bool(dev, "snps,dis_enblslpm-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG_ENBLSLPM;

	if (dev_read_bool(dev, "snps,dis-u2-freeclk-exists-quirk"))
		reg &= ~DWC3_GUSB2PHYCFG_U2_FREECLK_EXISTS;

	if (dev_read_bool(dev, "snps,dis_u2_susphy_quirk"))
		reg &= ~DWC3_GUSB2PHYCFG_SUSPHY;

	writel(reg, &dwc3_reg->g_usb2phycfg[0]);

	dr_mode = usb_get_dr_mode(dev_ofnode(dev));
	if ((dr_mode == USB_DR_MODE_UNKNOWN) || (dr_mode == USB_DR_MODE_OTG))
		/* by default set dual role mode to HOST */
		dr_mode = USB_DR_MODE_HOST;

	dwc3_set_mode(dwc3_reg, dr_mode);

	return xhci_register(dev, hccr, hcor);
}

static int xhci_dwc3_remove(struct udevice *dev)
{
	//struct xhci_dwc3_plat *plat = dev_get_plat(dev);

	//dwc3_shutdown_phy(dev, &plat->phys);
#if defined(CONFIG_TARGET_PENTAGRAM_eCV5546)
	if (!IS_ERR(gpiodir))
		dm_gpio_free(phydev, gpiodir);
#endif
	return xhci_deregister(dev);
}

static const struct udevice_id xhci_dwc3_ids[] = {
	{ .compatible = "snps,dwc3" },
	{ .compatible = "synopsys,dwc3" },
	{ }
};

U_BOOT_DRIVER(xhci_dwc3) = {
	.name = "xhci-dwc3",
	.id = UCLASS_USB,
	.of_match = xhci_dwc3_ids,
	.probe = xhci_dwc3_probe,
	.remove = xhci_dwc3_remove,
	.ops = &xhci_usb_ops,
	.priv_auto	= sizeof(struct xhci_ctrl),
	.plat_auto	= sizeof(struct xhci_dwc3_plat),
	.flags = DM_FLAG_ALLOC_PRIV_DMA,
};
#endif
