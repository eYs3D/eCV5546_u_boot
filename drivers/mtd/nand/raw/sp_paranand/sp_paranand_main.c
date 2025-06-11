#include <common.h>
#include <malloc.h>
#include <dm.h>
#include <dm/device_compat.h>
#include <asm/io.h>
#include <nand.h>
#include <rand.h> //rand()

#include <linux/mtd/mtd.h>
#include <linux/mtd/rawnand.h>
#include <linux/mtd/nand.h>
#include <linux/mtd/partitions.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <mach/gpio_drv.h>
#include <cpu_func.h>
#include <mapmem.h>
#include <clk.h>
#include <reset.h>

#include <linux/dma-mapping.h>
#include <linux/mtd/bbm.h>
#include <linux/bitops.h>
#include <command.h>

#include "sp_paranand.h"

static struct sp_pnand_info *our_paranfc = NULL;
extern const struct nand_flash_dev sp_pnand_ids[];

/* Note: The unit of tWPST/tRPST/tWPRE/tRPRE field of sp_pnand_chip_timing is ns.
 *
 * tWH, tCH, tCLH, tALH, tCALH, tWP, tREH, tCR, tRSTO, tREAID,
 * tREA, tRP, tWB, tRB, tWHR, tWHR2, tRHW, tRR, tAR, tRC
 * tADL, tRHZ, tCCS, tCS, tCS2, tCLS, tCLR, tALS, tCALS, tCAL2, tCRES, tCDQSS, tDBS, tCWAW, tWPRE,
 * tRPRE, tWPST, tRPST, tWPSTH, tRPSTH, tDQSHZ, tDQSCK, tCAD, tDSL
 * tDSH, tDQSL, tDQSH, tDQSD, tCKWR, tWRCK, tCK, tCALS2, tDQSRE, tWPRE2, tRPRE2, tCEH
 */
static struct sp_pnand_chip_timing chip_timing[] = {
	{ //SAMSUNG_K9F2G08U0A_ZEBU
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	20, 12, 100, 0, 60, 0, 100, 20, 10, 0,
	70, 100, 0, 20, 0, 12, 10, 12, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //GD9FS2G8F2A 256MiB 1.8V 8-bit
	10, 5, 5, 5, 0, 15, 10, 0, 0, 0,
	15, 12, 100, 0, 60, 0, 100, 20, 10, 25,
	300, 100, 0, 15, 0, 12, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //GD9AU4G8F3A 512MiB 3.3V 8-bit
	7, 5, 5, 5, 0, 12, 7, 0, 0, 0,
	18, 10, 100, 0, 80, 0, 100, 20, 10, 20,
	100, 100, 0, 15, 0, 12, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //GD9FU4G8F4B 512MiB 3.3V 8-bit
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	20, 12, 100, 0, 60, 0, 100, 20, 10, 0,
	150, 100, 0, 15, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //W29N08GZSIBA 1GMiB 1.8V 8-bit
	10, 5, 5, 5, 0, 12, 10, 0, 0, 0,
	25, 12, 100, 0, 80, 0, 100, 20, 10, 0,
	150, 100, 0, 15, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ //K9GBG08U0B 4GiB 3.3V 8-bit
	11, 5, 5, 5, 0, 11, 11, 0, 0, 0,
	20, 11, 100, 0, 120, 300, 100, 20, 10, 25,
	70, 100, 0, 20, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 0 10MHz
	40, 20, 20, 20, 0, 60, 50, 0, 0, 0,
	50, 50, 200, 0, 120, 0, 200, 40, 25, 100,
	200, 200, 0, 70, 0, 50, 20, 50, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 1 20MHz
	15, 10, 10, 10, 0, 30, 15, 0, 0, 0,
	30, 25, 100, 0, 80, 0, 100, 20, 10, 50,
	100, 100, 0, 35, 0, 25, 10, 25, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 2 28MHz
	15, 10, 10, 10, 0, 20, 15, 0, 0, 0,
	20, 17, 100, 0, 80, 0, 100, 20, 10, 35,
	100, 100, 0, 25, 0, 15, 10, 15, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 3 33MHz
	12, 5, 5, 5, 0, 17, 12, 0, 0, 0,
	17, 15, 100, 0, 60, 0, 100, 20, 10, 30,
	100, 100, 0, 25, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 4 40MHz
	10, 5, 5, 5, 0, 15, 10, 0, 0, 0,
	15, 12, 100, 0, 60, 0, 100, 20, 10, 25,
	70, 100, 0, 20, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 5 50MHz
	7, 5, 5, 5, 0, 12, 7, 0, 0, 0,
	12, 12, 100, 0, 60, 0, 100, 20, 10, 20,
	70, 100, 0, 15, 0, 10, 10, 10, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

#if 1
/* Note: The unit of tWPST/tRPST/tWPRE/tRPRE field of sp_pnand_chip_timing is ns.
 *
 * tWH, tCH, tCLH, tALH, tCALH, tWP, tREH, tCR, tRSTO, tREAID,
 * tREA, tRP, tWB(max), tRB, tWHR, tWHR2, tRHW, tRR, tAR, tRC
 * tADL, tRHZ, tCCS(max), tCS, tCS2, tCLS, tCLR, tALS, tCALS, tCAL2, tCRES, tCDQSS, tDBS, tCWAW, tWPRE,
 * tRPRE, tWPST, tRPST, tWPSTH, tRPSTH, tDQSHZ(max), tDQSCK(max), tCAD, tDSL
 * tDSH, tDQSL, tDQSH, tDQSD(max), tCKWR, tWRCK, tCK, tCALS2, tDQSRE, tWPRE2, tRPRE2, tCEH
 */
static struct sp_pnand_chip_timing sync_timing[] = {
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 0 20MHz
	0, 10, 0, 0, 10, 0, 0, 0, 0, 0,
	0, 0, 100, 0, 80, 0, 100, 20, 0, 0,
	100, 0, 200, 35, 0, 0, 0, 0, 10, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 20, 20, 25, 0,
	0, 0, 0, 18, 2, 20, 50, 0, 0, 0, 0, 0},
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 1 33MHz
	0, 5, 0, 0, 5, 0, 0, 0, 0, 0,
	0, 0, 100, 0, 80, 0, 100, 20, 0, 0,
	100, 0, 200, 25, 0, 0, 0, 0, 5, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 20, 20, 25, 0,
	0, 0, 0, 18, 2, 20, 30, 0, 0, 0, 0, 0},
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 2 50MHz
	0, 4, 0, 0, 4, 0, 0, 0, 0, 0,
	0, 0, 100, 0, 80, 0, 100, 20, 0, 0,
	70, 0, 200, 15, 0, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 20, 20, 25, 0,
	0, 0, 0, 18, 2, 20, 20, 0, 0, 0, 0, 0},
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 3 67MHz
	0, 3, 0, 0, 3, 0, 0, 0, 0, 0,
	0, 0, 100, 0, 80, 0, 100, 20, 0, 0,
	70, 0, 200, 15, 0, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 20, 20, 25, 0,
	0, 0, 0, 18, 3, 20, 15, 0, 0, 0, 0, 0},
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 4 83MHz
	0, 2.5, 0, 0, 2.5, 0, 0, 0, 0, 0,
	0, 0, 100, 0, 80, 0, 100, 20, 0, 0,
	70, 0, 200, 15, 0, 0, 0, 0, 2.5, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 20, 20, 25, 0,
	0, 0, 0, 18, 3, 20, 12, 0, 0, 0, 0, 0},
	{ //MT29F32G08ABXXX 4GiB 8-bit mode 5 100MHz
	0, 2, 0, 0, 2, 0, 0, 0, 0, 0,
	0, 0, 100, 0, 80, 0, 100, 20, 0, 0,
	70, 0, 200, 15, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 20, 20, 25, 0,
	0, 0, 0, 18, 3, 20, 10, 0, 0, 0, 0, 0}
};
#endif

struct sp_pnand_info *get_pnand_info(void)
{
	return our_paranfc;
}

static int sp_pnand_ooblayout_ecc(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand = mtd_to_nand(mtd);

	if (section >= nand->ecc.steps)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = 1;

	return 0;
}

static int sp_pnand_ooblayout_free(struct mtd_info *mtd, int section,
				struct mtd_oob_region *oobregion)
{
	struct nand_chip *nand = mtd_to_nand(mtd);

	if (section >= nand->ecc.steps)
		return -ERANGE;

	oobregion->offset = 0;
	oobregion->length = mtd->oobsize;

	return 0;
}

static const struct mtd_ooblayout_ops sp_pnand_ooblayout_ops = {
	.ecc = sp_pnand_ooblayout_ecc,
	.rfree = sp_pnand_ooblayout_free,
};

static uint8_t sp_pnand_bbt_pattern[] = { 'B', 'b', 't', '0' };
static uint8_t sp_pnand_mirror_pattern[] = { '1', 't', 'b', 'B' };

static struct nand_bbt_descr sp_pnand_bbt_mirror_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
	    | NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = sp_pnand_mirror_pattern
};

static struct nand_bbt_descr sp_pnand_bbt_main_descr = {
	.options = NAND_BBT_LASTBLOCK | NAND_BBT_CREATE | NAND_BBT_WRITE
	    | NAND_BBT_2BIT | NAND_BBT_VERSION | NAND_BBT_PERCHIP,
	.offs = 0,
	.len = 4,
	.veroffs = 4,
	.maxblocks = 4,
	.pattern = sp_pnand_bbt_pattern
};

static uint8_t sp_pnand_scan_ff_pattern[] = { 0xff, 0xff, 0xff, 0xff };

static struct nand_bbt_descr sp_pnand_largepage_flashbased = {
	.offs = 0,
	.len = 4,
	.pattern = sp_pnand_scan_ff_pattern
};

static void sp_pnand_set_warmup_cycle(struct nand_chip *nand,
			u8 wr_cyc, u8 rd_cyc)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	int val;

	val = readl(info->io_base + MEM_ATTR_SET2);
	val &= ~(0xFF);
	val |= (((rd_cyc & 0x3) << 4) | (wr_cyc & 0x3));
	writel(val, info->io_base + MEM_ATTR_SET2);
}

