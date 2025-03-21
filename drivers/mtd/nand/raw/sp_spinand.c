#include <common.h>
#include <malloc.h>
#include <asm/io.h>
#include <nand.h>

#include <linux/mtd/rawnand.h>
#include <linux/mtd/nand.h>
#include <dm.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <mach/gpio_drv.h>
#include <cpu_func.h>
#include <command.h>
#include <clk.h>

#include "sp_bch.h"
#include "sp_spinand.h"


/**************************************************************************
 *                             M A C R O S                                *
 **************************************************************************/

DECLARE_GLOBAL_DATA_PTR;
/**************************************************************************
 *                         D A T A   T Y P E S                            *
 **************************************************************************/

/**************************************************************************
 *                        G L O B A L   D A T A                           *
 **************************************************************************/
static struct sp_spinand_info *our_spinfc = NULL;

/**************************************************************************
 *                 E X T E R N A L   R E F E R E N C E S                  *
 **************************************************************************/
extern const struct nand_flash_dev sp_spinand_ids[];

/**************************************************************************/
/*static void printf_block(uint8_t *data, uint32_t size)
{
	int i;
	for(i=0; i<size; i++) {
		if( i%16 == 0)
			printf("\n\t");
		printf("0x%02x,", data[i]);
	}
	printf("\n");
}*/

void dump_spi_regs(struct sp_spinand_info *info)
{
	u32 *p = (u32 *)info->regs;
	int i;
	const char *reg_name[] = {
		"spi_ctrl",
		"spi_timing",
		"spi_page_addr",
		"spi_data",
		"spi_status",
		"spi_auto_cfg",
		"spi_cfg0",
		"spi_cfg1",
		"spi_cfg2",
		"spi_data_64",
		"spi_buf_addr",
		"spi_statu_2",
		"spi_err_status",
		"mem_data_addr",
		"mem_parity_addr",
		"spi_col_addr",
		"spi_bch",
		"spi_intr_msk",
		"spi_intr_sts",
		"spi_page_size",
		"device_parity_addr",
	};

	for (i = 0; i < sizeof(reg_name) / sizeof(reg_name[0]); i++, p++) {
		SPINAND_LOGI("%s = 0x%08X\n", reg_name[i], readl(p));
	}
}

struct sp_spinand_info* get_spinand_info(void)
{
	return our_spinfc;
}

static int get_iomode_cfg(u32 io_mode, u32 write)
{
	int cfg = -1;
	if (io_mode == SPINAND_1BIT_MODE) {
		cfg = SPINAND_CMD_BITMODE(1)
			| SPINAND_CMD_DQ(1)
			| SPINAND_ADDR_BITMODE(1)
			| SPINAND_ADDR_DQ(1)
			| SPINAND_DATA_BITMODE(1)
			| SPINAND_DATAOUT_DQ(1)
			| SPINAND_DATAIN_DQ(2);
	} else if (io_mode == SPINAND_2BIT_MODE) {
		cfg = SPINAND_CMD_BITMODE(1)
			| SPINAND_CMD_DQ(1)
			| SPINAND_ADDR_BITMODE(1)
			| SPINAND_ADDR_DQ(1)
			| SPINAND_DATA_BITMODE(2);
	} else if (io_mode == SPINAND_4BIT_MODE) {
		cfg = SPINAND_CMD_BITMODE(1)
			| SPINAND_CMD_DQ(1)
			| SPINAND_ADDR_BITMODE(1)
			| SPINAND_ADDR_DQ(write? 2:1)		// 1->2. Solve D1 delay issue for write
			| SPINAND_DATA_BITMODE(3);
	} else if (io_mode == SPINAND_DUAL_MODE) {
		cfg = SPINAND_CMD_BITMODE(1)
			| SPINAND_ADDR_BITMODE(2)
			| SPINAND_DATA_BITMODE(2);
	} else if (io_mode == SPINAND_QUAD_MODE) {
		cfg = SPINAND_CMD_BITMODE(1)
			| SPINAND_ADDR_BITMODE(3)
			| SPINAND_DATA_BITMODE(3);
	}

	return cfg;
}

static int get_iomode_readcmd(u32 io_mode)
{
	int cmd = -1;
	if (io_mode == SPINAND_1BIT_MODE) {
		cmd = SPINAND_CMD_PAGEREAD;
	} else if (io_mode == SPINAND_2BIT_MODE) {
		cmd = SPINAND_CMD_PAGEREAD_X2;
	} else if (io_mode == SPINAND_4BIT_MODE) {
		cmd = SPINAND_CMD_PAGEREAD_X4;
	} else if (io_mode == SPINAND_DUAL_MODE) {
		cmd = SPINAND_CMD_PAGEREAD_DUAL;
	} else if (io_mode == SPINAND_QUAD_MODE) {
		cmd = SPINAND_CMD_PAGEREAD_QUAD;
	}
	return cmd;
}

static int get_iomode_writecmd(u32 io_mode)
{
	int cmd = -1;
	if (io_mode == SPINAND_1BIT_MODE) {
		cmd = SPINAND_CMD_PROGLOAD;
	} else if (io_mode == SPINAND_4BIT_MODE) {
		cmd = SPINAND_CMD_PROGLOAD_X4;
	}
	return cmd;
}

int wait_spi_idle(struct sp_spinand_info *info)
{
	struct sp_spinand_regs *regs = info->regs;
	unsigned long now = get_timer(0);
	int ret = -1;

	do {
		if (!(readl(&regs->spi_ctrl) & SPINAND_BUSY_MASK)) {
			ret = 0;
			break;
		}
	} while(get_timer(now) < CONFIG_SPINAND_TIMEOUT);

	if (ret < 0) {
		SPINAND_LOGE("%s timeout \n", __FUNCTION__);
		//dump_spi_regs(info);
	}

	return ret;
}

int spi_nand_trigger_and_wait_dma(struct sp_spinand_info *info)
{
	struct sp_spinand_regs *regs = info->regs;
	unsigned long timeout_ms = CONFIG_SPINAND_TIMEOUT;
	unsigned long now;
	u32 value;
	int ret = -1;

	value = ~SPINAND_DMA_DONE_MASK;
	writel(value, &regs->spi_intr_msk);

	value = readl(&regs->spi_intr_sts);
	writel(value, &regs->spi_intr_sts);

	value = readl(&regs->spi_auto_cfg);
	value |= SPINAND_DMA_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	now = get_timer(0);
	do {
		if ((readl(&regs->spi_intr_sts) & SPINAND_DMA_DONE_MASK)) {
			ret = 0;
			break;
		}
	} while(get_timer(now) < timeout_ms);

	if(ret < 0) {
		SPINAND_LOGE("%s timeout\n", __FUNCTION__);
		//dump_spi_regs(info);
	}

	return ret;
}

int spi_nand_trigger_and_wait_pio(struct sp_spinand_info *info)
{
	struct sp_spinand_regs *regs = info->regs;
	unsigned long timeout_ms = CONFIG_SPINAND_TIMEOUT;
	unsigned long now;
	u32 value;
	int ret = -1;

	value = ~SPINAND_PIO_DONE_MASK;
	writel(value, &regs->spi_intr_msk);

	value = readl(&regs->spi_intr_sts);
	writel(value, &regs->spi_intr_sts);

	value = readl(&regs->spi_auto_cfg);
	value |= SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	now = get_timer(0);
	do {
		if ((readl(&regs->spi_intr_sts) & SPINAND_PIO_DONE_MASK)) {
			ret = 0;
			break;
		}
	} while(get_timer(now) < timeout_ms);

	if(ret < 0) {
		SPINAND_LOGE("%s timeout\n", __FUNCTION__);
		//dump_spi_regs(info);
	}

	return ret;
}

static int spi_nand_getfeatures(struct sp_spinand_info *info, u32 addr)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value = 0;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_GETFEATURES)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(1)
		| SPINAND_READ_MODE
		| SPINAND_USRCMD_ADDRSZ(1);
	writel(value, &regs->spi_ctrl);

	writel(addr, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1)
		| SPINAND_DATA_BITMODE(1)
		| SPINAND_DATAIN_DQ(2);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	wait_spi_idle(info);

	return (readl(&regs->spi_data) & 0xFF);
}

static int spi_nand_setfeatures(struct sp_spinand_info *info, u32 addr,u32 data)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value = 0;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_AUTOWEL_EN
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_SETFEATURES)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(1)
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(1);
	writel(value, &regs->spi_ctrl);

	writel(addr, &regs->spi_page_addr);

	writel(data, &regs->spi_data);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1)
		| SPINAND_DATA_BITMODE(1)
		| SPINAND_DATAOUT_DQ(1);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	return wait_spi_idle(info);
}

static int spi_nand_reset(struct sp_spinand_info *info)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value = 0;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(7)
		| SPINAND_USR_CMD(SPINAND_CMD_RESET)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(0)
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(0);
	writel(value, &regs->spi_ctrl);

	value = SPINAND_CS_DISACTIVE_CYC(CONFIG_SPINAND_CS_DISACTIVE_CYC)
		| SPINAND_READ_TIMING(CONFIG_SPINAND_READ_TIMING_SEL);
	writel(value ,&regs->spi_timing);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(0)
		| SPINAND_ADDR_DQ(0)
		| SPINAND_DATA_BITMODE(0)
		| SPINAND_DATAIN_DQ(0)
		| SPINAND_DATAOUT_DQ(0);
	writel(value, &regs->spi_cfg[1]);

	writel(0, &regs->spi_bch);

	value = SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	wait_spi_idle(info);

	value = SPINAND_CHECK_OIP_EN
		| SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	return wait_spi_idle(info);
}

static void spi_nand_readid(struct sp_spinand_info *info, u32 addr, u8 *data)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value = 0;

	/*read 3 byte cycle */
	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_READID)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(3)
		| SPINAND_READ_MODE
		| SPINAND_USRCMD_ADDRSZ(1);
	writel(value, &regs->spi_ctrl);

	writel(addr, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1)
		| SPINAND_DATA_BITMODE(1)
		| SPINAND_DATAIN_DQ(2)
		| SPINAND_DATAOUT_DQ(0);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	wait_spi_idle(info);

	value = readl(&regs->spi_data);

	*(u32 *)data = value;
}

static int spi_nand_select_die(struct sp_spinand_info *info, u32 id)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_DIE_SELECT)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(0)
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(1);
	writel(value, &regs->spi_ctrl);

	writel(id, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	return wait_spi_idle(info);
}

static int spi_nand_wait_dev_idle(struct sp_spinand_info *info)
{
	u32 timeout = CONFIG_SPINAND_TIMEOUT;
	u32 time_start = get_timer(0);
	u32 status;
	do {
		status = spi_nand_getfeatures(info, DEVICE_STATUS_ADDR);
		if ((status & DEVICE_STATUS_OIP_MSK) == 0)
			break;
	} while (get_timer(time_start) < timeout);
	return status;
}

static int spi_nand_blkerase(struct sp_spinand_info *info, u32 row)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value = 0;
	int ret;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_AUTOWEL_EN
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_BLKERASE)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(0)
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(3);
	writel(value, &regs->spi_ctrl);

	writel(row, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1)
		| SPINAND_DATA_BITMODE(1)
		| SPINAND_DATAIN_DQ(2)
		| SPINAND_DATAOUT_DQ(1);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_AUTO_RDSR_EN;
	writel(value, &regs->spi_auto_cfg);

	ret = spi_nand_trigger_and_wait_pio(info);
	if (!ret) {
		value = readl(&regs->spi_status);
		ret = (value & ERASE_STATUS) ? (-1) : 0;
	}

	return ret;
}

