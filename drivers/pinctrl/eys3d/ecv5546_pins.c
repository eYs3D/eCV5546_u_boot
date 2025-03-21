// SPDX-License-Identifier: GPL-2.0

#include <linux/stringify.h>
#include <linux/kernel.h>
#include "ecv5546_pins.h"

/**
 * struct pinctrl_pin_desc - boards/machines provide information on their
 * pins, pads or other muxable units in this struct
 * @number: unique pin number from the global pin number space
 * @name: a name for this pin
 * @drv_data: driver-defined per-pin data. pinctrl core does not touch this
 */
struct pinctrl_pin_desc {
	unsigned int number;
	const char *name;
	void *drv_data;
};

#define D_PIS(x) "GPIO" __stringify(x)
#define PINCTRL_PIN(a, b)                                                      \
	{                                                                      \
		.number = a, .name = b                                         \
	}
#define P(x) PINCTRL_PIN(x, D_PIS(x))

static const struct pinctrl_pin_desc list_pins[] = {
	P(0),  P(1),   P(2),   P(3),   P(4),   P(5),   P(6),   P(7),  P(8),
	P(9),  P(10),  P(11),  P(12),  P(13),  P(14),  P(15),  P(16), P(17),
	P(18), P(19),  P(20),  P(21),  P(22),  P(23),  P(24),  P(25), P(26),
	P(27), P(28),  P(29),  P(30),  P(31),  P(32),  P(33),  P(34), P(35),
	P(36), P(37),  P(38),  P(39),  P(40),  P(41),  P(42),  P(43), P(44),
	P(45), P(46),  P(47),  P(48),  P(49),  P(50),  P(51),  P(52), P(53),
	P(54), P(55),  P(56),  P(57),  P(58),  P(59),  P(60),  P(61), P(62),
	P(63), P(64),  P(65),  P(66),  P(67),  P(68),  P(69),  P(70), P(71),
	P(72), P(73),  P(74),  P(75),  P(76),  P(77),  P(78),  P(79), P(80),
	P(81), P(82),  P(83),  P(84),  P(85),  P(86),  P(87),  P(88), P(89),
	P(90), P(91),  P(92),  P(93),  P(94),  P(95),  P(96),  P(97), P(98),
	P(99), P(100), P(101), P(102), P(103), P(104), P(105),
};

static const size_t list_pins_nums = ARRAY_SIZE(list_pins);

const char *eys3d_get_pin_name_by_selector(unsigned int selector)
{
	if (selector >= list_pins_nums)
		return NULL;

	return list_pins[selector].name;
}

size_t eys3d_get_pins_count(void)
{
	return list_pins_nums;
}
