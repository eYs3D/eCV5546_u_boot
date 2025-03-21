// SPDX-License-Identifier: GPL-2.0

#include <linux/compat.h>
#include <dm/devres.h>
#include "pinctrl_eys3d.h"
#include "ecv5546_func_group.h"
#include "ecv5546_pins.h"

static const unsigned int pins_spif[] = { 21, 22, 23, 24, 25, 26 };
static const struct sppctlgrp_t ecv5546grps_spif[] = {
	EGRP("SPI_FLASH", 1, pins_spif),
};

static const unsigned int pins_emmc[] = { 20, 28, 29, 30, 31, 32,
					  33, 34, 35, 36, 37 };
static const struct sppctlgrp_t ecv5546grps_emmc[] = {
	EGRP("EMMC", 1, pins_emmc),
};

static const unsigned int pins_spi_nand_x1[] = { 30, 31, 32, 33, 34, 35 };
static const unsigned int pins_spi_nand_x2[] = { 21, 22, 23, 24, 25, 26 };
static const struct sppctlgrp_t ecv5546grps_spi_nand[] = {
	EGRP("SPI_NAND_X1", 1, pins_spi_nand_x1),
	EGRP("SPI_NAND_X2", 2, pins_spi_nand_x2),
};

static const unsigned int pins_sdc30[] = { 38, 39, 40, 41, 42, 43 };
static const struct sppctlgrp_t ecv5546grps_sdc30[] = {
	EGRP("SD_CARD", 1, pins_sdc30),
};

static const unsigned int pins_sdio30[] = { 44, 45, 46, 47, 48, 49 };
static const struct sppctlgrp_t ecv5546grps_sdio30[] = {
	EGRP("SDIO", 1, pins_sdio30),
};

static const unsigned int pins_para_nand[] = { 20, 21, 22, 23, 24, 25,
					       26, 27, 28, 29, 30, 31,
					       32, 33, 34, 35, 36 };
static const struct sppctlgrp_t ecv5546grps_para_nand[] = {
	EGRP("PARA_NAND", 1, pins_para_nand),
};

static const unsigned int pins_usb_otg[] = { 18, 19 };
static const struct sppctlgrp_t ecv5546grps_usb_otg[] = {
	EGRP("USB_OTG", 1, pins_usb_otg),
};

static const unsigned int pins_gmac_rgmii[] = { 3,  4,	5,  6,	7,  8,	9,
						10, 11, 12, 13, 14, 15, 16 };
static const unsigned int pins_gmac_rmii[] = { 4, 5, 6, 7, 8, 9, 10, 11, 12 };

static const struct sppctlgrp_t ecv5546grps_gmac[] = {
	EGRP("GMAC_RGMII", 1, pins_gmac_rgmii),
	EGRP("GMAC_RMII", 2, pins_gmac_rmii),
};

static const unsigned int pins_pwm0_x1[] = { 78 };
static const unsigned int pins_pwm0_x2[] = { 58 };
static const struct sppctlgrp_t ecv5546grps_pwm0[] = {
	EGRP("PWM0_X1", 1, pins_pwm0_x1),
	EGRP("PWM0_X2", 2, pins_pwm0_x2),
};

static const unsigned int pins_pwm1_x1[] = { 79 };
static const unsigned int pins_pwm1_x2[] = { 59 };
static const struct sppctlgrp_t ecv5546grps_pwm1[] = {
	EGRP("PWM1_X1", 1, pins_pwm1_x1),
	EGRP("PWM1_X2", 2, pins_pwm1_x2),
};

static const unsigned int pins_pwm2_x1[] = { 60 };
static const unsigned int pins_pwm2_x2[] = { 92 };
static const struct sppctlgrp_t ecv5546grps_pwm2[] = {
	EGRP("PWM2_X1", 1, pins_pwm2_x1),
	EGRP("PWM2_X2", 2, pins_pwm2_x2),
};

static const unsigned int pins_pwm3_x1[] = { 61 };
static const unsigned int pins_pwm3_x2[] = { 93 };
static const struct sppctlgrp_t ecv5546grps_pwm3[] = {
	EGRP("PWM3_X1", 1, pins_pwm3_x1),
	EGRP("PWM3_X2", 2, pins_pwm3_x2),
};

static const unsigned int pins_uart0_x1[] = { 50, 51 };
static const unsigned int pins_uart0_x2[] = { 68, 69 };
static const struct sppctlgrp_t ecv5546grps_uart0[] = {
	EGRP("UART0_X1", 1, pins_uart0_x1),
	EGRP("UART0_X2", 2, pins_uart0_x2),
};

static const unsigned int pins_uart1_x1[] = { 52, 53 };
static const unsigned int pins_uart1_x2[] = { 64, 65 };
static const struct sppctlgrp_t ecv5546grps_uart1[] = {
	EGRP("UART1_X1", 1, pins_uart1_x1),
	EGRP("UART1_X2", 2, pins_uart1_x2),
};