static struct sp_pnand_chip_timing *sp_pnand_scan_timing(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);

	if(strcmp(info->dev_name, "K9F2G08XXX 256MiB ZEBU 8-bit") == 0) {
		return &chip_timing[0];
	} else if(strcmp(info->dev_name, "GD9FS2G8F2A 256MiB 1.8V 8-bit") == 0) {
		return &chip_timing[1];
	} else if(strcmp(info->dev_name, "GD9AU4G8F3A 512MiB 3.3V 8-bit") == 0) {
		return &chip_timing[2];
	} else if(strcmp(info->dev_name, "GD9FU4G8F4B 512MiB 3.3V 8-bit") == 0) {
		return &chip_timing[3];
	} else if(strcmp(info->dev_name, "W29N08GZSIBA 1GiB 1.8V 8-bit") == 0) {
		return &chip_timing[4];
	} else if(strcmp(info->dev_name, "K9GBG08U0B 4GiB 3.3V 8-bit") == 0) {
		return &chip_timing[5];
	} else if(strcmp(info->dev_name, "MT29F32G08ABXXX 4GiB 8-bit") == 0) {
		if (info->flash_type == ONFI2)
			return &sync_timing[info->timing_mode];
		else
			return &chip_timing[6 + info->timing_mode];
	} else {
		DBGLEVEL1(sp_pnand_dbg("Failed to get AC timing!\n"));
		return NULL;
	}
}

/* The unit of Hclk is MHz, and the unit of Time is ns.
 * We desire to calculate N to satisfy N*(1/Hclk) > Time given Hclk and Time
 * ==> N > Time * Hclk
 * ==> N > Time * 10e(-9) * Hclk *10e(6)        --> take the order out
 * ==> N > Time * Hclk * 10e(-3)
 * ==> N > Time * Hclk / 1000
 * ==> N = (Time * Hclk + 999) / 1000
 */
static void sp_pnand_calc_timing(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct sp_pnand_chip_timing *p;
	int tWH, tWP, tREH, tRES, tBSY, tBUF1;
	int tBUF2, tBUF3, tBUF4, tPRE, tRLAT, t1;
	int tPST, tPSTH, tWRCK;
	int i;
	u32 CLK, FtCK, timing[4];

	CLK = info->clkfreq / 1000000;

	tWH = tWP = tREH = tRES =  0;
	tRLAT = tBSY = t1 = 0;
	tBUF4 = tBUF3 = tBUF2 = tBUF1 = 0;
	tPRE = tPST = tPSTH = tWRCK = 0;

	p = sp_pnand_scan_timing(nand);
	if(!p)
		DBGLEVEL1(sp_pnand_dbg("Failed to get AC timing!\n"));

	if(info->flash_type == LEGACY_FLASH) {
		// tWH = max(tWH, tCH, tCLH, tALH)
		tWH = max_4(p->tWH, p->tCH, (int)p->tCLH, (int)p->tALH);
		tWH = (tWH * CLK + 1000 - 1) / 1000;
		// tWP = tWP
		tWP = (p->tWP * CLK + 1000 - 1) / 1000;
		// tREH = tREH
		tREH = (p->tREH * CLK + 1000 - 1) / 1000;
		// tRES = max(tREA, tRSTO, tREAID)
		tRES = max_3(p->tREA, p->tRSTO, (int)p->tREAID);
		tRES = (tRES * CLK + 1000 - 1) / 1000;
		// tRLAT < (tRES + tREH) + 2
		//tRLAT = tRES + tREH;////////////////////////////////////////////////
		tRLAT= 3;//0;
		// t1 = max(tCS, tCLS, tALS) - tWP
		t1 = max_3(p->tCS, p->tCLS, (int)p->tALS) - p->tWP;
		if (t1 < 0)
			t1 = 0;
		else
			t1 = (t1 * CLK) / 1000;
		// tPSTH(EBI setup time) = max(tCS, tCLS, tALS)
		tPSTH = max_3(p->tCS, p->tCLS, (int)p->tALS);
		tPSTH = (tPSTH * CLK) / 1000;
		// tWRCK(EBI hold time) = max(tRHZ, tREH)
		tWRCK = max_2(p->tRHZ, p->tREH);
		tWRCK = (tWRCK * CLK) / 1000;
	}

	else if(info->flash_type == ONFI2) {
		tRLAT= 3;
		if (info->clkfreq == 400000000) {//unit = 2.5ns
			if(info->timing_mode == 0)// mode 0 20MHz ---> 50ns
				tRES = 10;
			else if(info->timing_mode == 1)// mode 1 33MHz ---> 30ns
				tRES = 6;
			else if(info->timing_mode == 2)// mode 2 50MHz ---> 20ns
				tRES = 4;
			else if(info->timing_mode == 3)// mode 3 67MHz ---> 15ns
				tRES = 3;
			else if(info->timing_mode == 4)// mode 4 83MHz ---> 12ns
				tRES = 3; // 2*2.5 < 6 < 3*2.5
			else if(info->timing_mode == 5)// mode 5 100MHz ---> 10ns
				tRES = 2;
		}
		#if 1
		// tWP = tCAD
		tWP = (p->tCAD * CLK) / 1000;

		// Fill this field with value N, FTck = mem_clk/2(N + 1)
		// Note:mem_clk is same as core_clk. Here, we'd like to
		// assign 30MHz to FTck.
		//tRES = 0;
		//FtCK = CLK / ( 2 * (tRES + 1));
		FtCK = CLK / (2 * tRES);

		// Increase p->tCK by one, is for the fraction which
		// cannot store in the variable, Integer type.
		p->tCK = 1000 / FtCK + 1;
		#if 0
		// The vfp calculation isn't supported in kernel code.
		p->tWPRE = 1.5 * p->tCK;
		p->tWPST = 1.5 * p->tCK;
		p->tDQSL = 0.6 * p->tCK;
		p->tDQSH = 0.6 * p->tCK;
		#else
		p->tWPRE = 2 * p->tCK;
		p->tWPST = 2 * p->tCK;
		p->tDQSL = 1 * p->tCK;
		p->tDQSH = 1 * p->tCK;
		#endif

		p->tCKWR = (p->tDQSCK + p->tCK) / p->tCK;
		if(p->tDQSCK % p->tCK !=0)
			p->tCKWR += 1;

		t1 = (p->tCS * CLK) / 1000;

		tPRE = 2;	// Assign 2 due to p->tWPRE is 1.5*p->tCK
		tPST = 2;	// Assign 2 due to p->tWPST is 1.5*p->tCK
		tPSTH = ((p->tDQSHZ * FtCK) / 1000) + 1;
		tWRCK = (p->tWRCK * CLK) /1000;
		#endif
	}

	// tBSY = max(tWB, tRB), min value = 1
	tBSY = max_2(p->tWB, p->tRB);
	tBSY = (tBSY * CLK) / 1000;
	if(tBSY < 1)
		tBSY = 1;
	// tBUF1 = max(tADL, tCCS)
	tBUF1 = max_2(p->tADL, p->tCCS);
	tBUF1 = (tBUF1 * CLK) / 1000;
	// tBUF2 = max(tAR, tRR, tCLR, tCDQSS, tCRES, tCALS, tCALS2, tDBS)
	tBUF2 = max_2(max_4(p->tAR, p->tRR, (int)p->tCLR, (int)p->tCDQSS),
			max_4((int)p->tCRES, (int)p->tCALS, (int)p->tCALS2, (int)p->tDBS));
	tBUF2 = (tBUF2 * CLK) / 1000;
	// tBUF3 = max(tRHW, tRHZ, tDQSHZ)
	tBUF3 = max_3(p->tRHW, p->tRHZ, (int)p->tDQSHZ);
	tBUF3 = (tBUF3 * CLK) / 1000;
	// tBUF4 = max(tWHR, tWHR2)
	tBUF4 = max_2((int)p->tWHR, p->tWHR2);
	if(info->flash_type == ONFI3)
		tBUF4 = max_2(tBUF4, p->tCCS);
	tBUF4 = (tBUF4 * CLK) / 1000;

#if 1	//xt:TEST
	tWH -= 1;
	tWP -= 1;
	tREH -= 1;
	if (tRES != 0)
		tRES -= 1;
	t1 -= 1;
	tPRE -= 1;
	tPST -= 1;
	tPSTH -= 1;
	tWRCK -= 1;
#endif
	timing[0] = (tWH << 24) | (tWP << 16) | (tREH << 8) | tRES;
	timing[1] = (tRLAT << 16) | (tBSY << 8) | t1;
	timing[2] = (tBUF4 << 24) | (tBUF3 << 16) | (tBUF2 << 8) | tBUF1;
	timing[3] = (tPRE << 28) | (tPST << 24) | (tPSTH << 16) | tWRCK;

	if(info->timing_mode == 5) {
		DBGLEVEL1(sp_pnand_dbg("kAC Timing 0:0x%08x\n", timing[0]));
		DBGLEVEL1(sp_pnand_dbg("kAC Timing 1:0x%08x\n", timing[1]));
		DBGLEVEL1(sp_pnand_dbg("kAC Timing 2:0x%08x\n", timing[2]));
		DBGLEVEL1(sp_pnand_dbg("kAC Timing 3:0x%08x\n", timing[3]));
		timing[0] = 0xf1f0f01;
		timing[1] = 0x400a;
		timing[2] = 0x20284050;
		timing[3] = 0x44040007;
	}

	for (i = 0;i < MAX_CHANNEL;i++) {
		writel(timing[0], info->io_base + FL_AC_TIMING0(i));
		writel(timing[1], info->io_base + FL_AC_TIMING1(i));
		writel(timing[2], info->io_base + FL_AC_TIMING2(i));
		writel(timing[3], info->io_base + FL_AC_TIMING3(i));

		/* A380: Illegal data latch occur at setting "rlat" field
		 * of ac timing register from 0 to 1.
		 * read command failed on A380 Linux
		 * Workaround: Set Software Reset(0x184) after
		 * "Trlat" field of AC Timing Register changing.
		 * Fixed in IP version 2.2.0
		 */
		if (tRLAT) {
			if (readl(info->io_base + REVISION_NUM) < 0x020200) {
				writel((1 << i), info->io_base + NANDC_SW_RESET);
				// Wait for the NANDC024 reset is complete
				while(readl(info->io_base + NANDC_SW_RESET) & (1 << i)) ;
			}
		}
	}

	DBGLEVEL1(sp_pnand_dbg("AC Timing 0:0x%08x\n", timing[0]));
	DBGLEVEL1(sp_pnand_dbg("AC Timing 1:0x%08x\n", timing[1]));
	DBGLEVEL1(sp_pnand_dbg("AC Timing 2:0x%08x\n", timing[2]));
	DBGLEVEL1(sp_pnand_dbg("AC Timing 3:0x%08x\n", timing[3]));
	/* t(ns) = reg * 1/coreclk * 1000000000//--- 200MHz t=reg*5; 400MHz t=reg*2.5 */
	//int r_cycle = ((tREH + 1) + (tRES + 1)) * 1000 / CLK;
	//int w_cycle = ((tWH + 1) + (tWP + 1)) * 1000 / CLK;
	//DBGLEVEL2(sp_pnand_dbg("read cycle %d, write cycle %d(only valid for PIO)\n", r_cycle, w_cycle));
}

