// SPDX-License-Identifier: GPL-2.0
/*
 * common clock driver
 *
 * Author: qinjian <eys3d@eys3d.com>
 */

#ifndef __CLK_EYS3D_H__
#define __CLK_EYS3D_H__
#include <common.h>
#include <asm/io.h>
#include <clk-uclass.h>
#include <linux/clk-provider.h>
#include <div64.h>
#include <dm.h>
#include <dm/device-internal.h>

//#define SP_CLK_TEST

//#define TRACE	printf("!!! %s:%d\n", __FUNCTION__, __LINE__)
#define TRACE

#define CLK_PRINT(name, rate)	printf("%-16.16s:%11lu Hz\n", name, rate)

struct eys3d_clk {
	void *base;
};

int sp_clk_is_enabled(struct clk *clk);
void sp_clk_endisable(struct clk *clk, int enable);
void sp_clk_dump(struct clk *clk);
void sp_plls_dump(void);
ulong sp_clk_get_rate(struct clk *clk);
ulong sp_clk_set_rate(struct clk *clk, ulong rate);

int sp_clkc_init(void);

int eys3d_clk_get_by_index(int index, struct clk *clk);
int eys3d_clk_request(struct udevice *dev, struct clk *clk);

extern struct udevice *clkc_dev;

#endif /*__CLK_EYS3D_H__*/