static const unsigned int pins_uart1_fc_x1[] = { 54, 55 };
static const unsigned int pins_uart1_fc_x2[] = { 66, 67 };
static const struct sppctlgrp_t ecv5546grps_uart1_fc[] = {
	EGRP("UART1_FC_X1", 1, pins_uart1_fc_x1),
	EGRP("UART1_FC_X2", 2, pins_uart1_fc_x2),
};

static const unsigned int pins_uart2_x1[] = { 56, 57 };
static const unsigned int pins_uart2_x2[] = { 76, 77 };
static const struct sppctlgrp_t ecv5546grps_uart2[] = {
	EGRP("UART2_X1", 1, pins_uart2_x1),
	EGRP("UART2_X2", 2, pins_uart2_x2),
};

static const unsigned int pins_uart2_fc_x1[] = { 58, 59 };
static const unsigned int pins_uart2_fc_x2[] = { 78, 79 };
static const struct sppctlgrp_t ecv5546grps_uart2_fc[] = {
	EGRP("UART2_FC_X1", 1, pins_uart2_fc_x1),
	EGRP("UART2_FC_X2", 2, pins_uart2_fc_x2),
};

static const unsigned int pins_uart3_x1[] = { 62, 63 };
static const unsigned int pins_uart3_x2[] = { 7, 8 };
static const struct sppctlgrp_t ecv5546grps_uart3[] = {
	EGRP("UART3_X1", 1, pins_uart3_x1),
	EGRP("UART3_X2", 2, pins_uart3_x2),
};

static const unsigned int pins_uadbg[] = { 13, 14 };
static const struct sppctlgrp_t ecv5546grps_uadbg[] = {
	EGRP("UADBG", 1, pins_uadbg),
};

static const unsigned int pins_uart6_x1[] = { 80, 81 };
static const unsigned int pins_uart6_x2[] = { 48, 49 };
static const struct sppctlgrp_t ecv5546grps_uart6[] = {
	EGRP("UART6_X1", 1, pins_uart6_x1),
	EGRP("UART6_X2", 2, pins_uart6_x2),
};

static const unsigned int pins_uart7[] = { 82, 83 };
static const struct sppctlgrp_t ecv5546grps_uart7[] = {
	EGRP("UART7", 1, pins_uart7),
};

