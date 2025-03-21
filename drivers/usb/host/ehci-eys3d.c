/*
 * This is a driver for the ehci controller found in eYs3D Gemini SoC
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */

#include <clk.h>
#include <common.h>
#include <dm.h>
#include <usb.h>
#include <asm/io.h>
#include <usb/ehci-ci.h>
#include <usb/ulpi.h>
#include <linux/delay.h>

#include "ehci.h"
#include <usb/sp_usb2.h>

struct eys3d_ehci_priv {
	struct ehci_ctrl ehcictrl;
	struct usb_ehci *ehci;
	struct clk ehci_clk;
	fdt_addr_t hcd_base;
};

int clk_usbc0_cnt = 0;

static void uphy_init(int port_num)
{
	unsigned int val, set;

	if (0 == port_num) {
		/* disable reset for OTP */
		MOON0_REG->reset[0] = RF_MASK_V_CLR(1 << 9);  // RBUS_BLOCKB_RESET=0
		mdelay(1);
		MOON0_REG->reset[4] = RF_MASK_V_CLR(1 << 13); // OTPRX_RESET=0
		mdelay(1);

		/* reset UPHY0 */
		/* UPHY0_RESET : 1->0 */
		MOON0_REG->reset[5] = RF_MASK_V_SET(1 << 12);
		mdelay(1);
		MOON0_REG->reset[5] = RF_MASK_V_CLR(1 << 12);
		mdelay(1);

		/* Default value modification */
		/* G149.28 uphy0_gctr0 */
		UPHY0_RN_REG->gctrl[0] = 0x08888102;

		/* PLL power off/on twice */
		/* G149.30 uphy0_gctrl2 */
		UPHY0_RN_REG->gctrl[2] = 0x88;
		mdelay(1);
		UPHY0_RN_REG->gctrl[2] = 0x80;
		mdelay(1);
		UPHY0_RN_REG->gctrl[2] = 0x88;
		mdelay(1);
		UPHY0_RN_REG->gctrl[2] = 0x80;
		mdelay(1);
		UPHY0_RN_REG->gctrl[2] = 0;
		mdelay(20); /*  experience */

		/* USBC 0 reset */
		/* USBC0_RESET : 1->0 */
		MOON0_REG->reset[5] = RF_MASK_V_SET(1 << 15);
		mdelay(1);
		MOON0_REG->reset[5] = RF_MASK_V_CLR(1 << 15);
		mdelay(1);

		/* fix rx-active question */
		/* G149.19 */
		UPHY0_RN_REG->cfg[19] |= 0xf;

		/* OTP for USB phy rx clock invert */
		/* G149.29[6] */
		val = OTP_REG->hb_otp_data[2];
		if (val & 0x1)
			UPHY0_RN_REG->gctrl[1] |= (1 << 6);

		/* OTP for USB phy tx clock invert */
		/* G149.29[5] */
		val = OTP_REG->hb_otp_data[2];
		if ((val >> 1) & 0x1)
			UPHY0_RN_REG->gctrl[1] |= (1 << 5);

		/* OTP for USB DISC (disconnect voltage) */
		/* G149.7[4:0] */
		val = (OTP_REG->reserved_8[11] >> 8);
	        set = val & 0x1f;
	        if (!set)
	                set = DEFAULT_UPHY_DISC;

	        UPHY0_RN_REG->cfg[7] = (UPHY0_RN_REG->cfg[7] & 0xffffffe0) | set;
	}
}

static void usb_power_init(int is_host, int port_num)
{
	/* a. enable pin mux control	*/
	/*    Host: enable		*/
	/*    Device: disable		*/
	if (is_host) {
		MOON1_REG->sft_cfg[1] = RF_MASK_V_SET(1 << 7);
	} else {
		MOON1_REG->sft_cfg[1] = RF_MASK_V_CLR(1 << 7);
	}

	/* b. USB control register: 			*/
	/*    Host:   ctrl=1, host sel=1, type=1 	*/
	/*    Device  ctrl=1, host sel=0, type=0 	*/
	if (is_host) {
		MOON4_REG->sft_cfg[10] = RF_MASK_V_SET(7 << 0);
	} else {
		MOON4_REG->sft_cfg[10] = RF_MASK_V_SET(1 << 0);
		MOON4_REG->sft_cfg[10] = RF_MASK_V_CLR(3 << 1);
	}
}