static void sp_pnand_onfi_set_feature(struct nand_chip *nand, int val, int type)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct cmd_feature cmd_f;

	/* val is sub-feature Parameter P1 (P2~P4 = 0)
	 * b[5:4] means Data interface: 0x0(SDR); 0x1(NV-DDR); 0x2(NV-DDR2)
	 * b[3:0] means Timing mode number
	 */
	writel(val, info->io_base + SPARE_SRAM + (info->cur_chan << info->spare_ch_offset));

	/* 0x1 is Timing mode feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0x1;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(type) |\
			CMD_START_CE(info->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(4) |\
			CMD_INDEX(ONFI_FIXFLOW_SETFEATURE);

	sp_pnand_issue_cmd(nand, &cmd_f);

	sp_pnand_wait(mtd, nand);

}

static u32 sp_pnand_onfi_get_feature(struct nand_chip *nand, int type)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct cmd_feature cmd_f;
	u32 val;

	//writel(0xffff, info->io_base + SPARE_SRAM + (info->cur_chan << info->spare_ch_offset));

	/* 0x1 is Timing mode feature address */
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0x1;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(type) |\
			CMD_START_CE(info->sel_chip) | CMD_BYTE_MODE | CMD_SPARE_NUM(4) |\
			CMD_INDEX(ONFI_FIXFLOW_GETFEATURE);

	sp_pnand_issue_cmd(nand, &cmd_f);

	sp_pnand_wait(mtd, nand);

	val = readl(info->io_base + SPARE_SRAM + (info->cur_chan << info->spare_ch_offset));

	return val;
}

static int sp_pnand_onfi_sync(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	//struct sp_pnand_chip_timing *p;
	u32 val;
	int ret = -1;

	sp_pnand_select_chip(mtd, 0);
	val = sp_pnand_onfi_get_feature(nand, LEGACY_FLASH);
	printk("SDR feature for Ch %d, CE %d: 0x%x\n", info->cur_chan, info->sel_chip, val);

	//Check if the PNANDC support DDR interface
	val = readl(info->io_base + FEATURE_1);
	if ((val & DDR_IF_EN) == 0)
		return ret;


	sp_pnand_onfi_set_feature(nand, 0x10, LEGACY_FLASH);//mode1:0x11

	val = sp_pnand_onfi_get_feature(nand, ONFI2);
	printk("NV-DDR feature for Ch %d, CE %d: 0x%x\n", info->cur_chan, info->sel_chip, val);
	if (val != 0x1010) {
		return ret;
	}

	return 0;
}

static void sp_pnand_read_raw_id(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct cmd_feature cmd_f;
	u8 id_size = 5;

	info->cur_chan = 0;
	info->sel_chip = 0;

	// Set the flash to Legacy mode, in advance.
	if(info->flash_type == ONFI2 || info->flash_type == ONFI3) {
		sp_pnand_onfi_set_feature(nand, 0x00, LEGACY_FLASH);
	}

	// Issue the RESET cmd
	cmd_f.cq1 = 0;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = 0;
	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(LEGACY_FLASH) |\
			CMD_START_CE(info->sel_chip) | CMD_INDEX(FIXFLOW_RESET);

	sp_pnand_issue_cmd(nand, &cmd_f);

	sp_pnand_wait(mtd, nand);

	// Issue the READID cmd
	cmd_f.row_cycle = ROW_ADDR_1CYCLE;
	cmd_f.col_cycle = COL_ADDR_1CYCLE;
	cmd_f.cq1 = 0;
	cmd_f.cq2 = 0;
	cmd_f.cq3 = CMD_COUNT(1);
	cmd_f.cq4 = CMD_FLASH_TYPE(LEGACY_FLASH) | CMD_COMPLETE_EN |\
			CMD_INDEX(FIXFLOW_READID) | CMD_START_CE(info->sel_chip) |\
			CMD_BYTE_MODE | CMD_SPARE_NUM(id_size);

	sp_pnand_issue_cmd(nand, &cmd_f);

	sp_pnand_wait(mtd, nand);

	memcpy(info->flash_raw_id, info->io_base + SPARE_SRAM + (info->cur_chan << info->spare_ch_offset) , id_size);

	DBGLEVEL1(sp_pnand_dbg("ID@(ch:%d, ce:%d):0x%x, 0x%x, 0x%x, 0x%x, 0x%x\n",
					info->cur_chan, info->sel_chip, info->flash_raw_id[0],
					info->flash_raw_id[1], info->flash_raw_id[2],
					info->flash_raw_id[3], info->flash_raw_id[4]));
}

static void sp_pnand_calibrate_dqs_delay(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct cmd_feature cmd_f;
	int i, max_dqs_delay = 0;
	int id_size = 5;
	int id_size_ddr = (id_size << 1);
	u8 *p, *golden_p;
	u8 dqs_lower_bound, dqs_upper_bound, state;
	u32 val;

	dqs_lower_bound = dqs_upper_bound = 0;
	p = kmalloc(id_size_ddr, GFP_KERNEL);
	golden_p = kmalloc(id_size_ddr, GFP_KERNEL);

	if(info->flash_type == ONFI2 || info->flash_type == ONFI3) {
		/* Extent the data from SDR to DDR.
		   Ex. If "0xaa, 0xbb, 0xcc, 0xdd, 0xee" is in SDR,
		          "0xaa, 0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee" is in DDR(ONFI).
		*/
		for(i = 0; i< id_size; i++) {
			*(golden_p + (i << 1) + 0) = *(info->flash_raw_id + i);
			*(golden_p + (i << 1) + 1) = *(info->flash_raw_id + i);
		}
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
					*golden_p, *(golden_p+1), *(golden_p+2),
					*(golden_p+3), *(golden_p+4), *(golden_p+5)));
		max_dqs_delay = 20;
	}
	else if(info->flash_type == TOGGLE1 || info->flash_type == TOGGLE2) {
		/* Extent the data from SDR to DDR.
		   Ex. If "0xaa, 0xbb, 0xcc, 0xdd, 0xee" is in SDR,
		          "0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee" is in DDR(TOGGLE).
		*/
		for(i = 0; i< id_size; i++) {
			*(golden_p + (i << 1) + 0) = *(info->flash_raw_id + i);
			*(golden_p + (i << 1) + 1) = *(info->flash_raw_id + i);
		}
		golden_p ++;

		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
					*golden_p, *(golden_p+1), *(golden_p+2),
					*(golden_p+3), *(golden_p+4), *(golden_p+5)));
		max_dqs_delay = 18;
	}
	else {
		printk("%s:Type:%d isn't allowed\n", __func__, info->flash_type);
		goto out;
	}


	state = 0;
	for(i = 0; i <= max_dqs_delay; i++) {
		// setting the dqs delay before READID.
		writel(i, info->io_base + DQS_DELAY);
		memset(p, 0, id_size_ddr);

		// Issuing the READID
		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 = CMD_FLASH_TYPE(info->flash_type) | CMD_COMPLETE_EN |\
				CMD_INDEX(FIXFLOW_READID) | CMD_BYTE_MODE |\
				CMD_START_CE(info->sel_chip) | CMD_SPARE_NUM(id_size_ddr);

		sp_pnand_issue_cmd(nand, &cmd_f);

		sp_pnand_wait(mtd, nand);

		if(info->flash_type == ONFI2 || info->flash_type == ONFI3) {//xtmark1
			//memcpy(p, info->io_base + SPARE_SRAM + (info->cur_chan<< info->spare_ch_offset), id_size_ddr);
			for(int j = 0; j < (id_size_ddr + 3) / 4; j++) {
				memcpy(p + 4 * j, info->io_base + SPARE_SRAM + 4 * j, 4);
				if (j  == ((id_size_ddr + 3) / 4 - 1))
					memcpy(p + 4* j, info->io_base + SPARE_SRAM + 4 * j, id_size_ddr % 4);
			}

			if(state == 0 && memcmp(golden_p, p, id_size_ddr) == 0) {
				dqs_lower_bound = i;
				state = 1;
			}
			else if(state == 1 && memcmp(golden_p, p, id_size_ddr) != 0){
				dqs_upper_bound = i - 1;
				break;
			}
		}
		else if(info->flash_type == TOGGLE1 || info->flash_type == TOGGLE2) {
			memcpy(p, info->io_base + SPARE_SRAM + (info->cur_chan<< info->spare_ch_offset), id_size_ddr-1);

			if(state == 0 && memcmp(golden_p, p, (id_size_ddr - 1)) == 0) {
				dqs_lower_bound = i;
				state = 1;
			}
			else if(state == 1 && memcmp(golden_p, p, (id_size_ddr - 1)) != 0){
				dqs_upper_bound = (i - 1);
				break;
			}

		}
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
		DBGLEVEL2(sp_pnand_dbg("ID       :0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*p, *(p+1), *(p+2), *(p+3),
							*(p+4), *(p+5), *(p+6), *(p+7),
							*(p+8), *(p+9) ));
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*golden_p, *(golden_p+1), *(golden_p+2), *(golden_p+3),
							*(golden_p+4), *(golden_p+5),*(golden_p+6), *(golden_p+7),
							*(golden_p+8), *(golden_p+9) ));
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
	}
	// Prevent the dqs_upper_bound is zero when ID still accuracy on the max dqs delay
	if(i == max_dqs_delay + 1)
		dqs_upper_bound = max_dqs_delay;

	printk("Upper:%d & Lower:%d for DQS, then Middle:%d\n",
		dqs_upper_bound, dqs_lower_bound, ((dqs_upper_bound + dqs_lower_bound) >> 1));
	// Setting the middle dqs delay
	val = readl(info->io_base + DQS_DELAY);
	val &= ~0x1F;
	val |= (((dqs_lower_bound + dqs_upper_bound) >> 1) & 0x1F);
	writel(val, info->io_base + DQS_DELAY);
out:
	kfree(p);
	kfree(golden_p);

}