static int spi_nand_program_exec(struct sp_spinand_info *info, u32 row)
{
	struct sp_spinand_regs *regs = info->regs;
	int ret = 0;
	u32 value = 0;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_PROGEXEC)
		| SPINAND_CTRL_EN
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(3);
	writel(value, &regs->spi_ctrl);

	writel(row, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1)
		| SPINAND_DATA_BITMODE(1)
		| SPINAND_DATAIN_DQ(2)
		| SPINAND_DATAOUT_DQ(1);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_USR_PRGMLOAD_CMD(SPINAND_CMD_PROGEXEC)
		| SPINAND_USR_PRGMLOAD_EN
		| SPINAND_AUTO_RDSR_EN;
	writel(value, &regs->spi_auto_cfg);

	ret = spi_nand_trigger_and_wait_pio(info);
	if (!ret) {
		value = readl(&regs->spi_status);
		ret = ((value&0x08) ? -1 : 0);
	}

	return ret;
}

static int spi_nand_program_load(struct sp_spinand_info *info, u32 io_mode,
				u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	int cfg = get_iomode_cfg(io_mode, 1);
	int cmd = get_iomode_writecmd(io_mode);
	u32 value = 0;
	u32 i;

	if (cfg < 0 || cmd < 0)
		return -1;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_CTRL_EN
		| SPINAND_USR_CMD(cmd)
		| SPINAND_AUTOWEL_EN
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_DATA64_EN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(size);
	writel(value, &regs->spi_cfg[0]);

	writel(cfg, &regs->spi_cfg[1]);

	value = col;
	writel(value, &regs->spi_page_addr);

	value = SPINAND_USR_PRGMLOAD_CMD(cmd)
		| SPINAND_USR_PRGMLOAD_EN
		| SPINAND_AUTOWEL_BF_PRGMLOAD
		| SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	size = (size + 3) & (~3);
	if (!((u64)buf & 0x03)) {
		/* the buf address is aligned to 4 bytes*/
		for(i=0; i<size; i+=4) {
			value = *(u32 *)(buf+i);
			writel(value, &regs->spi_data_64);
		}
	} else {
		/* the buf address is not aligned to 4 bytes*/
		for(i=0,value=0; i<size; i++) {
			value |= (buf[i] << (8*(i&0x03)));
			if ((i&0x03) == 0x03) {
				writel(value, &regs->spi_data_64);
				value = 0;
			}
		}
	}

	return wait_spi_idle(info);
}

static int spi_nand_pagecache(struct sp_spinand_info *info, u32 row)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 value = 0;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_PAGE2CACHE)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_DATASZ(0)
		| SPINAND_READ_MODE
		| SPINAND_USRCMD_ADDRSZ(3);
	writel(value, &regs->spi_ctrl);

	writel(row, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_CMD_BITMODE(1)
		| SPINAND_CMD_DQ(1)
		| SPINAND_ADDR_BITMODE(1)
		| SPINAND_ADDR_DQ(1)
		| SPINAND_DATA_BITMODE(1)
		| SPINAND_DATAIN_DQ(2)
		| SPINAND_DATAOUT_DQ(1);
	writel(value, &regs->spi_cfg[1]);

	value = SPINAND_AUTO_RDSR_EN;
	writel(value, &regs->spi_auto_cfg);

	return spi_nand_trigger_and_wait_pio(info);
}

/*
 *   memory access mode to read data.
 */
static int spi_nand_readcache(struct sp_spinand_info *info, u32 io_mode,
				u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	int cfg = get_iomode_cfg(io_mode, 0);
	int cmd = get_iomode_readcmd(io_mode);
	u32 value = 0;
	u32 i;

	if (cfg < 0 || cmd < 0)
		return -1;

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE;
	writel(value, &regs->spi_cfg[0]);

	value = SPINAND_DUMMY_CYCLES(8) | cfg;
	writel(value, &regs->spi_cfg[2]);

	value = SPINAND_USR_READCACHE_CMD(cmd)
		| SPINAND_USR_READCACHE_EN
		| SPINAND_AUTO_RDSR_EN;
	writel(value, &regs->spi_auto_cfg);

	do {
		value = readl(&regs->spi_auto_cfg);
	} while((value >> 24) != cmd);

	i = col;
	if (!(col&0x03) && !((u64)buf&0x03)) {
		/* 4 byte aligned case */
		for (; i<(col+size)>>2<<2; i+=4,buf+=4) {
			*(u32*)buf = *(u32*)((u64)SPI_NAND_DIRECT_MAP + i);
		}
	}
	for (; i<(col+size); i++,buf++)
		*buf = *(u8*)((u64)SPI_NAND_DIRECT_MAP + i);

	return wait_spi_idle(info);
}

// real pio mode
/*
static int spi_nand_readcache(struct sp_spinand_info *info, u32 io_mode,
				u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	int cfg = get_iomode_cfg(io_mode);
	int cmd = get_iomode_readcmd(io_mode);
	u32 value = 0;
	u32 i;

	if (cfg < 0 || cmd < 0)
		return -1;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_CTRL_EN
		| SPINAND_USR_CMD(cmd)
		| SPINAND_READ_MODE
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_DATA64_EN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(size);
	writel(value, &regs->spi_cfg[0]);

	value = cfg | SPINAND_DUMMY_CYCLES(8);
	writel(value, &regs->spi_cfg[1]);

	value = col;
	writel(value, &regs->spi_page_addr);

	value = SPINAND_USR_READCACHE_CMD(cmd)
		| SPINAND_USR_READCACHE_EN
		| SPINAND_USR_CMD_TRIGGER;
	writel(value, &regs->spi_auto_cfg);

	for (i=0; i<size; i++) {
		if ((i&0x03) == 0)
			value = readl(&regs->spi_data_64);
		buf[i] = value & 0xff;
		value >>= 8;
	}

	return wait_spi_idle(info);
}
*/

static int spi_nand_read_by_pio(struct sp_spinand_info *info, u32 io_mode,
				u32 row, u32 col, u8 *buf, u32 size)
{
	u32 plane_sel_mode = info->plane_sel_mode;
	int ret = 0;

	if ((plane_sel_mode & 0x1)) {
		u32 pagemark = (plane_sel_mode >> 2) & 0xfff;
		u32 colmark = (plane_sel_mode >> 16) & 0xffff;
		col |= ((row & pagemark) != 0) ? colmark : 0;
	}
	ret = spi_nand_pagecache(info, row);
	if (!ret)
		ret = spi_nand_readcache(info, io_mode, col, buf, size);

	return ret;
}

static int spi_nand_write_by_pio(struct sp_spinand_info *info, u32 io_mode,
				u32 row, u32 col, u8 *buf, u32 size)
{
	u32 plane_sel_mode = info->plane_sel_mode;
	int ret = 0;

	if ((plane_sel_mode & 0x1)) {
		u32 pagemark = (plane_sel_mode >> 2) & 0xfff;
		u32 colmark = (plane_sel_mode >> 16) & 0xffff;
		col |= ((row & pagemark) != 0) ? colmark : 0;
	}
	ret = spi_nand_program_load(info, io_mode, col, buf, size);
	if (!ret)
		ret = spi_nand_program_exec(info, row);

	return ret;
}

static int spi_nand_read_by_pio_auto(struct sp_spinand_info *info, u32 io_mode,
				u32 row, u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 plane_sel_mode = info->plane_sel_mode;
	u32 page_size = info->page_size;
	int cfg = get_iomode_cfg(io_mode, 0);
	int cmd = get_iomode_readcmd(io_mode);
	u32 value;
	u32 i;

	if (cfg < 0 || cmd < 0)
		return -1;

	value = SPINAND_AUTOMODE_EN
		| SPINAND_AUTOCMD_EN
		| SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_PAGE2CACHE)
		| SPINAND_CTRL_EN
		| SPINAND_READ_MODE
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	value = (size+3) & (~3);
	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_DATA64_EN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(value);
	writel(value, &regs->spi_cfg[0]);

	cfg |= SPINAND_DUMMY_CYCLES(8);
	writel(cfg, &regs->spi_cfg[1]);

	writel(row, &regs->spi_page_addr);

	if ((plane_sel_mode & 0x1)) {
		u32 pagemark = (plane_sel_mode >> 2) & 0xfff;
		u32 colmark = (plane_sel_mode >> 16) & 0xffff;
		col |= ((row & pagemark) != 0) ? colmark : 0;
		page_size += ((row & pagemark) != 0) ? colmark : 0;
	}
	writel(col, &regs->spi_col_addr);

	value = SPINAND_SPARE_SIZE(info->oob_size)
		| SPINAND_PAGE_SIZE((page_size >> 10) - 1);
	writel(value, &regs->spi_page_size);

	value = SPINAND_USR_CMD_TRIGGER
		| SPINAND_USR_READCACHE_CMD(cmd)
		| SPINAND_USR_READCACHE_EN;
	writel(value, &regs->spi_auto_cfg);

	for (i=0; i<size; i++) {
		if ((i&0x03) == 0)
			value = readl(&regs->spi_data_64);
		buf[i] = value & 0xff;
		value >>= 8;
	}

	return wait_spi_idle(info);
}

static int spi_nand_write_by_pio_auto(struct sp_spinand_info *info, u32 io_mode,
					u32 row, u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 plane_sel_mode = info->plane_sel_mode;
	u32 page_size = info->page_size;
	int cfg = get_iomode_cfg(io_mode, 1);
	int cmd = get_iomode_writecmd(io_mode);
	u32 value;
	u32 i;

	if (cfg < 0 || cmd < 0)
		return -1;

	value = SPINAND_AUTOMODE_EN
		| SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_PROGEXEC)
		| SPINAND_CTRL_EN
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_DATA64_EN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(size);
	writel(value, &regs->spi_cfg[0]);

	writel(cfg, &regs->spi_cfg[1]);

	writel(row, &regs->spi_page_addr);

	if ((plane_sel_mode & 0x1)) {
		u32 pagemark = (plane_sel_mode >> 2) & 0xfff;
		u32 colmark = (plane_sel_mode >> 16) & 0xffff;
		col |= ((row & pagemark) != 0) ? colmark : 0;
		page_size += ((row & pagemark) != 0) ? colmark : 0;
	}
	writel(col, &regs->spi_col_addr);

	value = SPINAND_SPARE_SIZE(info->oob_size)
		| SPINAND_PAGE_SIZE((page_size >> 10) - 1);
	writel(value, &regs->spi_page_size);

	value = SPINAND_USR_CMD_TRIGGER
		| SPINAND_USR_PRGMLOAD_CMD(cmd)
		| SPINAND_AUTOWEL_BF_PRGMLOAD
		| SPINAND_USR_PRGMLOAD_EN;
	writel(value, &regs->spi_auto_cfg);

	size = (size + 3) & (~3);
	if (!((u64)buf & 0x03)) {
		/* the buf address is aligned to 4 bytes */
		for(i=0; i<size; i+=4) {
			value = *(u32 *)(buf+i);
			writel(value, &regs->spi_data_64);
		}
	} else {
		/* the buf address is not aligned to 4 bytes */
		for(i=0,value=0; i<size; i++) {
			value |= (buf[i] << (8*(i&0x03)));
			if ((i&0x03) == 0x03) {
				writel(value, &regs->spi_data_64);
				value = 0;
			}
		}
	}

	wait_spi_idle(info);

	value = readl(&regs->spi_status);

	return ((value&0x08) ? -1 : 0);
}

