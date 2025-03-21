// SPDX-License-Identifier: GPL-2.0+
/*
 *
 * DWC3 Phy driver
 *
 */

#include <dm.h>

static const struct udevice_id eys3d_dwc3phy_ids[] = {
	{ .compatible = "eys3d,usb3-phy" },
	{ }
};

U_BOOT_DRIVER(eys3d_dwc3_phy) = {
	.name	= "phy-eys3d-dwc3",
	.id	= UCLASS_PHY,
	.of_match = eys3d_dwc3phy_ids,
};