static void sp_pnand_calibrate_rlat(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct cmd_feature cmd_f;
	int i, max_rlat;
	int id_size = 5;
	u8 *p, *golden_p;
	u8 rlat_lower_bound, rlat_upper_bound, state;
	u32 ac_reg0, ac_reg1, val;

	rlat_lower_bound = rlat_upper_bound = 0;
	p = kmalloc(id_size, GFP_KERNEL);
	golden_p = kmalloc(id_size, GFP_KERNEL);

	if(info->flash_type == LEGACY_FLASH) {
		for(i = 0; i< id_size; i++) {
			*(golden_p + i) = *(info->flash_raw_id + i);
		}
	} else {
		printk("%s:Type:%d isn't allowed\n", __func__, info->flash_type);
		goto out;
	}

	ac_reg0 = readl(info->io_base + FL_AC_TIMING0(0));
	max_rlat = (ac_reg0 & 0x1F) + ((ac_reg0 >> 8) & 0xF);
	ac_reg1 = readl(info->io_base + FL_AC_TIMING1(0));
	state = 0;
	for(i = 0; i <= max_rlat; i++) {
		// setting the trlat delay before READID.
		val = (ac_reg1 & ~(0x3F<<16)) | (i<<16);
		writel(val, info->io_base + FL_AC_TIMING1(0));
		memset(p, 0, id_size);

		// Issuing the READID
		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 = CMD_FLASH_TYPE(info->flash_type) | CMD_COMPLETE_EN |\
				CMD_INDEX(FIXFLOW_READID) | CMD_BYTE_MODE |\
				CMD_START_CE(info->sel_chip) | CMD_SPARE_NUM(id_size);

		sp_pnand_issue_cmd(nand, &cmd_f);

		sp_pnand_wait(mtd, nand);

		memcpy(p, info->io_base + SPARE_SRAM + (info->cur_chan<< info->spare_ch_offset), id_size);
		if(state == 0 && memcmp(golden_p, p, id_size) == 0) {
			rlat_lower_bound = i;
			state = 1;
		}
		else if(state == 1 && memcmp(golden_p, p, id_size) != 0) {
			rlat_upper_bound = i - 1;
			break;
		}

		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
		DBGLEVEL2(sp_pnand_dbg("ID       :0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*p, *(p+1), *(p+2), *(p+3), *(p+4)));
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*golden_p, *(golden_p+1), *(golden_p+2), *(golden_p+3), *(golden_p+4)));
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
	}

	// Prevent the dqs_upper_bound is zero when ID still accuracy on the max dqs delay
	if(i == max_rlat + 1)
		rlat_upper_bound = max_rlat;

	DBGLEVEL2(sp_pnand_dbg("Upper:%d & Lower:%d for tRLAT, then Middle:%d\n",
		rlat_upper_bound, rlat_lower_bound, ((rlat_upper_bound + rlat_lower_bound) >> 1)));

	// Setting the middle tRLAT
	val = ac_reg1&~(0x3F<<16);
	val |= ((((rlat_upper_bound + rlat_lower_bound) >> 1) & 0x3F) << 16);
	writel(val, info->io_base + FL_AC_TIMING1(0));
out:
	kfree(p);
	kfree(golden_p);
}

static int sp_pnand_available_oob(struct mtd_info *mtd)
{
	struct nand_chip *nand = mtd_to_nand(mtd);
	struct sp_pnand_info *info = nand_get_controller_data(nand);

	int ret = 0;
	int consume_byte, eccbyte, eccbyte_spare;
	int available_spare;

	if (info->useecc < 0)
		goto out;
	if (info->protect_spare != 0)
		info->protect_spare = 1;
	else
		info->protect_spare = 0;

	eccbyte = (info->useecc * 14) / 8;
	if (((info->useecc * 14) % 8) != 0)
		eccbyte++;

	consume_byte = (eccbyte * info->sector_per_page);
	if (info->protect_spare == 1) {

		eccbyte_spare = (info->useecc_spare * 14) / 8;
		if (((info->useecc_spare * 14) % 8) != 0)
			eccbyte_spare++;
		consume_byte += eccbyte_spare;
	}
	consume_byte += CONFIG_BI_BYTE;
	available_spare = info->spare - consume_byte;

	DBGLEVEL2(sp_pnand_dbg(
		"mtd->erasesize:%d, mtd->writesize:%d\n", mtd->erasesize, mtd->writesize));
	DBGLEVEL2(sp_pnand_dbg(
		"page num:%d, info->eccbasft:%d, protect_spare:%d, spare:%d Byte\n",
		mtd->erasesize/mtd->writesize,info->eccbasft, info->protect_spare,
		info->spare));
	DBGLEVEL2(sp_pnand_dbg(
		"consume_byte:%d, eccbyte:%d, eccbytes(spare):%d, useecc:%d bit\n",
		consume_byte, eccbyte, eccbyte_spare, info->useecc));

	/*----------------------------------------------------------
	 * YAFFS require 16 bytes OOB without ECC, 28 bytes with
	 * ECC enable.
	 * BBT require 5 bytes for Bad Block Table marker.
	 */
	if (available_spare >= 4) {
		if (available_spare >= info->max_spare) {
			ret = info->max_spare;
		}
		else {
			if (available_spare >= 64) {
				ret = 64;
			}
			else if (available_spare >= 16) {
				ret = 16;
			}
			else if (available_spare >= 8) {
				ret = 8;
			}
			else if (available_spare >= 4) {
				ret = 4;
			}
		}
		printk(KERN_INFO "Available OOB is %d byte, but we use %d bytes in page mode.\n", available_spare, ret);
	} else {
		printk(KERN_INFO "Not enough OOB, try to reduce ECC correction bits.\n");
		printk(KERN_INFO "(Currently ECC setting for Data:%d)\n", info->useecc);
		printk(KERN_INFO "(Currently ECC setting for Spare:%d)\n", info->useecc_spare);
	}
out:
	return ret;
}

static u8 sp_pnand_read_byte(struct mtd_info *mtd)
{
	struct sp_pnand_info *info = get_pnand_info();
	u32 lv;
	u8 b = 0;

	switch (info->cur_cmd) {
	case NAND_CMD_READID:
		b = readb(info->io_base + SPARE_SRAM + (info->cur_chan << info->spare_ch_offset) + info->byte_ofs);
		info->byte_ofs += 1;
		if (info->byte_ofs == info->max_spare)
			info->byte_ofs = 0;
		break;
	case NAND_CMD_STATUS:
		lv = readl(info->io_base + READ_STATUS0);
		lv = lv >> (info->cur_chan * 8);
		b = (lv & 0xFF);
		break;
	}
	return b;
}


static void sp_pnand_cmdfunc(struct mtd_info *mtd, unsigned command,
			     int column, int page_addr)
{
	struct sp_pnand_info *info = get_pnand_info();
	struct nand_chip *nand = mtd_to_nand(mtd);
	struct cmd_feature cmd_f;
	int real_pg, cmd_sts;
	u8 id_size = 5;

	#if defined(CONFIG_PNANDC_TOSHIBA_TC58NVG4T2ETA00) ||\
		defined(CONFIG_PNANDC_SAMSUNG_K9ABGD8U0B)
	int real_blk_nm, real_off;
	#endif

	cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(info->flash_type);
	info->cur_cmd = command;
	if (page_addr != -1)
		info->page_addr = page_addr;
	if (column != -1)
		info->column = column;

	switch (command) {
	case NAND_CMD_READID:
		DBGLEVEL2(sp_pnand_dbg( "Read ID@(CH:%d, CE:%d)\n", info->cur_chan, info->sel_chip));
		info->byte_ofs = 0;
		// ID size is doubled when the mode is DDR.
		if(info->flash_type == TOGGLE1 || info->flash_type == TOGGLE2 ||
		   info->flash_type == ONFI2 || info->flash_type == ONFI3) {
			id_size = (id_size << 1);
		}

		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 |= CMD_START_CE(info->sel_chip) | CMD_BYTE_MODE |\
				CMD_SPARE_NUM(id_size) | CMD_INDEX(FIXFLOW_READID);

		cmd_sts = sp_pnand_issue_cmd(nand, &cmd_f);
		if(!cmd_sts)
			sp_pnand_wait(mtd, nand);
		else
			printk(KERN_ERR "Read ID err\n");

		break;
	case NAND_CMD_RESET:
		DBGLEVEL2(sp_pnand_dbg("Cmd Reset@(CH:%d, CE:%d)\n", info->cur_chan, info->sel_chip));

		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = 0;
		cmd_f.cq4 |= CMD_START_CE(info->sel_chip);
		//if (info->flash_type == ONFI2 || info->flash_type == ONFI3)
			//cmd_f.cq4 |= CMD_INDEX(ONFI_FIXFLOW_SYNCRESET);
		//else
			cmd_f.cq4 |= CMD_INDEX(FIXFLOW_RESET);

		cmd_sts = sp_pnand_issue_cmd(nand, &cmd_f);
		if(!cmd_sts)
			sp_pnand_wait(mtd, nand);
		else
			printk(KERN_ERR "Reset Flash err\n");

		break;
	case NAND_CMD_STATUS:
		DBGLEVEL2(sp_pnand_dbg( "Read Status\n"));

		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 |= CMD_START_CE(info->sel_chip) | CMD_INDEX(FIXFLOW_READSTATUS);

		cmd_sts = sp_pnand_issue_cmd(nand, &cmd_f);
		if(!cmd_sts)
			sp_pnand_wait(mtd, nand);
		else
			printk(KERN_ERR "Read Status err\n");

		break;
	case NAND_CMD_ERASE1:
		real_pg = info->page_addr;
		DBGLEVEL2(sp_pnand_dbg(
			"Erase Page: 0x%x, Real:0x%x\n", info->page_addr, real_pg));

		cmd_f.cq1 = real_pg;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 |= CMD_START_CE(info->sel_chip) | CMD_SCALE(1);

		if (info->large_page) {
			cmd_f.row_cycle = ROW_ADDR_3CYCLE;
			cmd_f.col_cycle = COL_ADDR_2CYCLE;
			cmd_f.cq4 |= CMD_INDEX(LARGE_FIXFLOW_ERASE);
		} else {
			cmd_f.row_cycle = ROW_ADDR_2CYCLE;
			cmd_f.col_cycle = COL_ADDR_1CYCLE;
			cmd_f.cq4 |= CMD_INDEX(SMALL_FIXFLOW_ERASE);
		}

		/* Someone may be curious the following snippet that
		* sp_pnand_issue_cmd doesn't be followed by
		* sp_pnand_wait.
		* Waiting cmd complete will be call on the mtd upper layer via
		* the registered info->nand.waitfunc.
		*/
		cmd_sts = sp_pnand_issue_cmd(nand, &cmd_f);
		if(cmd_sts)
			printk(KERN_ERR "Erase block err\n");

		break;
	case NAND_CMD_READOOB:
		break;
	case NAND_CMD_READ0:
		break;
	case NAND_CMD_ERASE2:
		break;
	case NAND_CMD_PAGEPROG:
		break;
	case NAND_CMD_SEQIN:
		break;
	default:
		DBGLEVEL2(sp_pnand_dbg( "Unimplemented command (cmd=%u)\n", command));
		break;
	}
}