static int spi_nand_read_by_dma(struct sp_spinand_info *info, u32 io_mode,
				u32 row, u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 plane_sel_mode = info->plane_sel_mode;
	u32 page_size = info->page_size;
	int cmd = get_iomode_readcmd(io_mode);
	int cfg = get_iomode_cfg(io_mode, 0);
	u32 value = 0;

	if (cmd < 0 || cfg < 0)
		return -1;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	writel(row, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(size);
	writel(value, &regs->spi_cfg[0]);

	value = cfg | SPINAND_DUMMY_CYCLES(8);
	writel(value, &regs->spi_cfg[1]);

	if ((plane_sel_mode & 0x1) != 0) {
		u32 pagemark = (plane_sel_mode>>2)&0xfff;
		u32 colmark = (plane_sel_mode>>16)&0xffff;
		col |= ((row & pagemark) != 0) ? colmark : 0;
		page_size += ((row & pagemark) != 0) ? colmark : 0;
	}
	writel(col, &regs->spi_col_addr);

	value = SPINAND_SPARE_SIZE(info->oob_size)
		| SPINAND_PAGE_SIZE((page_size >> 10) - 1);
	writel(value, &regs->spi_page_size);

	writel((u64)buf, &regs->mem_data_addr);

	value = SPINAND_USR_READCACHE_CMD(cmd)
		| SPINAND_USR_READCACHE_EN;
	writel(value, &regs->spi_auto_cfg);

	return spi_nand_trigger_and_wait_dma(info);
}

static int spi_nand_write_by_dma(struct sp_spinand_info *info, u32 io_mode,
				u32 row, u32 col, u8 *buf, u32 size)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 plane_sel_mode = info->plane_sel_mode;
	u32 page_size = info->page_size;
	int cmd = get_iomode_writecmd(io_mode);
	int cfg = get_iomode_cfg(io_mode, 1);
	u32 value = 0;
	int ret;

	if (cmd < 0 || cfg < 0)
		return -1;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_CTRL_EN
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	writel(row, &regs->spi_page_addr);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(size);
	writel(value, &regs->spi_cfg[0]);

	writel(cfg, &regs->spi_cfg[1]);

	if ((plane_sel_mode & 0x1) != 0) {
		u32 pagemark = (plane_sel_mode>>2)&0xfff;
		u32 colmark = (plane_sel_mode>>16)&0xffff;
		col |= (row & pagemark) ? colmark : 0;
		page_size += (row & pagemark) ? colmark : 0;
	}
	writel(col, &regs->spi_col_addr);

	value = SPINAND_SPARE_SIZE(info->oob_size)
		| SPINAND_PAGE_SIZE((page_size >> 10) - 1);
	writel(value, &regs->spi_page_size);

	writel((u64)buf, &regs->mem_data_addr);

	value = SPINAND_DMA_DONE_MASK;
	writel(value, &regs->spi_intr_msk);
	writel(value, &regs->spi_intr_sts);

	value = SPINAND_USR_PRGMLOAD_CMD(cmd)
		| SPINAND_USR_PRGMLOAD_EN
		| SPINAND_AUTOWEL_BF_PRGMLOAD;
	writel(value, &regs->spi_auto_cfg);

	ret = spi_nand_trigger_and_wait_dma(info);

	if(!ret) {
		value = readl(&regs->spi_status);
		ret = (value & 0x08) ? (-1) : 0;
	}
	return ret;
}

static int spi_nand_pageread_autobch(struct sp_spinand_info *info, u32 io_mode,
					u32 row, u8 *buf)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 plane_sel_mode = info->plane_sel_mode;
	u32 page_size = info->page_size;
	int cmd = get_iomode_readcmd(io_mode);
	int cfg = get_iomode_cfg(io_mode, 0);
	u32 value = 0;
	int ret;

	if (cmd < 0 || cfg < 0)
		return -1;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_CTRL_EN
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(info->page_size);
	writel(value, &regs->spi_cfg[0]);

	value = cfg | SPINAND_DUMMY_CYCLES(8);
	writel(value, &regs->spi_cfg[1]);

	writel(row, &regs->spi_page_addr);

	value = 0;
	if ((plane_sel_mode & 0x1) != 0) {
		u32 pagemark = (plane_sel_mode>>2)&0xfff;
		u32 colmark = (plane_sel_mode>>16)&0xffff;
		value |= ((row & pagemark) != 0) ? colmark : 0;
		page_size += ((row & pagemark) != 0) ? colmark : 0;
	}
	writel(value, &regs->spi_col_addr);

	writel(page_size, &regs->device_parity_addr);

	value = SPINAND_SPARE_SIZE(info->oob_size)
		| SPINAND_PAGE_SIZE((page_size >> 10) - 1);
	writel(value, &regs->spi_page_size);

	writel((u64)buf, &regs->mem_data_addr);
	writel((u64)buf+info->page_size, &regs->mem_parity_addr);

	value = SPINAND_BCH_DECSRC(info->bch_dec_src)
		| SPINAND_BCH_DATA_LEN(info->parity_sector_size)
		| SPINAND_BCH_BLOCKS(info->nand.ecc.steps - 1)
		| SPINAND_BCH_AUTO_EN;
	value |= (info->parity_sector_size & 31) ?
			SPINAND_BCH_ALIGN_16B : SPINAND_BCH_ALIGN_32B;
	value |= (info->nand.ecc.size == 1024) ?
			SPINAND_BCH_1K_MODE : SPINAND_BCH_512B_MODE;
	writel(value, &regs->spi_bch);

	value = SPINAND_USR_READCACHE_CMD(cmd)
		| SPINAND_USR_READCACHE_EN;
	writel(value, &regs->spi_auto_cfg);

	sp_autobch_config(info->mtd, buf, buf+info->page_size, 0, info->bch_dec_src);

	ret = spi_nand_trigger_and_wait_dma(info);

	if(!ret)
		ret = sp_autobch_result(info->mtd);

	writel(0, &regs->spi_bch); /* close auto bch */

	return ret;
}

static int spi_nand_pagewrite_autobch(struct sp_spinand_info *info, u32 io_mode,
					u32 row, u8 *buf)
{
	struct sp_spinand_regs *regs = info->regs;
	u32 plane_sel_mode = info->plane_sel_mode;
	u32 page_size = info->page_size;
	int cmd = get_iomode_writecmd(io_mode);
	int cfg = get_iomode_cfg(io_mode, 1);
	u32 value = 0;
	int ret = 0;

	if (cmd < 0 || cfg < 0)
		return -1;

	value = SPINAND_SEL_CHIP_A
		| SPINAND_SCK_DIV(info->spi_clk_div)
		| SPINAND_USR_CMD(SPINAND_CMD_PROGEXEC)
		| SPINAND_CTRL_EN
		| SPINAND_WRITE_MODE
		| SPINAND_USRCMD_ADDRSZ(2);
	writel(value, &regs->spi_ctrl);

	value = SPINAND_LITTLE_ENDIAN
		| SPINAND_TRS_MODE
		| SPINAND_DATA_LEN(info->page_size);
	writel(value, &regs->spi_cfg[0]);

	value = cfg;
	writel(value, &regs->spi_cfg[1]);

	writel(row, &regs->spi_page_addr);

	value = 0;
	if ((plane_sel_mode & 0x1) != 0) {
		u32 pagemark = (plane_sel_mode>>2)&0xfff;
		u32 colmark = (plane_sel_mode>>16)&0xffff;
		value |= ((row & pagemark) != 0) ? colmark : 0;
		page_size += ((row & pagemark) != 0) ? colmark : 0;
	}
	writel(value, &regs->spi_col_addr);

	writel(page_size, &regs->device_parity_addr);

	value = SPINAND_SPARE_SIZE(info->oob_size)
		| SPINAND_PAGE_SIZE((page_size >> 10) - 1);
	writel(value, &regs->spi_page_size);

	writel((u64)buf, &regs->mem_data_addr);
	writel((u64)buf+info->page_size, &regs->mem_parity_addr);

	value = SPINAND_BCH_DECSRC(info->bch_dec_src)
		| SPINAND_BCH_DATA_LEN(info->parity_sector_size)
		| SPINAND_BCH_BLOCKS(info->nand.ecc.steps - 1)
		| SPINAND_BCH_AUTO_EN;
	value |= (info->parity_sector_size & 31) ?
			SPINAND_BCH_ALIGN_16B : SPINAND_BCH_ALIGN_32B;
	value |= (info->nand.ecc.size == 1024) ?
			SPINAND_BCH_1K_MODE : SPINAND_BCH_512B_MODE;
	writel(value, &regs->spi_bch);

	sp_autobch_config(info->mtd, buf, buf+info->page_size, 1, info->bch_dec_src);

	value = SPINAND_USR_PRGMLOAD_CMD(cmd)
		| SPINAND_USR_PRGMLOAD_EN
		| SPINAND_AUTOWEL_BF_PRGMLOAD;
	writel(value, &regs->spi_auto_cfg);

	ret = spi_nand_trigger_and_wait_dma(info);
	if(!ret)
		ret = sp_autobch_result(info->mtd);

	writel(0, &regs->spi_bch); /* close auto bch */

	if (!ret) {
		value = spi_nand_getfeatures(info, DEVICE_STATUS_ADDR);
		if ((value & DEVICE_STATUS_OIP_MSK) != 0) {
			spi_nand_wait_dev_idle(info);
			ret = -1;
		} else {
			ret = (value & DEVICE_STATUS_PFAIL_MSK) ? (-1) : 0;
		}
	}
	return ret;
}

static int sp_spinand_read_raw(struct sp_spinand_info *info,
				u32 row, u32 col, u32 size)
{
	int ret = -1;
	u8 io = info->read_bitmode;
	u8 trsmode = info->raw_trs_mode;
	u8 *va = info->buff.virt + info->buff.idx;
	u8 *pa = (u8*)info->buff.phys + info->buff.idx;

	if (trsmode == SPINAND_TRS_DMA) {
		ret = spi_nand_read_by_dma(info, io, row, col, pa, size);
	} else if (trsmode == SPINAND_TRS_PIO_AUTO) {
		ret = spi_nand_read_by_pio_auto(info, io, row, col, va, size);
	} else if (trsmode == SPINAND_TRS_PIO) {
		ret = spi_nand_read_by_pio(info, io, row, col, va, size);
	}
	return ret;
}

static int sp_spinand_write_raw(struct sp_spinand_info *info,
				u32 row, u32 col, u32 size)
{
	int ret = -1;
	u8 io = info->write_bitmode;
	u8 trsmode = info->raw_trs_mode;
	u8 *va = info->buff.virt + info->buff.idx;
	u8 *pa = (u8*)info->buff.phys + info->buff.idx;

	if (trsmode == SPINAND_TRS_DMA) {
		ret = spi_nand_write_by_dma(info, io, row, col, pa, size);
	} else if (trsmode == SPINAND_TRS_PIO_AUTO) {
		ret = spi_nand_write_by_pio_auto(info, io, row, col, va, size);
	} else if (trsmode == SPINAND_TRS_PIO) {
		ret = spi_nand_write_by_pio(info, io, row, col, va, size);
	}
	return ret;
}

static void sp_spinand_select_chip(struct mtd_info *mtd, int chipnr)
{
	struct sp_spinand_info *info = get_spinand_info();
	if (info->chip_num > 1 && info->cur_chip != chipnr && chipnr >= 0) {
		info->cur_chip = chipnr;
		spi_nand_select_die(info, chipnr);
	}
}

static void sp_spinand_cmd_ctrl(struct mtd_info *mtd, int cmd, u32 ctrl)
{
	return;
}

static void sp_spinand_cmdfunc(struct mtd_info *mtd, u32 cmd, int col, int row)
{
	struct sp_spinand_info *info = get_spinand_info();

	info->cmd = cmd;
	switch (cmd) {
	case NAND_CMD_READOOB:
		info->buff.idx = 0;
		info->col = col + info->mtd->writesize;
		info->row = row;
		break;
	case NAND_CMD_READ0:
		info->buff.idx = 0;
		info->col = col;
		info->row = row;
		break;
	case NAND_CMD_SEQIN:
		info->buff.idx = 0;
		info->col = col;
		info->row = row;
		break;
	case NAND_CMD_PAGEPROG:
		if(info->buff.idx) {
			u32 size = info->buff.idx;
			info->buff.idx = 0;
			#ifndef CONFIG_SPINAND_USE_SRAM
			flush_dcache_range((unsigned long)info->buff.virt,
				(unsigned long)info->buff.virt + size);
			#endif
			sp_spinand_write_raw(info, info->row, info->col, size);
		}
		break;
	case NAND_CMD_ERASE1:
		row &= ~(info->mtd->erasesize / info->mtd->writesize - 1);
		spi_nand_blkerase(info, row);
		break;
	case NAND_CMD_ERASE2:
		break;
	case NAND_CMD_STATUS:
		info->buff.idx = 0;
		break;
	case NAND_CMD_RESET:
		spi_nand_reset(info);
		break;
	case NAND_CMD_READID:
		info->buff.idx = 0;
		info->col = col;
		break;
	case NAND_CMD_PARAM:
	case NAND_CMD_GET_FEATURES:
	case NAND_CMD_SET_FEATURES:
		/* these cmds are p-nand related, ignore them */
		break;
	default:
		SPINAND_LOGE("sp_spinand: unknown command=0x%02x.\n", cmd);
		break;
	}
}