static const unsigned int pins_i2c_combo0_x1[] = { 68, 69 };
static const unsigned int pins_i2c_combo0_x2[] = { 54, 55 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo0[] = {
	EGRP("I2C_COMBO0_X1", 1, pins_i2c_combo0_x1),
	EGRP("I2C_COMBO0_X2", 2, pins_i2c_combo0_x2),
};

static const unsigned int pins_i2c_combo1[] = { 70, 71 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo1[] = {
	EGRP("I2C_COMBO1", 1, pins_i2c_combo1),
};

static const unsigned int pins_i2c_combo2_x1[] = { 76, 77 };
static const unsigned int pins_i2c_combo2_x2[] = { 56, 57 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo2[] = {
	EGRP("I2C_COMBO2_X1", 1, pins_i2c_combo2_x1),
	EGRP("I2C_COMBO2_X2", 2, pins_i2c_combo2_x2),
};

static const unsigned int pins_i2c_combo3[] = { 88, 89 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo3[] = {
	EGRP("I2C_COMBO3", 1, pins_i2c_combo3),
};

static const unsigned int pins_i2c_combo4[] = { 90, 91 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo4[] = {
	EGRP("I2C_COMBO4", 1, pins_i2c_combo4),
};

static const unsigned int pins_i2c_combo5[] = { 92, 93 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo5[] = {
	EGRP("I2C_COMBO5", 1, pins_i2c_combo5),
};

static const unsigned int pins_i2c_combo6_x1[] = { 84, 85 };
static const unsigned int pins_i2c_combo6_x2[] = { 1, 2 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo6[] = {
	EGRP("I2C_COMBO6_X1", 1, pins_i2c_combo6_x1),
	EGRP("I2C_COMBO6_X2", 2, pins_i2c_combo6_x2),
};

static const unsigned int pins_i2c_combo7_x1[] = { 86, 87 };
static const unsigned int pins_i2c_combo7_x2[] = { 3, 4 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo7[] = {
	EGRP("I2C_COMBO7_X1", 1, pins_i2c_combo7_x1),
	EGRP("I2C_COMBO7_X2", 2, pins_i2c_combo7_x2),
};

static const unsigned int pins_i2c_combo8_x1[] = { 95, 96 };
static const unsigned int pins_i2c_combo8_x2[] = { 9, 10 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo8[] = {
	EGRP("I2C_COMBO8_X1", 1, pins_i2c_combo8_x1),
	EGRP("I2C_COMBO8_X2", 2, pins_i2c_combo8_x2),
};

static const unsigned int pins_i2c_combo9_x1[] = { 97, 98 };
static const unsigned int pins_i2c_combo9_x2[] = { 11, 12 };
static const struct sppctlgrp_t ecv5546grps_i2c_combo9[] = {
	EGRP("I2C_COMBO9_X1", 1, pins_i2c_combo9_x1),
	EGRP("I2C_COMBO9_X2", 2, pins_i2c_combo9_x2),
};

static const unsigned int pins_spi_master0_x1[] = { 64, 65, 66, 67 };
static const unsigned int pins_spi_master0_x2[] = { 9, 10, 11, 12 };
static const struct sppctlgrp_t ecv5546grps_spi_master0[] = {
	EGRP("SPI_MASTER0_X1", 1, pins_spi_master0_x1),
	EGRP("SPI_MASTER0_X2", 2, pins_spi_master0_x2),
};

static const unsigned int pins_spi_master1_x1[] = { 80, 81, 82, 83 };
static const unsigned int pins_spi_master1_x2[] = { 14, 15, 16, 17 };
static const struct sppctlgrp_t ecv5546grps_spi_master1[] = {
	EGRP("SPI_MASTER1_X1", 1, pins_spi_master1_x1),
	EGRP("SPI_MASTER1_X2", 2, pins_spi_master1_x2),
};

static const unsigned int pins_spi_master2[] = { 88, 89, 90, 91 };
static const struct sppctlgrp_t ecv5546grps_spi_master2[] = {
	EGRP("SPI_MASTER2", 1, pins_spi_master2),
};

static const unsigned int pins_spi_master3_x1[] = { 44, 45, 46, 47 };
static const unsigned int pins_spi_master3_x2[] = { 52, 53, 54, 55 };
static const struct sppctlgrp_t ecv5546grps_spi_master3[] = {
	EGRP("SPI_MASTER3_X1", 1, pins_spi_master3_x1),
	EGRP("SPI_MASTER3_X2", 2, pins_spi_master3_x2),
};

static const unsigned int pins_spi_master4[] = { 72, 73, 74, 75 };
static const struct sppctlgrp_t ecv5546grps_spi_master4[] = {
	EGRP("SPI_MASTER4", 1, pins_spi_master4),
};

static const unsigned int pins_spi_slave0_x1[] = { 94, 95, 96, 97 };
static const unsigned int pins_spi_slave0_x2[] = { 72, 73, 74, 75 };
static const struct sppctlgrp_t ecv5546grps_spi_slave0[] = {
	EGRP("SPI_SLAVE0_X1", 1, pins_spi_slave0_x1),
	EGRP("SPI_SLAVE0_X2", 2, pins_spi_slave0_x2),
};

static const unsigned int pins_aud_tdmtx_xck[] = { 93 };
static const struct sppctlgrp_t ecv5546grps_aud_tdmtx_xck[] = {
	EGRP("AUD_TDMTX_XCK", 1, pins_aud_tdmtx_xck),
};

static const unsigned int pins_aud_dac_xck1[] = { 71 };
static const struct sppctlgrp_t ecv5546grps_aud_dac_xck1[] = {
	EGRP("AUD_DAC_XCK1", 1, pins_aud_dac_xck1),
};

static const unsigned int pins_aud_dac_xck[] = { 83 };
static const struct sppctlgrp_t ecv5546grps_aud_dac_xck[] = {
	EGRP("AUD_DAC_XCK", 1, pins_aud_dac_xck),
};

static const unsigned int pins_aud_au2_data0[] = { 82 };
static const struct sppctlgrp_t ecv5546grps_aud_au2_data0[] = {
	EGRP("AUD_AU2_DATA0", 1, pins_aud_au2_data0),
};

static const unsigned int pins_aud_au1_data0[] = { 58 };
static const struct sppctlgrp_t ecv5546grps_aud_au1_data0[] = {
	EGRP("AUD_AU1_DATA0", 1, pins_aud_au1_data0),
};

static const unsigned int pins_aud_au2_ck[] = { 80, 81 };
static const struct sppctlgrp_t ecv5546grps_aud_au2_ck[] = {
	EGRP("AUD_AU2_CK", 1, pins_aud_au2_ck),
};

static const unsigned int pins_aud_au1_ck[] = { 56, 57 };
static const struct sppctlgrp_t ecv5546grps_aud_au1_ck[] = {
	EGRP("AUD_AU1_CK", 1, pins_aud_au1_ck),
};

static const unsigned int pins_aud_au_adc_data0_x1[] = { 94, 95, 96, 97 };
static const unsigned int pins_aud_au_adc_data0_x2[] = { 72, 73, 74, 75 };
static const struct sppctlgrp_t ecv5546grps_aud_au_adc_data0[] = {
	EGRP("AUD_AU_ADC_DATA0_X1", 1, pins_aud_au_adc_data0_x1),
	EGRP("AUD_AU_ADC_DATA0_X2", 2, pins_aud_au_adc_data0_x2),
};

static const unsigned int pins_aud_adc2_data0[] = { 82 };
static const struct sppctlgrp_t ecv5546grps_aud_adc2_data0[] = {
	EGRP("AUD_ADC2_DATA0", 1, pins_aud_adc2_data0),
};

static const unsigned int pins_aud_adc1_data0[] = { 58 };
static const struct sppctlgrp_t ecv5546grps_aud_adc1_data0[] = {
	EGRP("AUD_ADC1_DATA0", 1, pins_aud_adc1_data0),
};

static const unsigned int pins_aud_tdm[] = { 94, 95, 96, 97 };
static const struct sppctlgrp_t ecv5546grps_aud_tdm[] = {
	EGRP("AUD_TDM", 1, pins_aud_tdm),
};

static const unsigned int pins_spdif_x1[] = { 91 };
static const unsigned int pins_spdif_x2[] = { 53 };
static const unsigned int pins_spdif_x3[] = { 54 };
static const unsigned int pins_spdif_x4[] = { 55 };
static const unsigned int pins_spdif_x5[] = { 62 };
static const unsigned int pins_spdif_x6[] = { 52 };
static const struct sppctlgrp_t ecv5546grps_spdif_in[] = {
	EGRP("SPDIF_IN_X1", 1, pins_spdif_x1),
	EGRP("SPDIF_IN_X2", 2, pins_spdif_x2),
	EGRP("SPDIF_IN_X3", 3, pins_spdif_x3),
	EGRP("SPDIF_IN_X4", 4, pins_spdif_x4),
	EGRP("SPDIF_IN_X5", 5, pins_spdif_x5),
	EGRP("SPDIF_IN_X6", 6, pins_spdif_x6),
};

static const struct sppctlgrp_t ecv5546grps_spdif_out[] = {
	EGRP("SPDIF_OUT_X1", 1, pins_spdif_x1),
	EGRP("SPDIF_OUT_X2", 2, pins_spdif_x2),
	EGRP("SPDIF_OUT_X3", 3, pins_spdif_x3),
	EGRP("SPDIF_OUT_X4", 4, pins_spdif_x4),
	EGRP("SPDIF_OUT_X5", 5, pins_spdif_x5),
	EGRP("SPDIF_OUT_X6", 6, pins_spdif_x6),
};

static const unsigned int pins_int0_x1[] = { 1 };
static const unsigned int pins_int0_x2[] = { 2 };
static const unsigned int pins_int0_x3[] = { 3 };
static const unsigned int pins_int0_x4[] = { 4 };
static const unsigned int pins_int0_x5[] = { 5 };
static const unsigned int pins_int0_x6[] = { 6 };
static const unsigned int pins_int0_x7[] = { 13 };
static const unsigned int pins_int0_x8[] = { 14 };
static const unsigned int pins_int0_x9[] = { 15 };
static const struct sppctlgrp_t ecv5546grps_int0[] = {
	EGRP("INT0_X1", 1, pins_int0_x1), EGRP("INT0_X2", 2, pins_int0_x2),
	EGRP("INT0_X3", 3, pins_int0_x3), EGRP("INT0_X4", 4, pins_int0_x4),
	EGRP("INT0_X5", 5, pins_int0_x5), EGRP("INT0_X6", 6, pins_int0_x6),
	EGRP("INT0_X7", 7, pins_int0_x7), EGRP("INT0_X8", 8, pins_int0_x8),
	EGRP("INT0_X9", 9, pins_int0_x9),
};

static const struct sppctlgrp_t ecv5546grps_int1[] = {
	EGRP("INT1_X1", 1, pins_int0_x1), EGRP("INT1_X2", 2, pins_int0_x2),
	EGRP("INT1_X3", 3, pins_int0_x3), EGRP("INT1_X4", 4, pins_int0_x4),
	EGRP("INT1_X5", 5, pins_int0_x5), EGRP("INT1_X6", 6, pins_int0_x6),
	EGRP("INT1_X7", 7, pins_int0_x7), EGRP("INT1_X8", 8, pins_int0_x8),
	EGRP("INT1_X9", 9, pins_int0_x9),
};

static const unsigned int pins_int2_x1[] = { 5 };
static const unsigned int pins_int2_x2[] = { 6 };
static const unsigned int pins_int2_x3[] = { 7 };
static const unsigned int pins_int2_x4[] = { 8 };
static const unsigned int pins_int2_x5[] = { 9 };
static const unsigned int pins_int2_x6[] = { 10 };
static const unsigned int pins_int2_x7[] = { 11 };
static const unsigned int pins_int2_x8[] = { 16 };
static const unsigned int pins_int2_x9[] = { 17 };
static const struct sppctlgrp_t ecv5546grps_int2[] = {
	EGRP("INT2_X1", 1, pins_int2_x1), EGRP("INT2_X2", 2, pins_int2_x2),
	EGRP("INT2_X3", 3, pins_int2_x3), EGRP("INT2_X4", 4, pins_int2_x4),
	EGRP("INT2_X5", 5, pins_int2_x5), EGRP("INT2_X6", 6, pins_int2_x6),
	EGRP("INT2_X7", 7, pins_int2_x7), EGRP("INT2_X8", 8, pins_int2_x8),
	EGRP("INT2_X9", 9, pins_int2_x9),
};

static const struct sppctlgrp_t ecv5546grps_int3[] = {
	EGRP("INT3_X1", 1, pins_int2_x1), EGRP("INT3_X2", 2, pins_int2_x2),
	EGRP("INT3_X3", 3, pins_int2_x3), EGRP("INT3_X4", 4, pins_int2_x4),
	EGRP("INT3_X5", 5, pins_int2_x5), EGRP("INT3_X6", 6, pins_int2_x6),
	EGRP("INT3_X7", 7, pins_int2_x7), EGRP("INT3_X8", 8, pins_int2_x8),
	EGRP("INT3_X9", 9, pins_int2_x9),
};

static const unsigned int pins_int4_x1[] = { 7 };
static const unsigned int pins_int4_x2[] = { 8 };
static const unsigned int pins_int4_x3[] = { 9 };
static const unsigned int pins_int4_x4[] = { 10 };
static const unsigned int pins_int4_x5[] = { 11 };
static const unsigned int pins_int4_x6[] = { 12 };
static const unsigned int pins_int4_x7[] = { 13 };
static const unsigned int pins_int4_x8[] = { 18 };
static const unsigned int pins_int4_x9[] = { 19 };
static const struct sppctlgrp_t ecv5546grps_int4[] = {
	EGRP("INT4_X1", 1, pins_int4_x1), EGRP("INT4_X2", 2, pins_int4_x2),
	EGRP("INT4_X3", 3, pins_int4_x3), EGRP("INT4_X4", 4, pins_int4_x4),
	EGRP("INT4_X5", 5, pins_int4_x5), EGRP("INT4_X6", 6, pins_int4_x6),
	EGRP("INT4_X7", 7, pins_int4_x7), EGRP("INT4_X8", 8, pins_int4_x8),
	EGRP("INT4_X9", 9, pins_int4_x9),
};

static const struct sppctlgrp_t ecv5546grps_int5[] = {
	EGRP("INT5_X1", 1, pins_int4_x1), EGRP("INT5_X2", 2, pins_int4_x2),
	EGRP("INT5_X3", 3, pins_int4_x3), EGRP("INT5_X4", 4, pins_int4_x4),
	EGRP("INT5_X5", 5, pins_int4_x5), EGRP("INT5_X6", 6, pins_int4_x6),
	EGRP("INT5_X7", 7, pins_int4_x7), EGRP("INT5_X8", 8, pins_int4_x8),
	EGRP("INT5_X9", 9, pins_int4_x9),
};

static const unsigned int pins_int6_x1[] = { 9 };
static const unsigned int pins_int6_x2[] = { 10 };
static const unsigned int pins_int6_x3[] = { 11 };
static const unsigned int pins_int6_x4[] = { 12 };
static const unsigned int pins_int6_x5[] = { 13 };
static const unsigned int pins_int6_x6[] = { 14 };
static const unsigned int pins_int6_x7[] = { 15 };
static const unsigned int pins_int6_x8[] = { 16 };
static const unsigned int pins_int6_x9[] = { 17 };
static const unsigned int pins_int6_x10[] = { 18 };
static const unsigned int pins_int6_x11[] = { 19 };
static const struct sppctlgrp_t ecv5546grps_int6[] = {
	EGRP("INT6_X1", 1, pins_int6_x1),
	EGRP("INT6_X2", 2, pins_int6_x2),
	EGRP("INT6_X3", 3, pins_int6_x3),
	EGRP("INT6_X4", 4, pins_int6_x4),
	EGRP("INT6_X5", 5, pins_int6_x5),
	EGRP("INT6_X6", 6, pins_int6_x6),
	EGRP("INT6_X7", 7, pins_int6_x7),
	EGRP("INT6_X8", 8, pins_int6_x8),
	EGRP("INT6_X9", 9, pins_int6_x9),
	EGRP("INT6_X10", 10, pins_int6_x10),
	EGRP("INT6_X11", 11, pins_int6_x11),
};

static const struct sppctlgrp_t ecv5546grps_int7[] = {
	EGRP("INT7_X1", 1, pins_int6_x1),
	EGRP("INT7_X2", 2, pins_int6_x2),
	EGRP("INT7_X3", 3, pins_int6_x3),
	EGRP("INT7_X4", 4, pins_int6_x4),
	EGRP("INT7_X5", 5, pins_int6_x5),
	EGRP("INT7_X6", 6, pins_int6_x6),
	EGRP("INT7_X7", 7, pins_int6_x7),
	EGRP("INT7_X8", 8, pins_int6_x8),
	EGRP("INT7_X9", 9, pins_int6_x9),
	EGRP("INT7_X10", 10, pins_int6_x10),
	EGRP("INT7_X11", 11, pins_int6_x11),
};

static const unsigned int pins_gpio_ao_int0_x1[] = { 52, 53, 54, 55,
						     56, 57, 58, 59 };
static const unsigned int pins_gpio_ao_int0_x2[] = { 68, 69, 70, 71,
						     72, 73, 74, 75 };
static const struct sppctlgrp_t ecv5546grps_gpio_ao_int0[] = {
	EGRP("GPIO_AO_INT0_X1", 1, pins_gpio_ao_int0_x1),
	EGRP("GPIO_AO_INT0_X2", 1, pins_gpio_ao_int0_x2),
};

static const unsigned int pins_gpio_ao_int1_x1[] = { 60, 61, 62, 63,
						     64, 65, 66, 67 };
static const unsigned int pins_gpio_ao_int1_x2[] = { 76, 77, 78, 79,
						     80, 81, 82, 83 };
static const struct sppctlgrp_t ecv5546grps_gpio_ao_int1[] = {
	EGRP("GPIO_AO_INT1_X1", 1, pins_gpio_ao_int1_x1),
	EGRP("GPIO_AO_INT1_X2", 1, pins_gpio_ao_int1_x2),
};

static const unsigned int pins_gpio_ao_int2_x1[] = { 68, 69, 70, 71,
						     72, 73, 74, 75 };
static const unsigned int pins_gpio_ao_int2_x2[] = { 84, 85, 86, 87,
						     88, 89, 90, 91 };
static const struct sppctlgrp_t ecv5546grps_gpio_ao_int2[] = {
	EGRP("GPIO_AO_INT2_X1", 1, pins_gpio_ao_int2_x1),
	EGRP("GPIO_AO_INT2_X2", 1, pins_gpio_ao_int2_x2),
};

static const unsigned int pins_gpio_ao_int3_x1[] = { 76, 77, 78, 79,
						     80, 81, 82, 83 };
static const unsigned int pins_gpio_ao_int3_x2[] = { 91, 92, 93, 94,
						     95, 96, 97, 98 };
static const struct sppctlgrp_t ecv5546grps_gpio_ao_int3[] = {
	EGRP("GPIO_AO_INT3_X1", 1, pins_gpio_ao_int3_x1),
	EGRP("GPIO_AO_INT3_X2", 1, pins_gpio_ao_int3_x2),
};

/////////////////////////////////////////////////////////////////////////////

struct func_t list_funcs[] = {
	FNCN("GPIO", F_OFF_0, 0, 0, 0),
	FNCN("IOP", F_OFF_0, 0, 0, 0),
	FNCE("SPI_FLASH", F_OFF_G, 1, 0, 1, ecv5546grps_spif),
	FNCE("EMMC", F_OFF_G, 1, 1, 1, ecv5546grps_emmc),
	FNCE("SPI_NAND", F_OFF_G, 1, 2, 2, ecv5546grps_spi_nand),
	FNCE("SD_CARD", F_OFF_G, 1, 4, 1, ecv5546grps_sdc30),
	FNCE("SDIO", F_OFF_G, 1, 5, 1, ecv5546grps_sdio30),
	FNCE("PARA_NAND", F_OFF_G, 1, 6, 1, ecv5546grps_para_nand),
	FNCE("USB_OTG", F_OFF_G, 1, 7, 1, ecv5546grps_usb_otg),
	FNCE("GMAC", F_OFF_G, 1, 9, 1, ecv5546grps_gmac),
	FNCE("PWM0", F_OFF_G, 1, 10, 2, ecv5546grps_pwm0),
	FNCE("PWM1", F_OFF_G, 1, 12, 2, ecv5546grps_pwm1),
	FNCE("PWM2", F_OFF_G, 1, 14, 2, ecv5546grps_pwm2),

	FNCE("PWM3", F_OFF_G, 2, 0, 2, ecv5546grps_pwm3),
	FNCE("UART0", F_OFF_G, 2, 2, 2, ecv5546grps_uart0),
	FNCE("UART1", F_OFF_G, 2, 4, 2, ecv5546grps_uart1),
	FNCE("UART1_FC", F_OFF_G, 2, 6, 2, ecv5546grps_uart1_fc),
	FNCE("UART2", F_OFF_G, 2, 8, 2, ecv5546grps_uart2),
	FNCE("UART2_FC", F_OFF_G, 2, 10, 2, ecv5546grps_uart2_fc),
	FNCE("UART3", F_OFF_G, 2, 12, 2, ecv5546grps_uart3),
	FNCE("UADBG", F_OFF_G, 2, 14, 1, ecv5546grps_uadbg),

	FNCE("UART6", F_OFF_G, 3, 0, 2, ecv5546grps_uart6),
	FNCE("UART7", F_OFF_G, 3, 2, 1, ecv5546grps_uart7),
	FNCE("I2C_COMBO0", F_OFF_G, 3, 3, 2, ecv5546grps_i2c_combo0),
	FNCE("I2C_COMBO1", F_OFF_G, 3, 5, 1, ecv5546grps_i2c_combo1),
	FNCE("I2C_COMBO2", F_OFF_G, 3, 6, 2, ecv5546grps_i2c_combo2),
	FNCE("I2C_COMBO3", F_OFF_G, 3, 8, 1, ecv5546grps_i2c_combo3),
	FNCE("I2C_COMBO4", F_OFF_G, 3, 9, 1, ecv5546grps_i2c_combo4),
	FNCE("I2C_COMBO5", F_OFF_G, 3, 10, 1, ecv5546grps_i2c_combo5),
	FNCE("I2C_COMBO6", F_OFF_G, 3, 11, 2, ecv5546grps_i2c_combo6),
	FNCE("I2C_COMBO7", F_OFF_G, 3, 13, 2, ecv5546grps_i2c_combo7),

	FNCE("I2C_COMBO8", F_OFF_G, 4, 0, 2, ecv5546grps_i2c_combo8),
	FNCE("I2C_COMBO9", F_OFF_G, 4, 2, 2, ecv5546grps_i2c_combo9),
	FNCE("SPI_MASTER0", F_OFF_G, 4, 14, 2, ecv5546grps_spi_master0),

	FNCE("SPI_MASTER1", F_OFF_G, 5, 1, 2, ecv5546grps_spi_master1),
	FNCE("SPI_MASTER2", F_OFF_G, 5, 3, 1, ecv5546grps_spi_master2),
	FNCE("SPI_MASTER3", F_OFF_G, 5, 4, 2, ecv5546grps_spi_master3),
	FNCE("SPI_MASTER4", F_OFF_G, 5, 6, 1, ecv5546grps_spi_master4),
	FNCE("SPI_SLAVE0", F_OFF_G, 5, 7, 2, ecv5546grps_spi_slave0),
	FNCE("AUD_TDMTX_XCK", F_OFF_G, 5, 9, 1, ecv5546grps_aud_tdmtx_xck),
	FNCE("AUD_DAC_XCK1", F_OFF_G, 5, 10, 1, ecv5546grps_aud_dac_xck1),
	FNCE("AUD_DAC_XCK", F_OFF_G, 5, 11, 1, ecv5546grps_aud_dac_xck),
	FNCE("AUD_AU2_DATA0", F_OFF_G, 5, 12, 1, ecv5546grps_aud_au2_data0),
	FNCE("AUD_AU1_DATA0", F_OFF_G, 5, 13, 1, ecv5546grps_aud_au1_data0),
	FNCE("AUD_AU2_CK", F_OFF_G, 5, 14, 1, ecv5546grps_aud_au2_ck),
	FNCE("AUD_AU1_CK", F_OFF_G, 5, 15, 1, ecv5546grps_aud_au1_ck),

	FNCE("AUD_AU_ADC_DATA0", F_OFF_G, 6, 0, 2, ecv5546grps_aud_au_adc_data0),
	FNCE("AUD_ADC2_DATA0", F_OFF_G, 6, 2, 1, ecv5546grps_aud_adc2_data0),
	FNCE("AUD_ADC1_DATA0", F_OFF_G, 6, 3, 1, ecv5546grps_aud_adc1_data0),
	FNCE("AUD_TDM", F_OFF_G, 6, 4, 1, ecv5546grps_aud_tdm),
	FNCE("SPDIF_IN", F_OFF_G, 6, 5, 3, ecv5546grps_spdif_in),
	FNCE("SPDIF_OUT", F_OFF_G, 6, 8, 3, ecv5546grps_spdif_out),

	FNCE("INT0", F_OFF_G, 7, 5, 4, ecv5546grps_int0),
	FNCE("INT1", F_OFF_G, 7, 9, 4, ecv5546grps_int1),

	FNCE("INT2", F_OFF_G, 8, 0, 4, ecv5546grps_int2),
	FNCE("INT3", F_OFF_G, 8, 4, 4, ecv5546grps_int3),
	FNCE("INT4", F_OFF_G, 8, 8, 4, ecv5546grps_int4),
	FNCE("INT5", F_OFF_G, 8, 12, 4, ecv5546grps_int5),

	FNCE("INT6", F_OFF_G, 9, 0, 4, ecv5546grps_int6),
	FNCE("INT7", F_OFF_G, 9, 4, 4, ecv5546grps_int7),
	FNCE("GPIO_AO_INT0", F_OFF_G, 9, 8, 2, ecv5546grps_gpio_ao_int0),
	FNCE("GPIO_AO_INT1", F_OFF_G, 9, 10, 2, ecv5546grps_gpio_ao_int1),
	FNCE("GPIO_AO_INT2", F_OFF_G, 9, 12, 2, ecv5546grps_gpio_ao_int2),
	FNCE("GPIO_AO_INT3", F_OFF_G, 9, 14, 2, ecv5546grps_gpio_ao_int3),
};

const size_t list_func_nums = ARRAY_SIZE(list_funcs);

static size_t grps_count;

static char const **group_name_list;
static struct grp2fp_map_t *g2fp_maps;

const char *eys3d_get_function_name_by_selector(unsigned int selector)
{
	return list_funcs[selector].name;
}

struct func_t *eys3d_get_function_by_selector(unsigned int selector)
{
	return &list_funcs[selector];
}

int eys3d_get_function_count(void)
{
	return list_func_nums;
}

const char *eys3d_get_group_name_by_selector(unsigned int selector)
{
	if (!group_name_list)
		return NULL;

	return group_name_list[selector];
}

struct grp2fp_map_t *eys3d_get_group_by_name(const char *name)
{
	int i;

	for (i = 0; i < grps_count; i++) {
		if (!strcmp(group_name_list[i], name))
			return &g2fp_maps[i];
	}

	return NULL;
}

struct grp2fp_map_t *eys3d_get_group_by_selector(unsigned int group_selector,
						   unsigned int func_selector)
{
	int i, j;
	struct func_t *func;

	if (group_selector > eys3d_get_pins_count() - 1)
		return &g2fp_maps[group_selector];

	/* function:GPIO */
	if (func_selector == 0)
		return &g2fp_maps[group_selector];

	/* group:GPIO0 ~GPIO105 */
	func = eys3d_get_function_by_selector(func_selector);

	for (i = 0; i < func->gnum; i++) {
		for (j = 0; j < func->grps[i].pnum; j++) {
			if (group_selector == func->grps[i].pins[j]) {
				return eys3d_get_group_by_name(
					func->grps[i].name);
			}
		}
	}

	return NULL;
}

int eys3d_get_groups_count(void)
{
	return grps_count;
}

void group_groups(struct udevice *dev)
{
	int i, k, j;

	group_name_list = NULL;
	grps_count = 0;
	size_t pins_count = eys3d_get_pins_count();

	// fill array of all groups

	grps_count = pins_count;

	// calc unique group names array size
	for (i = 0; i < list_func_nums; i++) {
		if (list_funcs[i].freg != F_OFF_G)
			continue;
		grps_count += list_funcs[i].gnum;
	}

	// fill up unique group names array
	group_name_list = devm_kzalloc(dev, (grps_count + 1) * sizeof(char *),
				       GFP_KERNEL);
	g2fp_maps = devm_kzalloc(dev,
				 (grps_count + 1) * sizeof(struct grp2fp_map_t),
				 GFP_KERNEL);

	// groups == pins
	j = 0;

	for (i = 0; i < pins_count; i++) {
		group_name_list[i] = eys3d_get_pin_name_by_selector(i);
		g2fp_maps[i].f_idx = 0;
		g2fp_maps[i].g_idx = i;
	}
	j = pins_count;

	for (i = 0; i < list_func_nums; i++) {
		if (list_funcs[i].freg != F_OFF_G)
			continue;

		for (k = 0; k < list_funcs[i].gnum; k++) {
			list_funcs[i].grps_sa[k] =
				(char *)list_funcs[i].grps[k].name;
			group_name_list[j] = list_funcs[i].grps[k].name;
			g2fp_maps[j].f_idx = i;
			g2fp_maps[j].g_idx = k;
			j++;
		}
	}
}

int eys3d_pin_function_association_query(unsigned int pin_selector,
					   unsigned int func_selector)
{
	int i, j;
	struct func_t *func = eys3d_get_function_by_selector(func_selector);

	for (i = 0; i < func->gnum; i++) {
		for (j = 0; j < func->grps[i].pnum; j++) {
			if (pin_selector == func->grps[i].pins[j])
				return 0;
		}
	}

	return -EEXIST;
}

int eys3d_group_function_association_query(unsigned int group_selector,
					     unsigned int func_selector)
{
	struct grp2fp_map_t *group_map =
		eys3d_get_group_by_selector(group_selector, func_selector);

	if (group_map->f_idx == func_selector)
		return 0;

	return -EEXIST;
}