void sp_pnand_select_chip(struct mtd_info *mtd, int cs)
{
	struct sp_pnand_info *info = get_pnand_info();

	info->cur_chan = 0;
	info->sel_chip = 0;

	//DBGLEVEL2(sp_pnand_dbg("==>chan = %d, ce = %d\n", info->cur_chan, info->sel_chip));
}

static void sp_nand_set_ecc(void)
{
	u32 val;
	struct sp_pnand_info *info = get_pnand_info();

	if (info->useecc > 0) {
		DBGLEVEL1(sp_pnand_dbg("ECC correction bits: %d\n", info->useecc));
		writel(0x01010101, info->io_base + ECC_THRES_BITREG1);
		writel(0x01010101, info->io_base + ECC_THRES_BITREG2);
		val = (info->useecc - 1) | ((info->useecc - 1) << 8) |
			((info->useecc - 1) << 16) | ((info->useecc - 1) << 24);
		writel(val, info->io_base + ECC_CORRECT_BITREG1);
		writel(val, info->io_base + ECC_CORRECT_BITREG2);

		val = readl(info->io_base + ECC_CONTROL);
		val &= ~ECC_BASE;
		if (info->eccbasft > 9)
			val |= ECC_BASE;
		val |= (ECC_EN(0xFF) | ECC_ERR_MASK(0xFF));
		writel(val, info->io_base + ECC_CONTROL);
		writel(ECC_INTR_THRES_HIT | ECC_INTR_CORRECT_FAIL, info->io_base + ECC_INTR_EN);
	} else {
		DBGLEVEL1(sp_pnand_dbg("ECC disabled\n"));
		writel(0, info->io_base + ECC_THRES_BITREG1);
		writel(0, info->io_base + ECC_THRES_BITREG2);
		writel(0, info->io_base + ECC_CORRECT_BITREG1);
		writel(0, info->io_base + ECC_CORRECT_BITREG2);

		val = readl(info->io_base + ECC_CONTROL);
		val &= ~ECC_BASE;
		val &= ~(ECC_EN(0xFF) | ECC_ERR_MASK(0xFF));
		val |= ECC_NO_PARITY;
		writel(val, info->io_base + ECC_CONTROL);
	}

	// Enable the Status Check Intr
	val = readl(info->io_base + INTR_ENABLE);
	val &= ~INTR_ENABLE_STS_CHECK_EN(0xff);
	val |= INTR_ENABLE_STS_CHECK_EN(0xff);
	writel(val, info->io_base + INTR_ENABLE);

	// Setting the ecc capability & threshold for spare
	writel(0x01010101, info->io_base + ECC_THRES_BIT_FOR_SPARE_REG1);
	writel(0x01010101, info->io_base + ECC_THRES_BIT_FOR_SPARE_REG2);
	val = (info->useecc_spare-1) | ((info->useecc_spare-1) << 8) |
		((info->useecc_spare-1) << 16) | ((info->useecc_spare-1) << 24);
	writel(val, info->io_base + ECC_CORRECT_BIT_FOR_SPARE_REG1);
	writel(val, info->io_base + ECC_CORRECT_BIT_FOR_SPARE_REG2);
}

u32 temp_oobsize = 0;
void sp_pnand_set_ecc_for_bblk(struct sp_pnand_info *temp_info, int restore)
{
	struct sp_pnand_info *info = get_pnand_info();
	struct mtd_info * mtd = nand_to_mtd(&info->nand);

	//writel((1 << info->cur_chan), info->io_base + NANDC_SW_RESET);
	// Wait for the NANDC024 software reset is complete
	//while(readl(info->io_base + NANDC_SW_RESET) & (1 << info->cur_chan)) ;
	if (!restore) {
		temp_info->useecc = info->useecc;
		temp_info->useecc_spare = info->useecc_spare;
		temp_info->eccbasft = info->eccbasft;
		temp_info->sector_per_page = info->sector_per_page;
		temp_oobsize = mtd->oobsize;
		mtd->oobsize = info->spare; // used to compose nand_header
		info->useecc = 60;
		info->useecc_spare = 4;
		info->eccbasft = 10; // (1 << 10) = 1024 byte
		if (mtd->writesize == 2048)
			info->sector_per_page = 1;
		else if (mtd->writesize == 4096)
			info->sector_per_page = 3;
		else if (mtd->writesize == 8192)
			info->sector_per_page = 7;
		else
			info->sector_per_page = 1;
	} else {
		//sp_pnand_regdump(&info->nand);
		info->useecc = temp_info->useecc;
		info->useecc_spare = temp_info->useecc_spare;
		info->eccbasft = temp_info->eccbasft;
		info->sector_per_page = temp_info->sector_per_page;
		mtd->oobsize = temp_oobsize;
	}

	DBGLEVEL2(sp_pnand_dbg("ECC correction bits: %d\n", info->useecc));
	DBGLEVEL2(sp_pnand_dbg("ECC step size: %d\n", 1 << info->eccbasft));
	DBGLEVEL2(sp_pnand_dbg("ECC sector per page: %d\n", info->sector_per_page));
	DBGLEVEL2(sp_pnand_dbg("oobsize: %d\n", mtd->oobsize));

	sp_nand_set_ecc();
}

static int sp_pnand_attach_chip(struct nand_chip *nand)
{
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct sp_pnand_info *info = nand_get_controller_data(nand);

	u32 val;
	int i;

	nand->bbt_td = &sp_pnand_bbt_main_descr;
	nand->bbt_md = &sp_pnand_bbt_mirror_descr;
	nand->badblock_pattern = &sp_pnand_largepage_flashbased;

	info->eccbasft = fls(nand->ecc_step_ds) - 1;
	info->useecc = nand->ecc_strength_ds;
	info->protect_spare = 1;
	info->useecc_spare = 4;
	info->spare = mtd->oobsize;
	info->sector_per_page = mtd->writesize >> info->eccbasft;
	/* After nand_register(), the mtd->name is a num rather than a string */
	info->dev_name = mtd->name;
	/*
	 * ISP flow dont use DDR mode. Because synchronous interface
	 * transfer 2 bytes data in 1 cycle. The HW round up ECC
	 * parity data to even. If HW program with 1K60 in sync mode.
	 * the size 105(60*14/8)bytes extend to 106 bytes. And iboot
	 * use async interface then ECC check error.
	 */
	info->flash_type = LEGACY_FLASH;
	info->timing_mode = 0;
#if 0
	if(strcmp(info->name, "MT29F32G08ABXXX 4GiB 8-bit") == 0)
		info->flash_type = ONFI2;
#endif

	val = readl(info->io_base + MEM_ATTR_SET);
	val &= ~(0x7 << 16);

	if(mtd->writesize > 512) {
		info->large_page = 1;
		/* e.x. fls(0x10) = 2 */
		val |= ((fls(mtd->writesize) - 11) << 16); //bit[18:16] 1/2/3/4 -> PageSize=2k/4k/8k/16k
	} else {
		info->large_page = 0;
		val |= PG_SZ_512;
	}

	val &= ~(0x3FF << 2);
	val |= ((mtd->erasesize / mtd->writesize - 1) << 2);
//lichun@add, For BI_byte test
	val &= ~BI_BYTE_MASK;
	val |= (CONFIG_BI_BYTE << 19);
//~lichun
	writel(val, info->io_base + MEM_ATTR_SET);

	val = readl(info->io_base + MEM_ATTR_SET2);
	val &= ~(0x3FF << 16);
	val |=  VALID_PAGE((mtd->erasesize / mtd->writesize - 1));
	writel(val, info->io_base + MEM_ATTR_SET2);

	i = sp_pnand_available_oob(mtd);
	if (likely(i >= 4)) {
		if (i > info->max_spare)
			mtd->oobsize = info->max_spare;
		else
			mtd->oobsize = i;
		info->spare = mtd->oobsize;
	} else
		return -ENXIO;

	DBGLEVEL2(sp_pnand_dbg("total oobsize: %d\n", mtd->oobsize));

	sp_nand_set_ecc();

	nand->ecc.mode = NAND_ECC_HW;
	nand->ecc.size = mtd->writesize;
	nand->ecc.bytes = (info->useecc * 14) / 8;
	if (((info->useecc * 14) % 8) != 0)
		nand->ecc.bytes++;
	nand->ecc.strength = 2;

	nand->ecc.read_page = sp_pnand_read_page_lp;
	nand->ecc.write_page = sp_pnand_write_page_lp;
	nand->ecc.read_oob = sp_pnand_read_oob_lp;
	nand->ecc.write_oob = sp_pnand_write_oob_lp;
	nand->ecc.read_page_raw = sp_pnand_read_page_lp;

	mtd_set_ooblayout(mtd, &sp_pnand_ooblayout_ops);

	printk("Transfer: PIO\n");
	sp_pnand_dbg("Use nand flash %s\n", info->dev_name);
	sp_pnand_dbg("info->eccbasft: %d\n", info->eccbasft);
	sp_pnand_dbg("info->useecc: %d\n", info->useecc);
	sp_pnand_dbg("info->protect_spare: %d\n", info->protect_spare);
	sp_pnand_dbg("info->useecc_spare: %d\n", info->useecc_spare);
	sp_pnand_dbg("info->spare: %d\n", info->spare);
	sp_pnand_dbg("info->flash_type: %d\n", info->flash_type);
	sp_pnand_dbg("info->sector_per_page: %d\n", info->sector_per_page);

	return 0;
}

int sp_pnand_hw_init(struct sp_pnand_info *info)
{
	u32 val;
	int i;

	info->inverse = 0;		/* disable */
	info->scramble = 0;		/* disable */
	info->seed_val = 0;
	info->max_spare = 128;
	info->spare_ch_offset = 7;	/* shift 7 means 0x80*/

	/* Reset the HW */
	writel(1, info->io_base + GLOBAL_RESET);
	while (readl(info->io_base + GLOBAL_RESET)) ;

	val = BUSY_RDY_LOC(6) | CMD_STS_LOC(0) | CE_NUM(2);
	if (info->inverse)
		val |= DATA_INVERSE;
	if (info->scramble)
		val |= DATA_SCRAMBLER;

	writel(val, info->io_base + GENERAL_SETTING);

	if (info->scramble) {
		/* Support FW to program scramble seed */
		val = readl(info->io_base + NANDC_EXT_CTRL);
		for(i = 0; i < MAX_CHANNEL; i++)
			val |= SEED_SEL(i);
		writel(val, info->io_base + NANDC_EXT_CTRL);
		/* random set, b[13:0] */
		info->seed_val = 0x2fa5;
	}

	val = readl(info->io_base + AHB_SLAVEPORT_SIZE);
	val &= ~0xFFF0FF;
	val |= AHB_SLAVE_SPACE_32KB;
	for(int i = 0; i < MAX_CHANNEL; i++)
		val |= AHB_PREFETCH(i);
	val |= AHB_PRERETCH_LEN(128);
	writel(val, info->io_base + AHB_SLAVEPORT_SIZE);

	return 0;
}