static int sp_spinand_dev_ready(struct mtd_info *mtd)
{
	struct sp_spinand_info *info = get_spinand_info();

	return ((spi_nand_getfeatures(info, DEVICE_STATUS_ADDR) & 0x01) == 0);
}

static int sp_spinand_waitfunc(struct mtd_info *mtd, struct nand_chip *chip)
{
	struct sp_spinand_info *info = get_spinand_info();
	u32 timeout = (CONFIG_SYS_HZ * 400) / 1000;
 	u32 time_start;
	int status;
	int ret;

 	time_start = get_timer(0);
	do {
		status = spi_nand_getfeatures(info, DEVICE_STATUS_ADDR);
		if ((status & 0x01) == 0)
			break;
	} while (get_timer(time_start) < timeout);

	/* program/erase fail bit */
	if (info->cmd == NAND_CMD_PAGEPROG && (status&0x08))
		ret = 1;
	else if ((info->cmd == NAND_CMD_ERASE2) && (status&0x04))
		ret = 1;
	else
		ret = (status & 0x0c) ? 0x01 : 0x00;

	/* ready bit */
	ret |= (status & 0x01) ? 0x00 : 0x40;

	/* write protection bit*/
	ret |= (info->dev_protection & PROTECT_STATUS) ? 0x00 : 0x80;
	return ret;
}

static void sp_spinand_read_buf(struct mtd_info *mtd, u8 *buf, int len)
{
	struct sp_spinand_info *info = get_spinand_info();
	u32 value;

	if (info->buff.idx == 0) {
		switch (info->cmd) {
		case NAND_CMD_READOOB:
		case NAND_CMD_READ0: {
			u32 size = info->page_size + info->oob_size - info->col;
			#ifndef CONFIG_SPINAND_USE_SRAM
			invalidate_dcache_range( (unsigned long)info->buff.virt,
				(unsigned long)info->buff.virt + size);
			#endif
			sp_spinand_read_raw(info, info->row, info->col, size);
			break;
		}
		case NAND_CMD_READID:
			spi_nand_readid(info, info->col, info->buff.virt);
			break;
		case NAND_CMD_STATUS:
			value = spi_nand_getfeatures(info, DEVICE_STATUS_ADDR);
			*(u32 *)info->buff.virt  = (value&0x0c) ? 0x01 : 0x00;
			*(u32 *)info->buff.virt |= (value&0x01) ? 0x00 : 0x40;
			if(!(info->dev_protection & PROTECT_STATUS))
				*(u32 *)info->buff.virt |= 0X80;
			memcpy(buf, info->buff.virt + info->buff.idx, len);
			return;
		default:
			break;
		}
	}

	memcpy(buf, info->buff.virt + info->buff.idx, len);
	info->buff.idx += len;
}

static u8 sp_spinand_read_byte(struct mtd_info *mtd)
{
	u8 ret = 0;
	sp_spinand_read_buf(mtd, &ret, 1);
	return ret;
}

static void sp_spinand_write_buf(struct mtd_info *mtd, const u8 *buf, int len)
{
	struct sp_spinand_info *info = get_spinand_info();

	memcpy(info->buff.virt + info->buff.idx, buf, len);
	info->buff.idx += len;
}

static int sp_spinand_read_page(struct mtd_info *mtd, struct nand_chip *chip,
				u8 *buf, int oob_required, int page)
{
	struct sp_spinand_info *info = get_spinand_info();
	u8 *data_va = info->buff.virt;
	u8 *oob_va = data_va + mtd->writesize;
	dma_addr_t data_pa = info->buff.phys;
	dma_addr_t oob_pa = data_pa + mtd->writesize;
	int ret;

	#ifndef CONFIG_SPINAND_USE_SRAM
	invalidate_dcache_range( (unsigned long)data_va,
		(unsigned long)data_va + mtd->writesize + mtd->oobsize);
	#endif

	if (info->trs_mode == SPINAND_TRS_DMA_AUTOBCH) {
		ret = spi_nand_pageread_autobch(info,
			info->read_bitmode, page, (u8*)data_pa);
	} else {
		ret = sp_spinand_read_raw(info,
			page, 0, mtd->writesize + mtd->oobsize);
		if(ret == 0)
			ret = sp_bch_decode(mtd, (void*)data_pa, (void*)oob_pa);
	}

	if (ret < 0) {
		SPINAND_LOGE("sp_spinand: bch decode failed at page=%d\n",page);
		return ret;
	}

	memcpy(buf, data_va, mtd->writesize);
	if (oob_required)
		memcpy(chip->oob_poi, oob_va, mtd->oobsize);
	return 0;
}

static int sp_spinand_write_page(struct mtd_info *mtd, struct nand_chip *chip,
				 const u8 *buf, int oob_required, int page)
{
	struct sp_spinand_info *info = get_spinand_info();
	u8 *data_va = info->buff.virt;
	u8 *oob_va = data_va + mtd->writesize;
	dma_addr_t data_pa = info->buff.phys;
	dma_addr_t oob_pa = data_pa + mtd->writesize;
	int ret;

	memcpy(data_va, buf, mtd->writesize);
	memcpy(oob_va, chip->oob_poi, mtd->oobsize);

	#ifndef CONFIG_SPINAND_USE_SRAM
	flush_dcache_range( (unsigned long)data_va,
		(unsigned long)data_va + mtd->writesize + mtd->oobsize);
	#endif

	if(info->trs_mode == SPINAND_TRS_DMA_AUTOBCH) {
		ret = spi_nand_pagewrite_autobch(info,
			info->write_bitmode, info->row, (u8*)data_pa);
	} else {
		sp_bch_encode(mtd, (void*)data_pa, (void*)oob_pa);
		ret = sp_spinand_write_raw(info,
			info->row, 0, mtd->writesize+mtd->oobsize);
	}
	return ret;
}

static int sp_spinand_init(struct sp_spinand_info *info)
{
	struct nand_chip *nand = &info->nand;
	struct mtd_info *mtd = &nand->mtd;
	u32 value;
	int ret;
	int i;

	info->mtd = mtd;
	mtd->priv = nand;
	nand->IO_ADDR_R = nand->IO_ADDR_W = info->regs;

	info->buff.size = CONFIG_SPINAND_BUF_SZ;
	#ifdef CONFIG_SPINAND_USE_SRAM
	info->buff.virt = (u8*)CONFIG_SPINAND_SRAM_ADDR;
	#else
	u8 *buf = (u8*)malloc(info->buff.size + CONFIG_SYS_CACHELINE_SIZE);
	if (!buf) {
		ret = -ENOMEM;
		goto err_out;
	}
	/* the buff should be cacheline-aligned */
	info->buff.virt_base = buf;
	buf = buf + CONFIG_SYS_CACHELINE_SIZE - 1;
	info->buff.virt = (u8*)((u64)buf & (~(CONFIG_SYS_CACHELINE_SIZE-1)));
	#endif

	info->buff.phys = (dma_addr_t)info->buff.virt;
	SPINAND_LOGI("%s in\n", __FUNCTION__);
	SPINAND_LOGI("buff=0x%p@0x%08llx size=%u\n", info->buff.virt,
		info->buff.phys, info->buff.size);

	info->nand.select_chip = sp_spinand_select_chip;
	info->nand.cmd_ctrl = sp_spinand_cmd_ctrl;
	info->nand.cmdfunc = sp_spinand_cmdfunc;
	info->nand.dev_ready = sp_spinand_dev_ready;
	info->nand.waitfunc = sp_spinand_waitfunc;

	info->nand.read_byte = sp_spinand_read_byte;
	info->nand.read_buf = sp_spinand_read_buf;
	info->nand.write_buf = sp_spinand_write_buf;

	info->nand.ecc.read_page = sp_spinand_read_page;
	info->nand.ecc.write_page = sp_spinand_write_page;
	info->nand.ecc.layout = &info->ecc_layout;
	info->nand.ecc.mode = NAND_ECC_HW;

#if 0 // We can't find this register on eCV5546
	SPINAND_SET_CLKSRC(CONFIG_SPINAND_CLK_SRC);
#endif
	info->spi_clk_div = CONFIG_SPINAND_CLK_DIV;

	if (spi_nand_reset(info) < 0) {
		SPINAND_LOGE("reset fail!\n");
		ret = -EIO;
		goto err_out;
	}

	/* Read ID */
	spi_nand_readid(info, 0, (u8*)&info->id);

	ret = nand_scan_ident(mtd, 1, (struct nand_flash_dev *)sp_spinand_ids);
	if (ret < 0) {
		SPINAND_LOGE("unsupport device(id=0x%08x)!\n", info->id);
		ret = ENXIO;
		goto err_out;
	}

	if (info->nand.drv_options & SPINAND_OPT_HAS_TWO_PLANE) {
		info->plane_sel_mode = (0x1000<<16) | (0x40<<2) | 3;
	} else {
		info->plane_sel_mode = 0;
	}
	info->page_size = mtd->writesize;
	info->oob_size = mtd->oobsize;
	info->dev_name = mtd->name;

	info->bch_dec_src = CONFIG_SPINAND_AUTOBCH_DECSRC;
	info->trs_mode = CONFIG_SPINAND_TRSMODE;
	info->raw_trs_mode = CONFIG_SPINAND_TRSMODE_RAW;

	info->read_bitmode = CONFIG_SPINAND_READ_BITMODE;
	if ((info->read_bitmode == SPINAND_4BIT_MODE)
		&& (info->nand.drv_options & SPINAND_OPT_NO_4BIT_READ))
		info->read_bitmode = SPINAND_2BIT_MODE;
	if ((info->read_bitmode == SPINAND_2BIT_MODE)
		&& (info->nand.drv_options & SPINAND_OPT_NO_2BIT_READ))
		info->read_bitmode = SPINAND_1BIT_MODE;

	info->write_bitmode = CONFIG_SPINAND_WRITE_BITMODE;
	if (info->nand.drv_options & SPINAND_OPT_NO_4BIT_PROGRAM)
		info->write_bitmode = SPINAND_1BIT_MODE;

	info->chip_num = SPINAND_OPT_GET_DIENUM(info->nand.drv_options);
	info->cur_chip = -1;
	if (info->chip_num > 1) {
		nand->numchips = info->chip_num;
		mtd->size = info->chip_num * nand->chipsize;
	}

	for (i=0; i<info->chip_num; i++) {
		sp_spinand_select_chip(mtd, i);
		if (info->nand.drv_options & SPINAND_OPT_ECCEN_IN_F90_4) {
			value = spi_nand_getfeatures(info, 0x90);
			value &= ~0x01;
			spi_nand_setfeatures(info, 0x90, value);
		}

		value = spi_nand_getfeatures(info, DEVICE_FEATURE_ADDR);
		value &= ~0x10;          /* disable internal ECC */
		if (info->nand.drv_options & SPINAND_OPT_HAS_BUF_BIT)
			value |= 0x08;   /* use buffer read mode */
		if (info->nand.drv_options & SPINAND_OPT_HAS_CONTI_RD)
			value &= ~0x01;  /* disable continuous read mode */
		if (info->nand.drv_options & SPINAND_OPT_HAS_QE_BIT)
			value |= 0x01;   /* enable quad io */
		spi_nand_setfeatures(info, DEVICE_FEATURE_ADDR, value);

		/* close write protection */
		spi_nand_setfeatures(info, DEVICE_PROTECTION_ADDR, 0x0);
		info->dev_protection = spi_nand_getfeatures(info, DEVICE_PROTECTION_ADDR);
	}

	ret = sp_bch_init(mtd, (int*)&info->parity_sector_size);
	if (ret < 0) {
		SPINAND_LOGE("initialize BCH controller fail!\n");
		goto err_out;
	}

	SPINAND_LOGI("\tdevice name : %s\n", mtd->name);
	SPINAND_LOGI("\tdevice id   : 0x%08x\n", info->id);
	SPINAND_LOGI("\toptions     : 0x%08x\n", nand->options);
	SPINAND_LOGI("\tdrv options : 0x%08x\n", nand->drv_options);
	SPINAND_LOGI("\tchip number : %d\n", info->chip_num);
	SPINAND_LOGI("\tblock size  : %d\n", mtd->erasesize);
	SPINAND_LOGI("\tpage size   : %d\n", mtd->writesize);
	SPINAND_LOGI("\toob size    : %d\n", mtd->oobsize);
	SPINAND_LOGI("\toob avail   : %d\n", nand->ecc.layout->oobavail);
	SPINAND_LOGI("\tecc size    : %d\n", nand->ecc.size);
	SPINAND_LOGI("\tecc strength: %d\n", nand->ecc.strength);
	SPINAND_LOGI("\tecc steps   : %d\n", nand->ecc.steps);
	ret = nand_scan_tail(mtd);
	if (ret < 0) {
		goto err_out;
	}

	nand_register(0, mtd);

	return 0;

err_out:
	#ifndef CONFIG_SPINAND_USE_SRAM
	if (info->buff.virt_base) {
		free(info->buff.virt_base);
		info->buff.virt_base = NULL;
	}
	#endif

	return ret;
}

