/*
 * This is a driver for the ohci controller found in eYs3D Gemini SoC
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */
#include <clk.h>
#include <common.h>
#include <dm.h>
#include <usb.h>

#include "ohci.h"
#include <usb/sp_usb2.h>

struct sp_ohci {
	ohci_t ohci;
	struct clk ohci_clk;
};

#ifndef CONFIG_USB_EHCI_EYS3D
int clk_usbc0_cnt = 0;

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
#endif

static int ohci_eys3d_probe(struct udevice *dev)
{
	struct ohci_regs *regs = (struct ohci_regs *)devfdt_get_addr(dev);
	struct sp_ohci *priv = dev_get_priv(dev);
	int err;

	printf("%s.%d, dev_name:%s,port_num:%d\n",__FUNCTION__, __LINE__, dev->name, dev->seq_);

	err = clk_get_by_index(dev, 0, &priv->ohci_clk);
	if (err < 0) {
		pr_err("not found clk source\n");
		return err;
	}

	if (clk_usbc0_cnt == 0) {
		/* enable clock for USBC0 */
		clk_enable(&priv->ohci_clk);

		/* enable clock for UPHY0 */
		MOON2_REG->sft_cfg[6] = RF_MASK_V_SET(1 << 12);
	}

	clk_usbc0_cnt++;

#ifndef CONFIG_USB_EHCI_EYS3D
	usb_power_init(1, dev->seq_);
#endif

	return ohci_register(dev, regs);
}

static int ohci_eys3d_remove(struct udevice *dev)
{
	struct sp_ohci *priv = dev_get_priv(dev);
	int ret;

#ifndef CONFIG_USB_EHCI_EYS3D
	usb_power_init(0, dev->seq_);
#endif

	ret = ohci_deregister(dev);

	if (clk_usbc0_cnt == 1) {
		/* disable clock for UPHY0 */
		MOON2_REG->sft_cfg[6] = RF_MASK_V_CLR(1 << 12);

		/* disable clock for USBC0 */
		clk_disable_unprepare(&priv->ohci_clk);
	}

	clk_usbc0_cnt--;

	return ret;
}

static const struct udevice_id ohci_eys3d_ids[] = {
	{ .compatible = "eys3d,ecv5546-usb-ohci" },
	{ }
};

U_BOOT_DRIVER(ohci_eys3d) = {
	.name	 = "ohci_eys3d",
	.id	 = UCLASS_USB,
	.of_match = ohci_eys3d_ids,
	.probe = ohci_eys3d_probe,
	.remove = ohci_eys3d_remove,
	.ops	 = &ohci_usb_ops,
	.priv_auto = sizeof(struct sp_ohci),
	.flags  = DM_FLAG_ALLOC_PRIV_DMA,
};
