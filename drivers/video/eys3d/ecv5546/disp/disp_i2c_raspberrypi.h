// SPDX-License-Identifier: GPL-2.0+
/*
 * Author(s): Hammer Hsieh <eys3d@eys3d.com>
 *
 * This raspberrypi 7" touchscreen panel driver is inspired 
 * from the Linux Kernel driver
 * drivers/gpu/drm/panel/panel-raspberrypi-touchscreen.c
 */

#ifndef __DISP_I2C_RASPBERRYPI_H__
#define __DISP_I2C_RASPBERRYPI_H__

#include <i2c.h>
//#include <linux/delay.h>

#define ATMEL_MCU_I2C_ADDR 0x45

/* I2C registers of the Atmel microcontroller. */
enum REG_ADDR {
	REG_ID = 0x80,
	REG_PORTA, /* BIT(2) for horizontal flip, BIT(3) for vertical flip */
	REG_PORTB,
	REG_PORTC,
	REG_PORTD,
	REG_POWERON,
	REG_PWM,
	REG_DDRA,
	REG_DDRB,
	REG_DDRC,
	REG_DDRD,
	REG_TEST,
	REG_WR_ADDRL,
	REG_WR_ADDRH,
	REG_READH,
	REG_READL,
	REG_WRITEH,
	REG_WRITEL,
	REG_ID2,
};

int raspberrypi_check_exist(struct udevice *p1)
{
	int ver;

	ver = dm_i2c_reg_read(p1, REG_ID); /* Atmel I2C read version */

	if (ver < 0) {
		printf("Atmel I2C read failed: %d\n", ver);
		return ver;
	}

	switch (ver) {
	case 0xde: /* ver 1 */
	case 0xc3: /* ver 2 */
		break;
	default:
		printf("Unknown Atmel firmware revision: 0x%02x\n", ver);
		return ver;
	}
	//printf("Atmel I2C read ok: %02x\n", ver);
	return 0;
}

void raspberrypi_pre_init(struct udevice *p1)
{
	int i;

	dm_i2c_reg_write(p1, REG_POWERON, 0x01); /* power on */

	/* Wait for nPWRDWN to go low to indicate poweron is done. */
	for (i = 0; i < 100; i++) {
		if (dm_i2c_reg_read(p1, REG_PORTB) & 1)
			break;
	}

}
void raspberrypi_post_init(struct udevice *p1)
{
	/* Turn on the backlight. */
	dm_i2c_reg_write(p1, REG_PWM, 255);

	/* BIT(2) for horizontal flip, BIT(3) for vertical flip */
	/*
	 * add delay for reserving response time between dm_i2c_reg_write()
	 */
	udelay(15);
	dm_i2c_reg_write(p1, REG_PORTA, BIT(2));
	//dm_i2c_reg_write(p1, REG_PORTA, BIT(3));

}

#endif	//__DISP_I2C_RASPBERRYPI_H__