static int sp_spinand_probe(struct udevice *dev)
{
	struct sp_spinand_info *info;
	info = our_spinfc = dev_get_priv(dev);
	struct clk clk_spind, clk_bch;
	int ret;

	printk(KERN_INFO "\n");     // used for message alignment

	/* get spi-nand reg */
	info->regs = (void __iomem*)devfdt_get_addr_name(dev, "nand");
	//SPINAND_LOGI("sp_spinand: regs@0x%p\n", info->regs);

	/* get bch reg */
	info->bch_regs = (void __iomem*)devfdt_get_addr_name(dev, "bch");
	//SPINAND_LOGI("sp_bch	  : regs@0x%p\n", info->bch_regs);

	/* get and enable spi-nand clock */
	ret = clk_get_by_name(dev, "clk_nand", &clk_spind);
	if (ret < 0) {
		SPINAND_LOGE("get clk_spind fail\n");
		return ret;
	}

	ret = clk_enable(&clk_spind);
	if (ret)
		return ret;
	//SPINAND_LOGI("clk_enable for SPI-NAND succeed\n");

	/* get and enable bch clock */
	ret = clk_get_by_name(dev, "clk_bch", &clk_bch);
	if (ret < 0) {
		SPINAND_LOGE("get clk_bch fail\n");
		return ret;
	}

	ret = clk_enable(&clk_bch);
	if (ret)
		return ret;
	//SPINAND_LOGI("clk_enable for BCH succeed\n");

	/* do sp_spinand initialization */
	ret = sp_spinand_init(info);

	return ret;
}

static const struct udevice_id eys3d_spinand[] = {
	{ .compatible = "eys3d,ecv5546-spi-nand"},
	{}
};


U_BOOT_DRIVER(pentagram_spi_nand) = {
	.name           = "pentagram_spi_nand",
	.id             = UCLASS_MTD,
	.of_match       = eys3d_spinand,
	.priv_auto      = sizeof(struct sp_spinand_info),
	.probe          = sp_spinand_probe,
};

void board_spinand_init(void)
{
	struct udevice *dev;
	int ret;

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(pentagram_spi_nand),
					  &dev);

	if (ret && ret != -ENODEV)
		SPINAND_LOGW("initialize eys3d SPI NAND controller fail!\n");
}

#ifdef CONFIG_SPINAND_MEASURE_TIMIMNG
void pattern_generate(u32 data, u8 *buf, u32 size)
{
	u32 i = 0;
	u8 *p = (u8 *)&data;

	printk("%s, data: 0x%08x, size: 0x%08x\n", __FUNCTION__, data, size);

	for(i=0; i<size; i++) {
		buf[i] = *(p+(i&0x03));
	}
}

int pattern_check(u32 data, u8 *buf, u32 size)
{
	int i = 0;
	u8 *p = (u8*)&data;

	printk("%s, data: 0x%08x, size: 0x%08x\n", __FUNCTION__, data, size);

	for(i=0; i<size; i++) {
		if(buf[i] != *(p+(i&0x03))) {
			printk("\rdata mis-match. i: %d, buf[i]: 0x%02x, *(p+(i&0x03): 0x%02x\n", i, buf[i], *(p+(i&0x03)));
			return -1;
		}
	}
	return 0;
}
#else
void pattern_generate(u32 data, u8 *buf, u32 size)
{
	u32 i = 0;
	u8 *p = (u8 *)&data;
	for(i=0; i<size; i++) {
		buf[i] = *(p+(i&0x03));
		if((i&0x03) == 0x03)
			data++;
	}
}

int pattern_check(u32 data, u8 *buf, u32 size)
{
	int i = 0;
	u8 *p = (u8*)&data;
	for(i=0; i<size; i++) {
		if(buf[i] != *(p+(i&0x03))) {
			printk("\rdata mis-match at :0x%08x\n", data);
			return -1;
		}
		if((i&0x03) == 0x03)
			data++;
	}
	return 0;
}
#endif

static int sp_spinand_test_rw(u32 addr, u32 size, u32 seed)
{
	struct sp_spinand_info *info = get_spinand_info();
	struct mtd_info *mtd = info->mtd;
	nand_erase_options_t erase_opts;
	u32 block_size = mtd->erasesize;
	u32 block_num = (size + block_size - 1) / block_size;
	u32 start_block = addr / block_size;
	size_t actual_size;
	u32 now_seed;
	u32 i, j;
	int ret;
	u8 *buf;

	buf = (u8 *)malloc(block_size);
	if (!buf) {
		printk("no memory!\n");
		goto err_out;
	}

	printk("start block:%d, block_num:%d, seed:%d\n", start_block, block_num, seed);

	for (i=0,j=start_block; i<block_num; i++,j++) {
		while (nand_block_isbad(mtd, (loff_t)j*block_size))
			j++;

		printk("\rerase block:%d ", j);

		memset(&erase_opts, 0, sizeof(erase_opts));
		erase_opts.quiet = 1;
		erase_opts.offset = (loff_t)j * block_size;
		erase_opts.length = block_size;
		ret = nand_erase_opts(mtd, &erase_opts);
		if (ret) {
			printk("=> fail\n");
			goto err_out;
		}
	}
	printk("\rerase blocks => success\n");

	now_seed = seed;
	for (i=0,j=start_block; i<block_num; i++,j++) {
		/* skip bad block */
		while (nand_block_isbad(mtd, (loff_t)j*block_size))
			j++;

		printk("\rwrite block:%d ",j);

		/* prepare the pattern */
		now_seed += (i * block_size) / 4;
		pattern_generate(now_seed, buf, block_size);

		/* write block */
		actual_size = block_size;
		ret = nand_write(mtd, (loff_t)j*block_size, &actual_size, buf);
		if (ret) {
			printk("=> fail\n");
			goto err_out;
		}
	}
	printk("\rwrite blocks => success\n");


	now_seed = seed;
	for (i=0,j=start_block; i<block_num; i++,j++) {
		now_seed += (i * block_size) / 4;

		/* skip bad block */
		while (nand_block_isbad(mtd, (loff_t)j*block_size))
			j++;

		printk("\rread block:%d ",j);

		actual_size = block_size;
		ret = nand_read(mtd, (loff_t)j*block_size, &actual_size, buf);
		if (ret) {
			printk("=> fail\n");
			goto err_out;
		}

		if (pattern_check(now_seed, buf, block_size) == -1) {
			printk("=> data mis-match\n");
			goto err_out;
		}
	}
	printk("\rread blocks => success\n");

	free(buf);
	return 0;

err_out:
	if (buf)
		free(buf);

	return 0;
}

static int sp_spinand_test_speed(void)
{
	#define START_BLOCK_NUM (20)        /* This start block number is in blank area of uboot2 partition */
	#define TEST_BLOCK_NUM (10)
	const char *trs_mode[] = {"pio", "pio+auto", "dma", "dma+autobch"};
	const char *io_mode[] = {"x1", "x2", "x4", "dualIO", "quadIO"};
	struct sp_spinand_info *info = get_spinand_info();
	struct mtd_info *mtd = info->mtd;
	nand_erase_options_t erase_opts;
	u32 test_block_table[TEST_BLOCK_NUM];
	unsigned long now_time;
	unsigned long used_time;
	unsigned long speed;
	unsigned long average;
	unsigned long data_size = mtd->erasesize * TEST_BLOCK_NUM;
	size_t actual_size;
	u8  *buf;
	s32 i, j;
	int ret;

	for(i=0,j=START_BLOCK_NUM; i<TEST_BLOCK_NUM; i++,j++) {
		while (nand_block_isbad(mtd, (loff_t)j * mtd->erasesize))
			j++;
		test_block_table[i] = j*mtd->erasesize;
	}

	buf = (u8 *)malloc(data_size);
	if(!buf) {
		printk("no memory!\n");
		goto err_out;
	}

	pattern_generate(0, buf, data_size);

	printk( "[clk_sel:%d, "
		"clk_div:%d, "
		"trs_mode:%s, "
		"read_io:%s, "
		"write_io:%s]\n",
		SPINAND_GET_CLKSRC(),
		info->spi_clk_div,
		trs_mode[info->trs_mode],
		io_mode[info->read_bitmode],
		io_mode[info->write_bitmode]);
	printk("Erase: ");
	now_time = get_timer(0);
	for (i=0; i<TEST_BLOCK_NUM; i++) {
		memset(&erase_opts, 0, sizeof(erase_opts));
		erase_opts.quiet = 1;
		erase_opts.offset = test_block_table[i];
		erase_opts.length = mtd->erasesize;
		ret = nand_erase_opts(mtd, &erase_opts);
		if(ret) {
			printk("failed at 0x%08x\n", test_block_table[i]);
			goto err_out;
		}
	}
	used_time = get_timer(now_time);
	average = used_time * 100 / TEST_BLOCK_NUM;
	speed = ((data_size>>10) * 1000 * 100 / used_time) >> 10; //unit:100MB/s
	printk("total_size=%ld Byte, "
		"total_time:%ld ms, "
		"average:%ld.%ld ms/block, "
		"speed:%ld.%ld MB/s\n",
		data_size,
		used_time,
		average/100, average%100,
		speed/100, speed%100);

	printk("Write: ");
	now_time = get_timer(0);
	for (i=0; i<TEST_BLOCK_NUM; i++) {
		actual_size = mtd->erasesize;
		ret = nand_write(mtd, test_block_table[i],
				&actual_size, buf+mtd->erasesize*i);
		if(ret) {
			printk("failed at 0x%08x\n", test_block_table[i]);
			goto err_out;
		}
	}
	used_time = get_timer(now_time);
	average = used_time * 100 / TEST_BLOCK_NUM;
	speed = ((data_size>>10) * 1000 * 100 / used_time) >> 10; //unit:100MB/s
	printk("total_size=%ld Byte, "
		"total_time:%ld ms, "
		"average:%ld.%ld ms/block, "
		"speed:%ld.%ld MB/s\n",
		data_size,
		used_time,
		average/100, average%100,
		speed/100, speed%100);

	memset(buf, 0, data_size);

	printk("Read: ");
	now_time = get_timer(0);
	for (i=0; i<TEST_BLOCK_NUM; i++) {
		actual_size = mtd->erasesize;
		ret = nand_read(mtd, test_block_table[i],
				&actual_size, buf+mtd->erasesize*i);
		if(ret) {
			printk("failed at :0x%08x\n\n",test_block_table[i]);
			goto err_out;
		}
	}
	used_time = get_timer(now_time);

	if(pattern_check(0, buf, data_size)) {
		printk("failed for data mismatch\n\n");
		goto err_out;
	}
	average = used_time * 100 / TEST_BLOCK_NUM;
	speed = ((data_size>>10) * 1000 * 100 / used_time) >> 10; //unit:100MB/s
	printk("total_size=%ld Byte, "
		"total_time:%ld ms, "
		"average:%ld.%ld ms/block, "
		"speed:%ld.%ld MB/s\n\n",
		data_size,
		used_time,
		average/100, average%100,
		speed/100, speed%100);

	free(buf);
	return 0;

err_out:
	if(buf)
		free(buf);
	return -1;
}