void sp_pnand_set_actiming(struct nand_chip *nand)
{
	struct sp_pnand_info *info = nand_get_controller_data(nand);
	/* TODO: calibrate the DQS delay for Sync */
	/*----------------------------------------------------------
	 * ONFI synch mode means High Speed. If fails to change to
	 * Synch mode, then use flash as Async mode(Normal speed) and
	 * use LEGACY_LARGE fix flow.
	 */
	if (info->flash_type == ONFI2 || info->flash_type == ONFI3) {
		if (sp_pnand_onfi_sync(nand) == 0) {
			sp_pnand_calc_timing(nand);
			sp_pnand_calibrate_dqs_delay(nand);
		} else {
			DBGLEVEL1(sp_pnand_dbg("Failed to set DDR mode! Use SDR\n"));
			info->flash_type = LEGACY_FLASH;
		}
	}

	// Toggle & ONFI flash has set the proper timing before READ ID.
	// We don't do that twice.
	if(info->flash_type == LEGACY_FLASH) {
		sp_pnand_calc_timing(nand);
		sp_pnand_calibrate_rlat(nand);
	}
}

int sp_pnand_init(struct sp_pnand_info *info)
{
	struct nand_chip *nand = &info->nand;
	struct mtd_info *mtd = nand_to_mtd(nand);
	int ret;

	nand_set_controller_data(nand, info);
	/* hw reset, genernal config*/
	sp_pnand_hw_init(info);

	nand->IO_ADDR_R = nand->IO_ADDR_W = info->sram_base;
	nand->select_chip = sp_pnand_select_chip;
	nand->cmdfunc = sp_pnand_cmdfunc;
	nand->read_byte = sp_pnand_read_byte;
	nand->waitfunc = sp_pnand_wait;
	nand->chip_delay = 0;
	nand->options = NAND_NO_SUBPAGE_WRITE;// | NAND_OWN_BUFFERS;
	nand->options |= NAND_BBT_SCAN2NDPAGE;
	//nand->bbt_options = NAND_BBT_USE_FLASH;////////////////////xtdebug:close scan bbt

	// Set the default AC timing/Warmup cyc for sp_pnand.
	// The register of AC timing/Warmup  keeps the value
	// set before although the Global Reset is set.
	sp_pnand_set_default_timing(nand);
	sp_pnand_set_warmup_cycle(nand, 0, 0);//disable

	// Read the raw id to calibrate the DQS delay for Sync. latching(DDR)
	sp_pnand_read_raw_id(nand);

	if (nand->cmdfunc == NULL)
		printf("(%d)%s\n", __LINE__, __FUNCTION__);
	struct nand_chip *test = mtd_to_nand(mtd);
	if (test->cmdfunc == NULL)
		printf("(%d)%s\n", __LINE__, __FUNCTION__);

	printf("(%d)%s\n", __LINE__, __FUNCTION__);
	ret = nand_scan_ident(mtd, MAX_CE, (struct nand_flash_dev *)sp_pnand_ids);
	if (ret)
		return ret;
	printf("(%d)%s\n", __LINE__, __FUNCTION__);
	ret = sp_pnand_attach_chip(nand);
	if (ret)
		return ret;

	ret = nand_scan_tail(mtd);
	if (ret)
		return ret;

	sp_pnand_set_actiming(nand);
	printf("(%d)%s\n", __LINE__, __FUNCTION__);
	ret = nand_register(0, mtd);
	if (!ret)
		return ret;

	return 0;
}

/*
 * Probe for the NAND device.
 */
static int sp_pnand_probe(struct udevice *dev)
{
	struct sp_pnand_info *info = our_paranfc = dev_get_priv(dev);
	int ret;
	struct clk clk;
	struct reset_ctl reset_ctl;
	const void *blob = gd->fdt_blob;
	int node = dev_of_offset(dev);

	info->io_base = (void __iomem*)devfdt_get_addr_index(dev, 0);
	info->sram_base = (void __iomem*)devfdt_get_addr_index(dev, 1);

	info->clkfreq = fdtdec_get_uint(blob, node, "clock-frequency", 0);

	ret = clk_get_by_index(dev, 0, &clk);
	if (ret) {
		dev_err(dev, "Failed to clk_get_by_index()\n");
		return ret;
	}

	ret = clk_prepare_enable(&clk);
	if (ret) {
		dev_err(dev, "Failed to clk_prepare_enable()\n");
		return ret;
	}

	ret = clk_set_rate(&clk, info->clkfreq);
	if (IS_ERR_VALUE(ret)) {
		dev_err(dev, "Failed to clk_set_rate()\n");
		return ret;
	}

	info->clkfreq = clk_get_rate(&clk);
	DBGLEVEL2(sp_pnand_dbg("info->clkfreq %d\n", info->clkfreq));

	ret = reset_get_by_index(dev, 0, &reset_ctl);
	if (ret) {
		dev_err(dev, "Failed to reset_get_by_index()\n");
		return ret;
	}

	ret = reset_deassert(&reset_ctl);
	if (ret) {
		dev_err(dev, "Failed to reset_deassert()\n");
		return ret;
	}

	DBGLEVEL1(sp_pnand_dbg("info->io_base:0x%08lx\n", (unsigned long)info->io_base));
	DBGLEVEL1(sp_pnand_dbg("info->sram_base:0x%08lx\n", (unsigned long)info->sram_base));

	ret = sp_pnand_init(info);
	printf("(%d)%s\n", __LINE__, __FUNCTION__);
	return ret;
}

static const struct udevice_id eys3d_paranand[] = {
	{ .compatible = "eys3d,ecv5546-para-nand"},
	{}
};

U_BOOT_DRIVER(eys3d_para_nand) = {
	.name           = "eys3d_para_nand",
	.id             = UCLASS_MTD,
	.of_match       = eys3d_paranand,
	.probe          = sp_pnand_probe,
	.priv_auto      = sizeof(struct sp_pnand_info),
};

void board_paranand_init(void)
{
	struct udevice *dev;
	int ret;

	DBGLEVEL2(sp_pnand_dbg("board_paranand_init() entry\n"));

	ret = uclass_get_device_by_driver(UCLASS_MTD,
					  DM_DRIVER_GET(eys3d_para_nand),
					  &dev);

	if (ret && ret != -ENODEV)
		DBGLEVEL1(sp_pnand_dbg("Failed to initialize %s. (error %d)\n", dev->name, ret));
}
//#define INPUT_RANDOM_DATA
//#define PNAND_MEASURE

#ifdef PNAND_MEASURE
#ifdef INPUT_RANDOM_DATA
#define RANDOM_SIZE 1024
u8 *random_data;
#endif
static void pattern_generate(u32 data, u8 *buf, u32 size)
{
	u32 i = 0;

	printk("%s, data: 0x%08x, size: 0x%08x\n", __FUNCTION__, data, size);

#ifdef INPUT_RANDOM_DATA
	srand(data);
	for (i = 0; i < RANDOM_SIZE; i++) {
		random_data[i] = rand() % 256;
		if(i % 16 == 0)
			printk("\n");
		printk("%02x ", random_data[i]);
	}

	for (i = 0; i < size; i++) {
		buf[i] = random_data[i % RANDOM_SIZE];
	}
#else
	u8 *p = (u8 *)&data;
	for (i = 0; i < size; i++) {
		buf[i] = *(p + (i & 0x03));
	}
#endif
}

static int pattern_check(u32 data, u8 *buf, u32 size)
{
	int i = 0;
	int ret = 0;

	printk("%s, data: 0x%08x, size: 0x%08x\n", __FUNCTION__, data, size);

#ifdef INPUT_RANDOM_DATA
	srand(data);
	for (i = 0; i < RANDOM_SIZE; i++) {
		random_data[i] = rand() % 256;
		if(i % 16 == 0)
			printk("\n");
		printk("%02x ", random_data[i]);
	}

	for(i = 0; i < size; i++) {
		if(buf[i] != random_data[i % RANDOM_SIZE]) {
			printk("\rdata mis-match. i: %d, buf: 0x%02x, random_data: 0x%02x\n", i, buf[i], random_data[i % RANDOM_SIZE]);
			ret = -1;
		}
	}
#else
	u8 *p = (u8*)&data;
	for(i = 0; i < size; i++) {
		if(buf[i] != *(p + (i & 0x03))) {
			printk("\rdata mis-match. i: %d, buf[i]: 0x%02x, *(p+(i&0x03): 0x%02x\n", i, buf[i], *(p + (i & 0x03)));
			ret = -1;
		}
	}
#endif
	return ret;
}

bool str2dec(const char *p, u32 *num)
{
        char *endptr;

        *num = simple_strtoull(p, &endptr, 10);
        return *p != '\0' && *endptr == '\0';
}