static int ehci_eys3d_ofdata_to_platdata(struct udevice *dev)
{
	struct eys3d_ehci_priv *priv = dev_get_priv(dev);

	priv->ehci = (struct usb_ehci *)devfdt_get_addr_ptr(dev);
	if (!priv->ehci)
		return -EINVAL;

	return 0;
}

static int ehci_eys3d_probe(struct udevice *dev)
{
	struct usb_plat *plat = dev_get_plat(dev);
	struct eys3d_ehci_priv *priv = dev_get_priv(dev);
	struct ehci_hccr *hccr;
	struct ehci_hcor *hcor;
	int err;

	err = clk_get_by_index(dev, 0, &priv->ehci_clk);
	if (err < 0) {
		pr_err("not found clk source\n");
		return err;
	}

	if (clk_usbc0_cnt == 0) {
		/* enable clock for USBC0 */
		clk_enable(&priv->ehci_clk);

		/* enable clock for UPHY0 */
		MOON2_REG->sft_cfg[6] = RF_MASK_V_SET(1 << 12);
	}

	clk_usbc0_cnt++;

	uphy_init(dev->seq_);
	usb_power_init(1, dev->seq_);

	priv->hcd_base = dev_read_addr_index(dev, 0);
	if (priv->hcd_base == FDT_ADDR_T_NONE)
		return -ENXIO;

	hccr = (struct ehci_hccr *)priv->hcd_base;
	hcor = (struct ehci_hcor *)((void *)hccr + HC_LENGTH(ehci_readl(&hccr->cr_capbase)));

	printf("%s.%d, dev_name:%s,port_num:%d\n",__FUNCTION__, __LINE__, dev->name, dev->seq_);

	return ehci_register(dev, hccr, hcor, NULL, 0, plat->init_type);
}

static int ehci_usb_remove(struct udevice *dev)
{
	struct eys3d_ehci_priv *priv = dev_get_priv(dev);
	struct ehci_hccr *hccr;
	struct ehci_hcor *hcor;
	int ret;

	usb_power_init(0, dev->seq_);
	ret = ehci_deregister(dev);

	/* route all ports to OHCI */
	hccr = (struct ehci_hccr *)priv->hcd_base;
	hcor = (struct ehci_hcor *)((void *)hccr + HC_LENGTH(ehci_readl(&hccr->cr_capbase)));

	ehci_writel(&hcor->or_configflag, 0x0);

	if (clk_usbc0_cnt == 1) {
		/* disable clock for UPHY0 */
		MOON2_REG->sft_cfg[6] = RF_MASK_V_CLR(1 << 12);

		/* disable clock for USBC0 */
		clk_disable_unprepare(&priv->ehci_clk);
	}

	clk_usbc0_cnt--;

	return ret;
}

static const struct udevice_id ehci_eys3d_ids[] = {
	{ .compatible = "eys3d,ecv5546-usb-ehci" },
	{ }
};

U_BOOT_DRIVER(ehci_eys3d) = {
	.name	= "ehci_eys3d",
	.id	= UCLASS_USB,
	.of_match = ehci_eys3d_ids,
	.of_to_plat = ehci_eys3d_ofdata_to_platdata,
	.probe = ehci_eys3d_probe,
	.remove = ehci_usb_remove,
	.ops	= &ehci_usb_ops,
	.plat_auto = sizeof(struct usb_plat),
	.priv_auto = sizeof(struct eys3d_ehci_priv),
	.flags	= DM_FLAG_ALLOC_PRIV_DMA,
};