static int sp_spinand_test_stress(void)
{
	struct sp_spinand_info *info = get_spinand_info();
	struct mtd_info *mtd = info->mtd;
	struct nand_chip *nand = &info->nand;
	nand_erase_options_t erase_opts;
	u32 blocks = nand->chipsize / mtd->erasesize;
	u32 data_size = mtd->erasesize;
	size_t actual_size;
	u32 progress;
	u32 offset;
	u8  *src_buf;
	u8  *dst_buf;
	u8  *block_table;
	u32 i, j, k;
	int ret;

	src_buf = (u8 *)malloc(data_size*2 +blocks*4);
	if(!src_buf) {
		printk("no memory!\n");
		return -1;
	}
	dst_buf = src_buf + data_size;
	block_table = dst_buf + data_size;

	pattern_generate(0, src_buf, data_size);

	for (k=0; k<100000; k++) {
		memset(block_table, 0, blocks);
		for(i=0,j=0,progress=0; i<blocks; i++) {
			/* erase */
			memset(&erase_opts, 0, sizeof(erase_opts));
			offset = mtd->erasesize * i;
			erase_opts.offset = offset;
			erase_opts.length = mtd->erasesize;
			erase_opts.quiet = 1;
			ret = nand_erase_opts(mtd, &erase_opts);
			if(ret) {
				block_table[i] = 0xff;
				j++;
				continue;
			}

			/* write */
			actual_size = mtd->erasesize;
			ret = nand_write(mtd,  offset, &actual_size, src_buf);
			if (ret || actual_size!=mtd->erasesize) {
				block_table[i] = 0xff;
				j++;
				continue;
			}

			/* read */
			actual_size = mtd->erasesize;
			ret = nand_read(mtd, offset, &actual_size, dst_buf);
			if (ret	|| actual_size != mtd->erasesize
				|| pattern_check(0, dst_buf, data_size)<0) {
				block_table[i] = 0xff;
				j++;
				continue;
			}
			block_table[i] = 1;
			if(progress != i*100/blocks) {
				progress = i*100/blocks;
				printk("\rprgress : %d%%", progress);
			}
		}
		printk("\r==> %d round, %d block fail\n", k, j);
		if(j > 0) {
			printk("bad blocks:\n");
			for (i=0; i < blocks; i++) {
				if(block_table[i] == 0xff)
					printk("0x%08x\n", i*mtd->erasesize);
			}
		}
	}

	free(src_buf);

	return 0;
}

static int sp_spinand_test_stress1(u32 block)
{
	struct sp_spinand_info *info = get_spinand_info();
	struct mtd_info *mtd = info->mtd;
	nand_erase_options_t erase_opts;
	u32 row_addr = mtd->erasesize * block;
	u32 data_size = mtd->erasesize;
	size_t actual_size;
	u8  *src_buf;
	u8  *dst_buf;
	u32 k;
	int ret;

	src_buf = (u8 *)malloc(data_size * 2);
	if(!src_buf) {
		printk("no memory!\n");
		return -1;
	}
	dst_buf = src_buf + data_size;

	pattern_generate(0, src_buf, data_size);

	for(k=0; ; k++) {
		/* erase */
		memset(&erase_opts, 0, sizeof(erase_opts));
		erase_opts.quiet = 1;
		erase_opts.offset = row_addr;
		erase_opts.length = mtd->erasesize;
		ret = nand_erase_opts(mtd, &erase_opts);
		if(ret) {
			printk("\nErase fail at 0x%08x\n", row_addr);
			break;
		}

		/* write */
		actual_size = mtd->erasesize;
		ret = nand_write(mtd,  row_addr, &actual_size, src_buf);
		if (ret || actual_size!=mtd->erasesize) {
			printk("\nWrite fail at 0x%08x\n", row_addr);
			break;
		}

		/* read */
		actual_size = mtd->erasesize;
		ret = nand_read(mtd, row_addr, &actual_size, dst_buf);
		if (ret	|| actual_size != mtd->erasesize
			|| pattern_check(0, dst_buf, data_size)<0) {
			printk("\nWrite fail at 0x%08x\n", row_addr);
			break;
		}

		if(k%10 == 0) {
			printk("\r%d round test pass!", k);
		}
	}

	printk("Stress test exit at %d round.\n", k);

	free(src_buf);

	return 0;
}

static int sp_spinand_test_timing(u8 clk_bypass, u8 dq_bypass, u8 set_now,
				u8 rts_only, u32 *rts, u32 *softpad)
{
	struct sp_spinand_info *info = get_spinand_info();
	struct sp_spinand_regs *regs = info->regs;
	u32 *softpad_ctrl_reg = (u32 *)0x9C0032D4;
	u32 origin_value = readl(softpad_ctrl_reg);
	u32 default_value, value;
	u32 begin=0, num=0, _begin=0, _num=0, sum=0;
	u32 i, j, best_i, best_j = 0;
	u32 polarity;
	u32 origin_rts = readl(&regs->spi_timing);
	u32 curr_rts = 0;
	u8 result[4][33];

	printk("%s, start. softpad:0x%08x, timing:0x%08x\n", __FUNCTION__, readl(softpad_ctrl_reg), readl(&regs->spi_timing));

	memset(result, 0, sizeof(result));
	default_value = origin_value;
	default_value &= ~((0x3f<<2) | (1<<15) | (0xf<<21));
	default_value |= clk_bypass ? 0x00 : (0x04<<2);         /* Enable softpad function for CLK pin (g_MX[78]) */
	default_value |= dq_bypass ? (1<<15) : (0x3b<<2);       /* Enable softpad function for other pins */

	/* for version A IC, dq bypass means the softpad is disabled.
	 * so only rts can be scanned.
	 */
	if (dq_bypass || rts_only) {
		value = default_value;
		writel(value, softpad_ctrl_reg);
		while(readl(softpad_ctrl_reg) != value);
		for (i=0, sum=0; i<sizeof(result)/sizeof(result[0]); i++) {
			curr_rts = i+1;
			writel(SPINAND_READ_TIMING(curr_rts), &regs->spi_timing);
			if (sp_spinand_test_speed() < 0) {
				result[i][0] = 'x';
			} else {
				result[i][0] = 'o';
				sum++;
				if (!begin)
					begin = i;
			}
		}
		printk("rts | result \n");
		for (i=0; i<sizeof(result)/sizeof(result[0]); i++)
			printk("  %d |  %c\n", i+1, result[i][0]);

		if (sum) {
			curr_rts = begin + 1 + (sum-1)/2;
			if (rts)
				*rts = curr_rts;
			if (softpad)
				*softpad = value;
		}
		goto func_out;
	}

	for (i=0; i<sizeof(result)/sizeof(result[0]); i++)
	{
		curr_rts = i + 1;
		value = (origin_rts & 0xFFFFFFF1) | SPINAND_READ_TIMING(curr_rts);
		writel(value, &regs->spi_timing);

		for (j=0,result[i][32]=0; j<32; j++) {
			polarity = (j<16) ? 1 : 0;
			value = (polarity<<18) | ((j&0x0f)<<21) | default_value;

			writel(value, softpad_ctrl_reg);
			while(readl(softpad_ctrl_reg) != value);
			printk("<rts:%d, polarity:%d, delay:%d, value:0x%08x>\n",
				curr_rts, polarity, j&15, value);
			if (sp_spinand_test_speed() < 0) {

				result[i][j] = 'x';
			} else {
				result[i][32]++;
				result[i][j] = 'o';
				sum++;
			}
		}
	}
	/* print the result */
	printk("<%s>\n", info->dev_name);
	printk("      =======================Timing Test Result========================\n");
	printk("     |-----------polarity=1-----------|---------polarity=0-------------\n");
	printk("     |0 1 2 3 4 5 6 7 8 9 a b c d e f | 0 1 2 3 4 5 6 7 8 9 a b c d e f");
	for (i=0; i<sizeof(result)/sizeof(result[0]); i++) {
		printk("\nrts=%d|",i+1);
		for(j=0; j<32; j++) {
			printk("%c ", result[i][j]);
			if (j == 15)
				printk("| ");
		}
	}
	printk("\n");

	/* find best_i */
	for (i=1, best_i=0; i<sizeof(result)/sizeof(result[0]); i++) {
		if (result[i][32] > result[best_i][32])
			best_i = i;
	}
	/* find best_j */
	for (j=0; j<32; j++) {
		if (result[best_i][j] == 'o') {
			if (_num == 0)
				_begin = j;
			_num++;
		} else {
			if(_num > num) {
				begin = _begin;
				num = _num;
			}
			_num = 0;
		}
	}
	if (_num > num) {
		begin = _begin;
		num = _num;
	}

	if (num) {
		curr_rts = best_i+1;
		printk("advised rts = %d\n", curr_rts);
		if (rts)
			*rts = curr_rts;

		best_j = begin + num/2;
		polarity = best_j<16 ? 1 : 0;
		value = (polarity<<18) | ((best_j&0x0f)<<21) | origin_value;

		writel(value, softpad_ctrl_reg);
		printk("advised softpad ctrl parameter = 0x%08x\n", value);
		if (softpad)
			*softpad = value;
	}

func_out:
	if (sum==0 || !set_now) {
		//writel(SPINAND_READ_TIMING(origin_rts), &regs->spi_timing);
		writel(origin_rts, &regs->spi_timing);
		writel(origin_value, softpad_ctrl_reg);
	}

	printk("%s, end. softpad:0x%08x, timing:0x%08x\n", __FUNCTION__, readl(softpad_ctrl_reg), readl(&regs->spi_timing));
	return sum;
}