static int sp_pnand_test_erw(u32 addr, u32 size, u32 seed, u32 action)
{
	//struct mtd_info *mtd  = get_nand_dev_by_index(nand_curr_device);

	struct sp_pnand_info *info = get_pnand_info();
	struct nand_chip *nand = &info->nand;
	struct mtd_info *mtd = nand_to_mtd(nand);
	struct cmd_feature cmd_f;

	nand_erase_options_t erase_opts;
	u32 block_size = mtd->erasesize;
	u32 page_size = mtd->writesize;
	u32 block_num = (size + block_size - 1) / block_size;
	u32 start_block = addr / block_size;
	u32 sector_max = mtd->writesize >> info->eccbasft;
	size_t actual_size;
	u32 now_seed;
	u32 i, j;
	int ret;
	u8 *buf;

	int status;
	u32 *lbuf;
	u32 data_size;
	u32 real_pg;
	u32 sector_cnt = 1;
	u32 page_cnt = 1;
	int no_ecc = 0;
	u32 val;

	buf = (u8 *)malloc(block_size);
	if (!buf) {
		printk("no memory!\n");
		goto err_out;
	}
	memset(buf, 0, sizeof(block_size));
#ifdef INPUT_RANDOM_DATA
	random_data = (u8 *)malloc(RANDOM_SIZE);
	if (!random_data) {
		printk("no memory!\n");
		goto err_out;
	}
	memset(random_data, 0, sizeof(block_size));
#endif
	if (action & 0x1000)
		no_ecc = 1;

	if (action & 0xff0) {
		sector_cnt = (action >> 4) & 0xff;
		page_cnt = (action >> 4) & 0xff;
	} else {
		printk("start block:%d, block_num:%d, seed:0x%x\n", start_block, block_num, seed);
	}

	action &= 0xf;

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
			for (i = 0, j = start_block; i < block_num; i++, j++) {
				/* skip bad block */
				while (nand_block_isbad(mtd, (loff_t)j*block_size))
					j++;

				printk("\rwrite block:%d\n",j);

				/* prepare the pattern */
				//now_seed += (i * block_size) / 4;
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

			if(no_ecc) {
				val = readl(info->io_base + ECC_CONTROL);
				val = val & ~(ECC_EN(0xFF));
				writel(val, info->io_base + ECC_CONTROL);
			}

			for (i = 0, j = start_block; i < block_num; i++, j++) {
				//now_seed += (i * block_size) / 4;

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

			if(no_ecc) {
				val = readl(info->io_base + ECC_CONTROL);
				val = val | ECC_EN(0xFF);
				writel(val, info->io_base + ECC_CONTROL);
			}
			break;
		case 3:
			/* Read sector */
			if (sector_cnt > sector_max)
				sector_cnt = sector_max;
			DBGLEVEL1(sp_pnand_dbg("start blcok:%d sector_cnt %d\n", start_block, sector_cnt));

			now_seed = seed;
			real_pg = start_block * block_size / page_size; //0x480;

			cmd_f.row_cycle = ROW_ADDR_3CYCLE;
			cmd_f.col_cycle = COL_ADDR_2CYCLE;
			cmd_f.cq1 = real_pg | SCR_SEED_VAL1(info->seed_val);
			cmd_f.cq2 = CMD_EX_SPARE_NUM(info->spare) | SCR_SEED_VAL2(info->seed_val);
			cmd_f.cq3 = CMD_COUNT(sector_cnt) | (info->column & 0xFF);
			cmd_f.cq4 = CMD_COMPLETE_EN | CMD_FLASH_TYPE(info->flash_type) |\
					CMD_START_CE(info->sel_chip) | CMD_SPARE_NUM(info->spare) |\
					CMD_INDEX(LARGE_FIXFLOW_PAGEREAD);

			status = sp_pnand_issue_cmd(nand, &cmd_f);
			if(status < 0) {
				printk("\rread sectors => fail\n");
				goto err_out;
			}

			sp_pnand_wait(mtd, nand);

			data_size = sector_cnt << info->eccbasft;
			if(!BMC_region_status_empty(info)) {
				lbuf = (u32 *)buf;
				for (i = 0; i < data_size; i += 4)
					*lbuf++ = *(volatile unsigned *)(nand->IO_ADDR_R);
			} else {
				printk(KERN_ERR "Transfer timeout!");
				printk("\rread sectors => fail\n");
				goto err_out;
			}

			if (pattern_check(now_seed, buf, data_size) == -1) {
				printk("\rread sectors => data mis-match\n");
				goto err_out;
			}

			printk("\rread sectors => success\n");
			break;
		case 4:
			/* Read pages */
			DBGLEVEL1(sp_pnand_dbg("start blcok:%d page_cnt %d\n", start_block, page_cnt));
			now_seed = seed;

			for (i = 0, j = start_block; i < page_cnt; i++) {
				//now_seed += (i * block_size) / 4;

				/* skip bad block */
				while (nand_block_isbad(mtd, (loff_t)j*block_size))
					j++;

				printk("\rread block:%d\n",j);

				actual_size = page_size;
				ret = nand_read(mtd, (loff_t)(j*block_size+i*page_size), &actual_size, buf);
				if (ret) {
					printk("\rread pages => fail\n");
					ret = CMD_RET_FAILURE;
					goto err_out;
				}

				if (pattern_check(now_seed, buf, page_size) == -1) {
					printk("\rread pages => data mis-match\n");
					goto err_out;
				}
			}
			printk("\rread pages => success\n");
			break;

		default:
			ret = CMD_RET_USAGE;
	}

err_out:
	if (buf)
		free(buf);
#ifdef INPUT_RANDOM_DATA
	if (random_data)
		free(random_data);
#endif
	return ret;
}

struct pad_ctl_regs {
	unsigned int st_enable[4];		// 101.0 - 101.3
	unsigned int driving_selector0[4];	// 101.4 - 101.7
	unsigned int driving_selector1[4];	// 101.8 - 101.11
	unsigned int driving_selector2[4];	// 101.12 - 101.15
	unsigned int driving_selector3[4];	// 101.16 - 101.19
	unsigned int xtal_config; 		// 101.20
	unsigned int reserved_20;		// 101.21
	unsigned int sd_config;			// 101.22
	unsigned int sdio_config;		// 101.23
	unsigned int reserved_24;		// 101.24
	unsigned int gpio_first[4];		// 101.25 - 101.28
	unsigned int reserved_29[3];		// 101.29 - 101.31
};
#define PAD_CTL_REG	((volatile struct pad_ctl_regs *)0xF8803280) // G101.0

static void sp_pnand_set_ds(u8 pin, u8 strength)
{
	u32 reg_off = pin / 32;
	u32 bit_mask = 1 << (pin % 32);

	strength = (strength > 15) ? 15 : strength;

	if (strength & 1)
		PAD_CTL_REG->driving_selector0[reg_off] |= bit_mask;
	else
		PAD_CTL_REG->driving_selector0[reg_off] &= ~bit_mask;

	if (strength & 2)
		PAD_CTL_REG->driving_selector1[reg_off] |= bit_mask;
	else
		PAD_CTL_REG->driving_selector1[reg_off] &= ~bit_mask;

	if (strength & 4)
		PAD_CTL_REG->driving_selector2[reg_off] |= bit_mask;
	else
		PAD_CTL_REG->driving_selector2[reg_off] &= ~bit_mask;

	if (strength & 8)
		PAD_CTL_REG->driving_selector3[reg_off] |= bit_mask;
	else
		PAD_CTL_REG->driving_selector3[reg_off] &= ~bit_mask;

}

u8 sp_pnand_get_ds(u8 pin)
{
	u32 bit0, bit1, bit2, bit3;
	u32 reg_off = pin / 32;
	u32 bit_shift = pin % 32;
	u8 strength;

	bit0 = PAD_CTL_REG->driving_selector0[reg_off];
	bit1 = PAD_CTL_REG->driving_selector1[reg_off];
	bit2 = PAD_CTL_REG->driving_selector2[reg_off];
	bit3 = PAD_CTL_REG->driving_selector3[reg_off];

	bit0 = (bit0 >> bit_shift) & 0x1;
	bit1 = (bit1 >> bit_shift) & 0x1;
	bit2 = (bit2 >> bit_shift) & 0x1;
	bit3 = (bit3 >> bit_shift) & 0x1;

	strength = (bit3 << 3) | (bit2 << 2) | (bit1 << 1) | bit0;

	return strength;
}

void sp_pnand_test_driving(u32 val)
{
	u8 strength = val & 0xff;
	u8 pin = (val >> 8) & 0xff;
	u8 gs_flag = (val >> 16) & 0xff;

	if(gs_flag) {
		printk("DUMP the DS:\n");
		for(pin = 20; pin < 37; pin++) {
			strength = sp_pnand_get_ds(pin);
			printk("GPIO[%d] ds = %d\n", pin, strength);
		}
	} else {
		if(pin) {
			sp_pnand_set_ds(pin, strength);
			printk("GPIO[%d] ds = %d\n", pin, strength);
		} else {
			for(pin = 20; pin < 37; pin++)
				sp_pnand_set_ds(pin, strength);
		}
	}

}

#define PAD_CTL2_REG23	0xF880335C //G102.23

void sp_pnand_test_softpad(u32 val)
{
	u32 set = val & 0xff;
	int mode = (val >> 8) & 0xff;
	u8 bit12, bit31;

	if(set) {
		if (mode) { // 1:bypass 0 resample
			*(volatile u32 *)PAD_CTL2_REG23 |= (1 << 31);// bit[31]:1
			*(volatile u32 *)PAD_CTL2_REG23 &= ~(1 << 12);// bit[12]:0
		} else {
			*(volatile u32 *)PAD_CTL2_REG23 &= ~(1 << 31);// bit[31]:0
			*(volatile u32 *)PAD_CTL2_REG23 |= (1 << 12);// bit[12]:1
		}
	} else {
		set = *(volatile u32 *)PAD_CTL2_REG23;
		bit12 = (set >> 12) & 0x1;
		bit31 = (set >> 31) & 0x1;
		if ((bit12 == 1) && (bit31 == 0))
			printk("Softpad: resample\n");
		else if ((bit12 == 0) && (bit31 == 1))
			printk("Softpad: bypass\n");
	}
}

#define PAD_CTL2_REG20	0xF8803350 //G102.20

void sp_pnand_test_temp(int argc, char * const argv[])
{
	struct sp_pnand_info *info = get_pnand_info();
	struct nand_chip *nand = &info->nand;
	u32 val, param1;

	if (strncmp(argv[0], "feat", 4) == 0) {
		int new_mode, new_type;

		/* Only set feature */
		str2off(argv[1], (loff_t *)&param1);

		new_mode = param1 & 0xf;
		new_type = (param1 & 0x10) >> 4;

		sp_pnand_onfi_set_feature(nand, param1, info->flash_type);

		if (new_type)
			param1 |= (param1 << 8);

		val = sp_pnand_onfi_get_feature(nand, new_type);
		if (val != param1) {
			DBGLEVEL1(sp_pnand_dbg("Failed. Ct feat 0x%x\n", val));
		} else {
			info->timing_mode = new_mode;
			info->flash_type = new_type;
			DBGLEVEL1(sp_pnand_dbg("Success. Ct feat 0x%x\n", val));
		}
		sp_pnand_calc_timing(nand);
	}else if (strncmp(argv[0], "delay", 4) == 0) {

		u32 bit18_15;
		if (strncmp(argv[1], "get", 3) == 0) {
			val = *(volatile u32 *)PAD_CTL2_REG20;
			bit18_15 = (val >> 15) & 0xf; /* Coarse tuning */
			DBGLEVEL1(sp_pnand_dbg("Delay coarse-tuning %d ns\n", bit18_15));
		} else {
			/* Coarse tuning */
			str2dec(argv[1], &param1);

			/* bit[18:15] is valid */
			param1 &= 0xf;

			/* clear bit[18:15] */
			*(volatile u32 *)PAD_CTL2_REG20 &= ~(0xf << 15);
			*(volatile u32 *)PAD_CTL2_REG20 |= (param1 << 15);
		}
	}
}

__attribute__ ((unused))
static int sp_paranand_test(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	u32 param1 = 0, param2 = 0, param3 = 0;
	u32 val;
	struct sp_pnand_info *info = get_pnand_info();
	struct nand_chip *nand = &info->nand;
	struct mtd_info *mtd = nand_to_mtd(nand);

	char *cmd = argv[1];
	int ret = 0;
	u32 test_size, test_addr;
	int old_mode, old_type, new_mode, new_type;
	u32 test_seed;

	if (strncmp(cmd, "mode", 4) == 0) {

		if (argc < 3) {
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		if (strncmp(argv[2], "get", 3) == 0) {
			val = sp_pnand_onfi_get_feature(nand, info->flash_type);
			printk("Ct. feat : 0x%x\n", val);
		} else {
			str2off(argv[2], (loff_t *)&param1);
			new_mode = param1 & 0xf;
			new_type = (param1 & 0x10) >> 4;

			/*
			 * Record current mode which is used to restore after
			 * mode switching failure.
			 */
			val = sp_pnand_onfi_get_feature(nand, info->flash_type);
			old_mode = val & 0xf;
			old_type = (val & 0x10) >> 4;

			/*
			 *  ------------------------------------------------
			 * | Old type  |   New type  |  Operation           |
			 *  ------------------------------------------------
			 * |  SDR      |   SDR/DDR   |  set feature         |
			 *  ------------------------------------------------
			 * |  DDR      |   SDR/DDR   |  reset + set feature |
			 *  ------------------------------------------------
			 */
			if (old_type) {
				/* Send reset switch device DDR to SDR */
				sp_pnand_abort(nand);
				/*
				 * After reset succeed, the related
				 * paramters are set back to SDR mode
				 */
				info->flash_type = LEGACY_FLASH;
				info->timing_mode = 0;
				sp_pnand_calc_timing(nand);

				sp_pnand_onfi_set_feature(nand, param1, info->flash_type);
			} else {
				sp_pnand_onfi_set_feature(nand, param1, info->flash_type);
			}

			if (new_type)
				param1 |= (param1 << 8);

			val = sp_pnand_onfi_get_feature(nand, new_type);
			if (val != param1) {
				info->timing_mode = old_mode;
				info->flash_type = old_type;
				DBGLEVEL1(sp_pnand_dbg("Failed. Ct feat 0x%x\n", val));
			} else {
				info->timing_mode = new_mode;
				info->flash_type = new_type;
				DBGLEVEL1(sp_pnand_dbg("Success. Ct feat 0x%x\n", val));
			}

			sp_pnand_calc_timing(nand);
		}
	} else if (strncmp(cmd, "driving", 7) == 0) {

		if (argc < 3) {
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		if (strncmp(argv[2], "get", 3) == 0) {
			param3 = 1;
		} else if (strncmp(argv[2], "set", 3) == 0) {
			param3 = 0;
		} else {
			printk("[Error] Check cmd parameter set/get\n");
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		str2dec(argv[3], &param1);

		if (argc > 4)
			str2dec(argv[4], &param2);

		/* val bit[23:16]:get or set; bit[15:8]:gpio#x; bit[7:0]:ds */
		val = (param3 << 16) | (param2 << 8) | param1;
		sp_pnand_test_driving(val);

	} else if (strncmp(cmd, "softpad", 7) == 0) {
		if (argc < 3) {
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		if (strncmp(argv[2], "set", 3) == 0) {
			param1 = 1;
		} else if (strncmp(argv[2], "get", 3) == 0){
			param1 = 0;
		} else {
			printk("[Error] Check cmd parameter set/get\n");
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		if (argc > 3) {
			if (strncmp(argv[3], "bypass", 6) == 0) {
				param2 = 1;
			} else if (strncmp(argv[3], "resample", 8) == 0){
				param2 = 0;
			} else {
				printk("[Error] Check cmd parameter bypass/resample\n");
				ret = CMD_RET_USAGE;
				goto ret_usage;
			}
		}

		val = (param2 << 8) | param1;
		sp_pnand_test_softpad(val);
	} else if (strncmp(cmd, "reset", 5) == 0) {
		sp_pnand_abort(nand);
		/* After reset succeed, the related paramters are set back to SDR mode */
		info->flash_type = LEGACY_FLASH;
		info->timing_mode = 0;
		sp_pnand_calc_timing(nand);
	} else if (strncmp(cmd, "readid", 6) == 0) {
		struct cmd_feature cmd_f;
		int i, j;
		int id_size = 5;
		int id_size_ddr = (id_size << 1);
		u8 *p, *golden_p;

		p = kmalloc(id_size_ddr, GFP_KERNEL);
		golden_p = kmalloc(id_size_ddr, GFP_KERNEL);

		/* Extent the data from SDR to DDR.
		   Ex. If "0xaa, 0xbb, 0xcc, 0xdd, 0xee" is in SDR,
		          "0xaa, 0xaa, 0xbb, 0xbb, 0xcc, 0xcc, 0xdd, 0xdd, 0xee, 0xee" is in DDR(ONFI).
		*/
		for(i = 0; i< id_size; i++) {
			*(golden_p + (i << 1) + 0) = *(info->flash_raw_id + i);
			*(golden_p + (i << 1) + 1) = *(info->flash_raw_id + i);
		}

		memset(p, 0, id_size_ddr);

		// Issuing the READID
		cmd_f.row_cycle = ROW_ADDR_1CYCLE;
		cmd_f.col_cycle = COL_ADDR_1CYCLE;
		cmd_f.cq1 = 0;
		cmd_f.cq2 = 0;
		cmd_f.cq3 = CMD_COUNT(1);
		cmd_f.cq4 = CMD_FLASH_TYPE(info->flash_type) | CMD_COMPLETE_EN |\
				CMD_INDEX(FIXFLOW_READID) | CMD_BYTE_MODE |\
				CMD_START_CE(info->sel_chip) | CMD_SPARE_NUM(id_size_ddr);

		sp_pnand_issue_cmd(nand, &cmd_f);

		sp_pnand_wait(mtd, nand);

		for(j = 0; j < (id_size_ddr + 3) / 4; j++) {
			memcpy(p + 4 * j, info->io_base + SPARE_SRAM + 4 * j, 4);
			if (j  == ((id_size_ddr + 3) / 4 - 1))
				memcpy(p + 4* j, info->io_base + SPARE_SRAM + 4 * j, id_size_ddr % 4);
		}
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
		DBGLEVEL2(sp_pnand_dbg("ID       :0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*p, *(p+1), *(p+2), *(p+3),
							*(p+4), *(p+5), *(p+6), *(p+7),
							*(p+8), *(p+9) ));
		DBGLEVEL2(sp_pnand_dbg("Golden ID:0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
							*golden_p, *(golden_p+1), *(golden_p+2), *(golden_p+3),
							*(golden_p+4), *(golden_p+5),*(golden_p+6), *(golden_p+7),
							*(golden_p+8), *(golden_p+9) ));
		DBGLEVEL2(sp_pnand_dbg("===============================================\n"));
		if(memcmp(golden_p, p, id_size_ddr) == 0) {
			DBGLEVEL2(sp_pnand_dbg("Compare ID is OK\n"));
		} else {
			DBGLEVEL2(sp_pnand_dbg("Compare ID is Failed\n"));
		}
	} else if (strncmp(cmd, "test", 4) == 0) {
		if (argc < 3) {
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		/* Test start from 9th block */
		test_addr = mtd->erasesize * 9;
		test_size = mtd->erasesize * 2;
		DBGLEVEL2(sp_pnand_dbg("test_addr 0x%x\n", test_addr));
		DBGLEVEL2(sp_pnand_dbg("test_size 0x%x\n", test_size));
#if 0
		if (mtd->writesize = 8192) { //Mircon
			test_addr = 0x900000;
			test_size = 0x1000000;
		} else { //Winbond
			test_addr = 0x40000;
			test_size = 0x20000;
		}
#endif
		cmd = argv[2];

		test_seed = 0xff00ff00;

		param1 = 0;
		if (argc == 4) {
			str2off(argv[3], (loff_t *)&param1);
		}

		if (strncmp(cmd, "erase", 5) == 0) {
			ret = sp_pnand_test_erw(test_addr, test_size, test_seed, 0);
		} else if (strncmp(cmd, "write", 5) == 0) {
			ret = sp_pnand_test_erw(test_addr, test_size, test_seed, 1);
		} else if (strncmp(cmd, "read", 4) == 0) {
			param1 = (param1 << 12) | 2;
			ret = sp_pnand_test_erw(test_addr, test_size, test_seed, param1);
		} else if (strncmp(cmd, "sector", 6) == 0) {
			param1 = (param1 << 4) | 3;
			ret = sp_pnand_test_erw(test_addr, test_size, test_seed, param1);
		} else if (strncmp(cmd, "page", 4) == 0) {
			param1 = (param1 << 4) | 4;
			ret = sp_pnand_test_erw(test_addr, test_size, test_seed, param1);
		} else {
			ret = CMD_RET_USAGE;
		}
	}else if (strncmp(cmd, "temp", 4) == 0) {
		if (argc < 3) {
			ret = CMD_RET_USAGE;
			goto ret_usage;
		}

		sp_pnand_test_temp(argc - 2, argv + 2);

	} else {
		ret = CMD_RET_USAGE;
	}

ret_usage:
	return ret;
}

U_BOOT_CMD(pnand, CONFIG_SYS_MAXARGS, 1, sp_paranand_test,
	"eys3d parallel-nand tests",
	"pnand mode [value] - set actiming mode, 0~5 are allowed.\n"
	"pnand driving [set/get] [ds] [pin]- set driving strength. The ds range is 0~15.\n"
	"pnand softpad [out/input] [bypass/resample]- set softpad.\n"
	"pnand reset - reset device synchronous to asynchronous interface.\n"
	"pnand test - timing measurement command.\n"
	"\terase - erase a block.\n"
	"\twrite - write a block.\n"
	"\tread - read a block.\n"
	"\tsector - read some sectors.\n"
	"pnand temp - temporary test command.\n"
	"\tfeat [val] - set feature.\n"
	"\tdelay [ns] - pnand softpad delay.\n"
);
#endif
