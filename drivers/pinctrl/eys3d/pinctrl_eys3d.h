/* SPDX-License-Identifier: GPL-2.0 */

#ifndef __PINCTRL_EYS3D_H__
#define __PINCTRL_EYS3D_H__

#include <common.h>

#define DISABLE_CONFLICT_CODE_WITH_GENERIC_USAGE

#define pctl_err(fmt, arg...) printf(fmt, ##arg)
#if 0
#define pctl_info(fmt, arg...) printf(fmt, ##arg)
#else
#define pctl_info(fmt, arg...)
#endif

#define GPIO_PINGRP(x) moon1_regs[x]

#define GPIO_MASTER(x) gpioxt_regs[x]
#define GPIO_OE(x) gpioxt_regs[(x) + 13]
#define GPIO_OUT(x) gpioxt_regs[(x) + 26]
#define GPIO_IN(x) gpioxt_regs[(x) + 32 + 7]
#define GPIO_I_INV(x) gpioxt_regs[(x) + 32 + 15]
#define GPIO_O_INV(x) gpioxt_regs[(x) + 32 + 28]
#define GPIO_OD(x) gpioxt_regs[(x) + 64 + 9]
#define GPIO_FIRST(x) first_regs[x]

#define MAX_PINS 108
#define D(x, y) ((x)*8 + (y))

typedef enum {
	F_OFF_0, // nowhere
	F_OFF_M, // in mux registers
	F_OFF_G, // mux group registers
	F_OFF_I, // in iop registers
} F_OFF_t;

#define EGRP(n, v, p)                                                          \
	{                                                                      \
		.name = n, .gval = (v), .pins = (p), .pnum = ARRAY_SIZE(p)     \
	}

#define FNCE(n, r, o, bo, bl, g)                                               \
	{                                                                      \
		.name = n, .freg = r, .roff = o, .boff = bo, .blen = bl,       \
		.grps = (g), .gnum = ARRAY_SIZE(g)                             \
	}

#define FNCN(n, r, o, bo, bl)                                                  \
	{                                                                      \
		.name = n, .freg = r, .roff = o, .boff = bo, .blen = bl,       \
		.grps = NULL, .gnum = 0,                                       \
	}

struct sppctlgrp_t {
	const char *const name;
	const u8 gval; // value for register
	const unsigned *const pins; // list of pins
	const unsigned int pnum; // number of pins
};

struct func_t {
	const char *const name;
	const F_OFF_t freg; // function register type
	const u8 roff; // register offset
	const u8 boff; // bit offset
	const u8 blen; // number of bits
	const u8 gnum; // number of groups
	const struct sppctlgrp_t *const grps; // list of groups
	const char *grps_sa[12]; // array of pointers to func's grps names
};

struct grp2fp_map_t {
	u16 f_idx; // function index
	u16 g_idx; // pins/group index inside function
};

extern struct func_t list_funcs[];
extern const size_t list_func_nums;

extern u32 *moon1_regs;
extern u32 *padctl1_regs;
extern u32 *first_regs;
extern u32 *padctl2_regs;
extern u32 *gpioxt_regs;

int sp_gpio_pin_mux_set(u32 func, u32 pin);
int sp_gpio_pin_mux_get(u32 func, u32 *pin);
int sp_gpio_input_invert_set(u32 offset, u32 value);
int sp_gpio_input_invert_get(u32 offset, u32 *value);
int sp_gpio_output_invert_set(u32 offset, u32 value);
int sp_gpio_output_invert_get(u32 offset, u32 *value);
int sp_gpio_open_drain_set(u32 offset, u32 value);
int sp_gpio_open_drain_get(u32 offset, u32 *value);
int sp_gpio_first_set(u32 offset, u32 value);
int sp_gpio_first_get(u32 offset, u32 *value);
int sp_gpio_master_set(u32 offset, u32 value);
int sp_gpio_master_get(u32 offset, u32 *value);
int sp_gpio_oe_set(u32 offset, u32 value);
int sp_gpio_oe_get(u32 offset, u32 *value);
int sp_gpio_out_set(u32 offset, u32 value);
int sp_gpio_out_get(u32 offset, u32 *value);
int sp_gpio_in(u32 offset, u32 *value);
u32 sp_gpio_para_get(u32 offset);

#endif