#ifdef CONFIG_SPINAND_MEASURE_TIMIMNG
static int sp_spinand_test_rw2(u32 addr, u32 size, u32 seed, u8 action)
{
	struct sp_spinand_info *info = get_spinand_info();
	struct mtd_info *mtd = info->mtd;
	nand_erase_options_t erase_opts;
	u32 block_size = mtd->erasesize;
	u32 block_num = (size + block_size - 1) / block_size;
	u32 start_block = addr / block_size;
	size_t actual_size;
	u32 now_seed;
	u32 i, j;
	int ret;
	u8 *buf;

	buf = (u8 *)malloc(block_size);
	if (!buf) {
		printk("no memory!\n");
		goto err_out;
	}

	printk("start block:%d, block_num:%d, seed:0x%x\n", start_block, block_num, seed);

	switch (action)
	{
		case 0: 
			/* Erase blocks */
			for (i=0,j=start_block; i<block_num; i++,j++) {
				while (nand_block_isbad(mtd, (loff_t)j*block_size))
					j++;
			
				printk("\rerase block:%d\n", j);
			
				memset(&erase_opts, 0, sizeof(erase_opts));
				erase_opts.quiet = 1;
				erase_opts.offset = (loff_t)j * block_size;
				erase_opts.length = block_size;
				ret = nand_erase_opts(mtd, &erase_opts);
				if (ret) {
					printk("\rerase blocks => fail\n");
					ret = CMD_RET_FAILURE;
					goto err_out;
				}
			}
			printk("\rerase blocks => success\n");
			break;

		case 1:
			/* Write blocks */
			now_seed = seed;
			for (i=0,j=start_block; i<block_num; i++,j++) {
				/* skip bad block */
				while (nand_block_isbad(mtd, (loff_t)j*block_size))
					j++;
			
				printk("\rwrite block:%d\n",j);
			
				/* prepare the pattern */
				now_seed += (i * block_size) / 4;
				pattern_generate(now_seed, buf, block_size);
			
				/* write block */
				actual_size = block_size;
				ret = nand_write(mtd, (loff_t)j*block_size, &actual_size, buf);
				if (ret) {
					printk("\rwrite blocks => fail\n");
					ret = CMD_RET_FAILURE;
					goto err_out;
				}
			}
			printk("\rwrite blocks => success\n");
			break;

		case 2:
			/* Read blocks */
			now_seed = seed;
			for (i=0,j=start_block; i<block_num; i++,j++) {
				now_seed += (i * block_size) / 4;
			
				/* skip bad block */
				while (nand_block_isbad(mtd, (loff_t)j*block_size))
					j++;
			
				printk("\rread block:%d\n",j);
			
				actual_size = block_size;
				ret = nand_read(mtd, (loff_t)j*block_size, &actual_size, buf);
				if (ret) {
					printk("\rread blocks => fail\n");
					ret = CMD_RET_FAILURE;
					goto err_out;
				}
			
				if (pattern_check(now_seed, buf, block_size) == -1) {
					printk("\rread blocks => data mis-match\n");
					goto err_out;
				}
			}
			printk("\rread blocks => success\n");
			break;

			default:
				ret = CMD_RET_USAGE;
		}

	err_out:
		if (buf)
			free(buf);

	return ret;
}

static int read_mod_write(u32 *addr, u32 mask, u32 val)
{
	u32 reg_val = 0;

	reg_val = readl(addr);
	reg_val = (reg_val & (~mask)) | val;
	writel(reg_val, addr); 

	return 0;
}

#define PAD_CTL2_REG		0xF8803300	// G102
#define SOFTPAD_REG_START	24
#define SOFTPAD_REG_END		25
static int sp_spinand_test_softpad(int argc, char * const argv[])
{
	u32 *pad_ctl2_reg = (u32 *)PAD_CTL2_REG;
	u32 i, val, *addr;
	u32 ret = 0;

	if (argc == 3) {
		/* Show all SPI-NAND Softpad Control registers' value. */
		for (i = SOFTPAD_REG_START; i <= SOFTPAD_REG_END; i++) {
			addr = &pad_ctl2_reg[i]; 
			val = pad_ctl2_reg[i];
			printk("SC #%d, addr: 0x%p, val: 0x%08x\n", i, addr, val);
		}
	} else if (argc == 4) {
		/* Set SPI-NAND Softpad Control registers for case. */
		u32 mask;

		val = simple_strtoul(argv[3], NULL, 10);

		switch(val)
		{
			case 1:
				printk("Softpad setting: Case 1\n");
				val  = (1<<31)| 	// G102.25[31] - SPI GPIO bypass
					   (0<<21)|	 	// G102.25[21] - SPI CLK output
					   (0<<20)| 	// G102.25[20] - SPI D3 output
					   (0<<19)| 	// G102.25[19] - SPI D2 output
					   (0<<18)| 	// G102.25[18] - SPI D1 output
					   (0<<17)| 	// G102.25[17] - SPI D0 output
					   (0<<5)|	 	// G102.25[5]  - SPI D3 input
					   (0<<4)|	 	// G102.25[4]  - SPI D2 input
					   (0<<3)|	 	// G102.25[3]  - SPI D1 input
					   (0<<2)|	 	// G102.25[2]  - SPI D0 input
					   (0<<0);	 	// G102.25[0]  - SPI CLK input
				mask = (1<<31)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|
					   (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<0);
				read_mod_write(&pad_ctl2_reg[25], mask, val);
				break;

			case 2:
				printk("Softpad setting: Case 2\n");
				val  = (0<<31)| 	// G102.25[31] - SPI GPIO bypass
					   (0<<21)| 	// G102.25[21] - SPI CLK output
					   (1<<20)| 	// G102.25[20] - SPI D3 output
					   (1<<19)| 	// G102.25[19] - SPI D2 output
					   (1<<18)| 	// G102.25[18] - SPI D1 output
					   (1<<17)| 	// G102.25[17] - SPI D0 output
					   (0<<5)|		// G102.25[5]  - SPI D3 input
					   (0<<4)|		// G102.25[4]  - SPI D2 input
					   (0<<3)|		// G102.25[3]  - SPI D1 input
					   (0<<2)|		// G102.25[2]  - SPI D0 input
					   (0<<0);		// G102.25[0]  - SPI CLK input
				mask = (1<<31)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|
					   (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<0);
				read_mod_write(&pad_ctl2_reg[25], mask, val);
				break;

			case 3:
				printk("Softpad setting: Case 3\n");
				val  = (1<<31)| 	// G102.25[31] - SPI GPIO bypass
					   (1<<21)| 	// G102.25[21] - SPI CLK output
					   (0<<20)| 	// G102.25[20] - SPI D3 output
					   (0<<19)| 	// G102.25[19] - SPI D2 output
					   (0<<18)| 	// G102.25[18] - SPI D1 output
					   (0<<17)| 	// G102.25[17] - SPI D0 output
					   (0<<5)|		// G102.25[5]  - SPI D3 input
					   (0<<4)|		// G102.25[4]  - SPI D2 input
					   (0<<3)|		// G102.25[3]  - SPI D1 input
					   (0<<2)|		// G102.25[2]  - SPI D0 input
					   (0<<0);		// G102.25[0]  - SPI CLK input
				mask = (1<<31)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|
					   (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<0);
				read_mod_write(&pad_ctl2_reg[25], mask, val);
				break;

			case 4:
				printk("Softpad setting: Case 4\n");
				val  = (0<<31)| 	// G102.25[31] - SPI GPIO bypass
					   (1<<21)| 	// G102.25[21] - SPI CLK output
					   (1<<20)| 	// G102.25[20] - SPI D3 output
					   (1<<19)| 	// G102.25[19] - SPI D2 output
					   (1<<18)| 	// G102.25[18] - SPI D1 output
					   (1<<17)| 	// G102.25[17] - SPI D0 output
					   (0<<5)|		// G102.25[5]  - SPI D3 input
					   (0<<4)|		// G102.25[4]  - SPI D2 input
					   (0<<3)|		// G102.25[3]  - SPI D1 input
					   (0<<2)|		// G102.25[2]  - SPI D0 input
					   (0<<0);		// G102.25[0]  - SPI CLK input
				mask = (1<<31)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|
					   (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<0);
				read_mod_write(&pad_ctl2_reg[25], mask, val);
				break;

			case 5:
				printk("Softpad setting: Case 5\n");
				val  = (1<<31)| 	// G102.25[31] - SPI GPIO bypass
					   (0<<21)| 	// G102.25[21] - SPI CLK output
					   (0<<20)| 	// G102.25[20] - SPI D3 output
					   (0<<19)| 	// G102.25[19] - SPI D2 output
					   (0<<18)| 	// G102.25[18] - SPI D1 output
					   (0<<17)| 	// G102.25[17] - SPI D0 output
					   (1<<5)|		// G102.25[5]  - SPI D3 input
					   (1<<4)|		// G102.25[4]  - SPI D2 input
					   (1<<3)|		// G102.25[3]  - SPI D1 input
					   (1<<2)|		// G102.25[2]  - SPI D0 input
					   (1<<0);		// G102.25[0]  - SPI CLK input
				mask = (1<<31)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|
					   (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<0);
				read_mod_write(&pad_ctl2_reg[25], mask, val);
				break;

			case 6:
				printk("Softpad setting: Case 6\n");
				val  = (1<<31)| 	// G102.25[31] - SPI GPIO bypass
					   (0<<21)| 	// G102.25[21] - SPI CLK output
					   (0<<20)| 	// G102.25[20] - SPI D3 output
					   (0<<19)| 	// G102.25[19] - SPI D2 output
					   (0<<18)| 	// G102.25[18] - SPI D1 output
					   (0<<17)| 	// G102.25[17] - SPI D0 output
					   (0<<5)|		// G102.25[5]  - SPI D3 input
					   (0<<4)|		// G102.25[4]  - SPI D2 input
					   (0<<3)|		// G102.25[3]  - SPI D1 input
					   (0<<2)|		// G102.25[2]  - SPI D0 input
					   (0<<0);		// G102.25[0]  - SPI CLK input
				mask = (1<<31)|(1<<21)|(1<<20)|(1<<19)|(1<<18)|(1<<17)|
					   (1<<5)|(1<<4)|(1<<3)|(1<<2)|(1<<0);
				read_mod_write(&pad_ctl2_reg[25], mask, val);
				break;

			default:
				ret = CMD_RET_USAGE;
		}
	}

	return ret;
}

static void set_pad_driving_strength(u32 *base, u32 pin, u32 strength)
{
	int reg_off = pin / 32;
	int bit_mask = 1 << (pin % 32);

	strength = (strength > 15) ? 15 : strength;

	/* Set Pad Driving Select0 registers, DS0 #0~3*/
	if (strength & 1)
		base[reg_off] |= bit_mask;
	else
		base[reg_off] &= ~bit_mask;

	/* Set Pad Driving Select1 registers, DS1 #0~3*/
	if (strength & 2)
		base[4+reg_off] |= bit_mask;
	else
		base[4+reg_off] &= ~bit_mask;

	/* Set Pad Driving Select2 registers, DS2 #0~3*/
	if (strength & 4)
		base[8+reg_off] |= bit_mask;
	else
		base[8+reg_off] &= ~bit_mask;

	/* Set Pad Driving Select3 registers, DS3 #0~3*/
	if (strength & 8)
		base[12+reg_off] |= bit_mask;
	else
		base[12+reg_off] &= ~bit_mask;
}

#define PAD_CTL_REG		0xF8803280	// G101
#define START_PIN_NUM	30			// SPI-NAND(X1) use 6 pins, G_MX30~35
static int sp_spinand_test_driving(int argc, char * const argv[])
{
	u32 *pad_ctl1_reg = (u32 *)PAD_CTL_REG;
	u32 i, val, *addr;
	u32 ret = 0;

	/* For pad driving strength, there are 4 selector, DS0~3.
	 * Each DS has 4 registers, DSx#0~4, to set for G_MXxx pins.
	 * The pin range of DSx#0 is G_MX00~31.
	 * The pin range of DSx#1 is G_MX32~63. 
	 * The pin range of DSx#2 is G_MX64~95.
	 * The pin range of DSx#3 is G_MX96~127.
	 */

	if (argc == 3) {
		/* Show all Pad Driving Strength registers' value, DS0~3. */
		for (i = 0; i < 16; i++) {
			addr = &pad_ctl1_reg[4+i]; 
			val = pad_ctl1_reg[4+i];
			printk("DS%d #%d, addr: 0x%p, val: 0x%08x\n", i/4, i%4, addr, val);
		}
	} else if (argc == 4) {
		/* Set Pad Driving Strength registers. */
		addr = &pad_ctl1_reg[4]; 
		val = simple_strtoul(argv[3], NULL, 10);

		if (val > 15) {
			printk("Warning! The DS value exceeds 15. (val: %d)\n", val);
			val = 15;
		}

		printk("reg base: 0x%p, val: %d\n", addr, val);

		/* For eCV5546, SPI-NAND(X1) use 6 pins, G_MX30~35. */
		for (i = 0; i < 6; i++) {
			set_pad_driving_strength(addr, START_PIN_NUM+i, val);
		}
	} else {
		ret = CMD_RET_USAGE;
	}

	return ret;
}

static int sp_spinand_test_clkdly(int argc, char * const argv[])
{
	u32 *pad_ctl2_reg = (u32 *)0xF8003300;
	u32 val, *addr;
	u32 ret = 0;

	if (argc == 3) {
		/* Show CLK softpad setting. */
			addr = &pad_ctl2_reg[0];
			val = pad_ctl2_reg[0];
			printk("SC #%d, addr: 0x%p, val: 0x%08x\n", 0, addr, val);
			printk("CLK softpad status, enable: %d,  delay: %d\n", ((val&0x00000001)>>0), ((val&0x0000001E)>>1));
	} else if (argc == 4) {
		/* Set CLK Softpad delay. */
		u32 mask;

		val = simple_strtoul(argv[3], NULL, 10);
		if (val > 15){
			printk("Warning! The softpad delay value exceeds 15. (val: %d)\n", val);
			val = 15;
		}

		printk("CLK softpad delay: %d\n", val);

		val = (val<<1);	// G102.0[4:1] - CLK softpad delay
		val |= (0<<0);	// G102.0[0]   - Select EMMC Clock output polarity
		val |= (1<<20);	// G102.0[20]  - EMMC PAD test mode enable
		mask = (0x1<<20)|(0xF<<1)|(0x1<<0);
		read_mod_write(&pad_ctl2_reg[0], mask, val);
	}

	return ret;
}

#define SPINAND_TEST_ADDR	0x500000
#define SPINAND_TEST_SIZE	0x20000
#define SPINAND_TEST_SEED	0x3c3c3c3c
static int sp_spinand_test2(int argc, char * const argv[])
{
	char *cmd = argv[2];
	int ret = 0;

	if (strncmp(cmd, "erase", 5) == 0) {
		ret = sp_spinand_test_rw2(SPINAND_TEST_ADDR, SPINAND_TEST_SIZE, SPINAND_TEST_SEED, 0);
	} else if (strncmp(cmd, "write", 5) == 0) {
		ret = sp_spinand_test_rw2(SPINAND_TEST_ADDR, SPINAND_TEST_SIZE, SPINAND_TEST_SEED, 1);
	} else if (strncmp(cmd, "read", 4) == 0) {
		ret = sp_spinand_test_rw2(SPINAND_TEST_ADDR, SPINAND_TEST_SIZE, SPINAND_TEST_SEED, 2);
	} else if (strncmp(cmd, "driving", 7) == 0) {
		ret = sp_spinand_test_driving(argc, argv);
	} else if (strncmp(cmd, "softpad", 7) == 0) {
		ret = sp_spinand_test_softpad(argc, argv);
	} else if (strncmp(cmd, "clkdly", 6) == 0) {
		ret = sp_spinand_test_clkdly(argc, argv);
	} else {
		ret = CMD_RET_USAGE;
	}

	return ret;
}
#endif

__attribute__ ((unused))
static int sp_spinand_test(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	char *cmd = argv[1];
	int ret = 0;
	struct sp_spinand_info *info = get_spinand_info();
	struct sp_spinand_regs *regs = info->regs;

	if (argc < 2)
		return CMD_RET_USAGE;

	if (strncmp(cmd, "reset", 5) == 0) {
		spi_nand_reset(info);
	} else if (strncmp(cmd, "regs", 4) == 0) {
		dump_spi_regs(info);
	} else if (strncmp(cmd, "feature", 7) == 0) {
		u32 addr, value;
		if (argc == 2) {
			printk("feature(A0) = 0x%02x\n",
				spi_nand_getfeatures(info, 0xA0));
			printk("feature(B0) = 0x%02x\n",
				spi_nand_getfeatures(info, 0xB0));
			printk("feature(C0) = 0x%02x\n",
				spi_nand_getfeatures(info, 0xC0));
		} else {
			addr = simple_strtoul(argv[2], NULL, 16);
			if(addr!=0xA0 && addr!=0xB0 && addr!=0xC0 && addr!=0xD0){
				printk("invalid addr(0x%02x)!\n",addr);
				ret = CMD_RET_USAGE;
			} else {
				if(argc == 4) {
					value = simple_strtoul(argv[3],NULL,16);
					value &= 0xff;
					spi_nand_setfeatures(info, addr, value);
					printk("set feature(%02X) to 0x%02x\n",
						addr, value);
				} else {
					printk("feature(%02X) = 0x%02x\n", addr,
						spi_nand_getfeatures(info, addr));
				}
			}
		}
	} else if (strncmp(cmd, "id", 2) == 0) {
		u32 devid = 0;
		spi_nand_readid(info, 0, (u8*)&devid);
		printk("device id: 0x%08x\n", devid);
	} else if (strncmp(cmd, "rw", 2) == 0) {
		u32 addr = 0;
		u32 size = 0;
		u32 seed = 0;
		if(argc == 5) {
			addr = simple_strtoul(argv[2],NULL,16);
			size = simple_strtoul(argv[3],NULL,16);
			seed = simple_strtoul(argv[4],NULL,16);
			sp_spinand_test_rw(addr, size, seed);
		} else {
			ret = CMD_RET_USAGE;
		}
	} else if (strncmp(cmd, "speed", 5) == 0) {
		sp_spinand_test_speed();
	} else if (strncmp(cmd, "stress", 6) == 0) {
		u32 block = -1;
		if(argc >= 3)
			block = simple_strtol(argv[2], NULL, 10);
		if(block == -1)
			sp_spinand_test_stress();
		else
			sp_spinand_test_stress1(block);
	} else if (strncmp(cmd, "timing", 6) == 0) {
		u32 clk_bypass = 0, dq_bypass = 0;
		u32 i;
		for (i=2; i<argc; i++) {
			if (strncmp(argv[i], "clk", 3) == 0)
				clk_bypass = 1;
			else if (strncmp(argv[i], "dq", 2) == 0)
				dq_bypass = 1;
		}
		sp_spinand_test_timing(clk_bypass, dq_bypass, 0, 0, NULL, NULL);
	} else if (strncmp(cmd, "trsmode", 7) == 0) {
		if (argc >= 3) {
			u32 trsmode = simple_strtoul(argv[2], NULL, 10);
			if (trsmode >= SPINAND_TRS_MAX) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				info->trs_mode = trsmode;
				printk("set trs_mode => %d\n", trsmode);
			}
		} else {
			printk("trs_mode = %d\n", info->trs_mode);
		}
	} else if (strncmp(cmd, "rawtrsmode", 10) == 0) {
		if (argc >= 3) {
			u32 rawtrsmode = simple_strtoul(argv[2], NULL, 10);
			if (rawtrsmode >= SPINAND_TRS_MAX) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				info->raw_trs_mode = rawtrsmode;
				printk("set raw_trs_mode => %d\n", rawtrsmode);
			}
		} else {
			printk("raw_trs_mode = %d\n", info->raw_trs_mode);
		}
	}
	else if (strncmp(cmd, "wio", 3) == 0) {
		if (argc >= 3) {
			u32 bitmode = simple_strtoul(argv[2], NULL, 10);
			if (bitmode != SPINAND_1BIT_MODE
				&& bitmode != SPINAND_4BIT_MODE) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				info->write_bitmode = bitmode;
				printk("set write_bitmode => %d\n", bitmode);
			}
		} else {
			printk("write_bitmode = %d\n", info->write_bitmode );
		}
	} else if (strncmp(cmd, "rio", 3) == 0) {
		if (argc >= 3) {
			u32 bitmode = simple_strtoul(argv[2], NULL, 10);
			if(bitmode > SPINAND_4BIT_MODE) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				info->read_bitmode = bitmode;
				printk("set read_bitmode => %d\n", bitmode);
			}
		} else {
			printk("read_bitmode = %d\n", info->read_bitmode);
		}
	} else if (strncmp(cmd, "clksel", 6) == 0) {
		if (argc >= 3) {
			u32 clksel = simple_strtoul(argv[2], NULL, 10);
			if (clksel < SPINAND_CLKSRC_MIN && clksel > SPINAND_CLKSRC_MAX) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				SPINAND_SET_CLKSRC(clksel);
				printk("set spi_clk_sel => %d\n", clksel);
			}
		} else {
			printk("spi_clk_sel = %d\n", SPINAND_GET_CLKSRC());
		}
	} else if (strncmp(cmd, "clkdiv", 6) == 0) {
		if (argc >= 3) {
			u32 clkdiv = simple_strtoul(argv[2], NULL, 10);
			if (clkdiv < 1 || clkdiv > 7) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				info->spi_clk_div = clkdiv;
				printk("set spi_clk_div => %d\n", clkdiv);
			}
		} else {
			printk("spi_clk_div = %d\n", info->spi_clk_div);
		}
	} else if (strncmp(cmd, "rts", 3) == 0) {
		u32 rts;
		if(argc >= 3) {
			rts = simple_strtoul(argv[2], NULL, 10);
			if(rts > 7) {
				printk("invalid value!\n");
				ret = CMD_RET_USAGE;
			} else {
				writel(SPINAND_READ_TIMING(rts), &regs->spi_timing);
				printk("set rts => %d\n", rts);
			}
		} else {
			rts = readl(&regs->spi_timing);
			printk("rts = %d\n",(rts>>1)&0x07);
		}
#ifdef CONFIG_SPINAND_MEASURE_TIMIMNG
	} else if (strncmp(cmd, "test", 4) == 0) {
		if (argc >= 3) {
			ret = sp_spinand_test2(argc, argv);
		} else {
			ret = CMD_RET_USAGE;
		}
#endif
	} else {
		ret = CMD_RET_USAGE;
	}

	return ret;
}

U_BOOT_CMD(ssnand, CONFIG_SYS_MAXARGS, 1, sp_spinand_test,
	"eys3d spi-nand tests",
	"reset - send reset cmd to device.\n"
	"ssnand regs - dump spi-nand registers.\n"
	"ssnand id - send ReadId cmd to device to achieve device id.\n"
	"ssnand feature [addr] [value] - get/set the device's feature values.\n"
	"\tif 'addr' is not set, it shows all feature values.\n"
	"\tif 'value' is not set, it shows the feature value of 'addr'.\n"
	"\tif 'value' is set, it sets 'value' to feature of 'addr'.\n"
	"ssnand rw [addr] [size] [seed] - write and read 'size' of sequenced data to 'addr'.\n"
	"\t then check if the data is the same, the 'seed' is the start code of the sequence.\n"
	"ssnand speed - test erase,write,read the speed.\n"
	"ssnand stress [block] - do stress test on 'block',\n"
	"\tif 'block' is -1 or not specified, all blocks would be tested.\n"
	"ssnand timing [clk|dq] - timing test,\n"
	"\tif 'clk' or 'dq' is specified, means the softpad function of clk or dq is disabled.\n"
	"ssnand trsmode [value] - set/show trs_mode, 0~3 are allowed.\n"
	"ssnand rawtrsmode [value] - set/show raw_trs_mode, 0~2 are allowed.\n"
	"ssnand wio [value] - set/show write_bitmode, 0/2 are allowed.\n"
	"ssnand rio [value] - set/show read_bitmode, 0/1/2 are allowed.\n"
	"ssnand clksel [value] - set/show spi_clk_sel, 0~7 are allowed.\n"
	"ssnand clkdiv [value] - set/show spi_clk_div, 1~7 are allowed.\n"
	"ssnand rts [value] - set/show rts(read timing select). 0~7 are allowed.\n"
#ifdef CONFIG_SPINAND_MEASURE_TIMIMNG
	"ssnand test - timing measurement command.\n"
	"\terase - erase a block.\n"
	"\twrite - write a block.\n"
	"\tread - read a block.\n"
	"\tdriving - set driving strength. The range is 0~15.\n"
	"\tsoftpad - set softpad. The range is 1~6.\n"
	"\tclkdly - set CLK softpad delay. The range is 0~15.\n"
#endif
);


