#include <common.h>
#include <command.h>
#include <image.h>
#include <mapmem.h>
#include <cpu_func.h>

static uint32_t sum32(uint32_t sum, uint8_t *data, uint32_t len)
{
	uint32_t val = 0, pos = 0;

	for (; (pos + 4) <= len; pos += 4)
		sum += *(uint32_t *)(data + pos);
	/*
	 * word0: 3 2 1 0
	 * word1: _ 6 5 4
	 */
	for (; len - pos; len--)
		val = (val << 8) | data[len - 1];

	sum += val;

	return sum;
}
/* Similar with original u-boot flow but use different crc calculation */
int sp_image_check_hcrc(const struct legacy_img_hdr *hdr)
{
	ulong hcrc;
	ulong len = image_get_header_size();
	struct legacy_img_hdr header;

	/* Copy header so we can blank CRC field for re-calculation */
	memmove(&header, (char *)hdr, image_get_header_size());
	image_set_hcrc(&header, 0);

	hcrc = sum32(0, (unsigned char *)&header, len);

	return (hcrc == image_get_hcrc(hdr));
}

/* Similar with original u-boot flow but use different crc calculation */
int sp_image_check_dcrc(const struct legacy_img_hdr *hdr)
{
	ulong data = image_get_data(hdr);
	ulong len = image_get_data_size(hdr);
	ulong dcrc = sum32(0, (unsigned char *)data, len);

	return (dcrc == image_get_dcrc(hdr));
}

/*
 * Similar with original u-boot verifiction. Only data crc is different.
 * Return NULL if failed otherwise return header address.
 */
int sp_qk_uimage_verify(ulong img_addr, int verify)
{
	struct legacy_img_hdr *hdr = (struct legacy_img_hdr *)img_addr;

	/* original uImage header's magic */
	if (!image_check_magic(hdr)) {
		puts("Bad Magic Number\n");
		return (int)((u64)NULL);
	}

	/* hcrc by quick eys3d crc */
	if (!sp_image_check_hcrc(hdr)) {
		puts("Bad Header Checksum(Simplified)\n");
		return (int)((u64)NULL);
	}

	image_print_contents(hdr);

	/* dcrc by quick eys3d crc */
	if (verify) {
		puts("   Verifying Checksum ... ");
		if (!sp_image_check_dcrc(hdr)) {
			printf("Bad Data CRC(Simplified)\n");
			return (int)((u64)NULL);
		}
		puts("OK\n");
	}

	return (int)((u64)hdr);
}

/* return 0 if failed otherwise return 1 */
int sp_image_verify(u32 kernel_addr, u32 dtb_addr)
{
	if (!sp_qk_uimage_verify(kernel_addr, 1))
		return 0;

	if (!sp_qk_uimage_verify(dtb_addr, 1))
		return 0;

	return 1;
}

__attribute__((weak))
unsigned long do_sp_go_exec(ulong (*entry)(int, char * const [], unsigned int), int argc,
			    char * const argv[], unsigned int dtb)
{
	u32 kernel_addr, dtb_addr; /* these two addr will include headers. */

	kernel_addr = simple_strtoul(argv[0], NULL, 16);
	dtb_addr = simple_strtoul(argv[1], NULL, 16);

	printf("[u-boot] kernel address 0x%08x, dtb address 0x%08x\n",
		kernel_addr, dtb_addr);

	if (!sp_image_verify(kernel_addr, dtb_addr))
		return CMD_RET_FAILURE;

	cleanup_before_linux();

	return entry (0, 0, (dtb_addr + 0x40));
}

static int do_sp_go(struct cmd_tbl *cmdtp, int flag, int argc, char * const argv[])
{
	ulong   addr, rc;
	int     rcode = 0;

	addr = simple_strtoul(argv[1], NULL, 16);
	addr += 0x40; /* 0x40 for skipping quick uImage header */

	printf ("## Starting application at 0x%08lX ...\n", addr);

	/*
	 * pass address parameter as argv[0] (aka command name),
	 * and all remaining args
	 */
	rc = do_sp_go_exec ((void *)addr, argc - 1, argv + 1, 0);
	if (rc != 0) rcode = 1;

	printf ("## Application terminated, rc = 0x%lX\n", rc);
	return rcode;
}

U_BOOT_CMD(
	sp_go, CONFIG_SYS_MAXARGS, 1, do_sp_go,
	"eys3d booting command",
	"sp_go - run kernel at address 'addr'\n"
	"\n"
	"sp_go [kernel addr] [dtb addr]\n"
	"\tkernel addr should include the 'qk_sp_header'\n"
	"\twhich is similar as uImage header but different crc method.\n"
	"\tdtb also should have 'qk_sp_header', althrough dtb originally\n"
	"\thas its own header.\n"
	"\n"
	"\tSo image would be like this :\n"
	"\t<kernel addr> : [qk uImage header][kernel]\n"
	"\t<dtb addr>    : [qk uImage header][dtb header][dtb]\n"
);

#ifdef SPEED_UP_SPI_NOR_CLK

#if 0
#define dev_dbg(fmt, args ...)  printf(fmt, ## args)
#else
#define dev_dbg(fmt, args ...)
#endif
#define SPI_NOR_CTRL_BASE       0xF8000B00
#define EYS3D_ROMTER_ID       0x0053554E
#define DEFAULT_READ_ID_CMD     0x9F

/* spi_ctrl */
#define SPI_CLK_DIV_MASK        (0x7<<16)
#define SPI_CLK_D_2             1
#define SPI_CLK_D_4             2
#define SPI_CLK_D_6             3
#define SPI_CLK_D_8             4
#define SPI_CLK_D_16            5
#define SPI_CLK_D_24            6
#define SPI_CLK_D_32            7

#define CLEAR_CUST_CMD          (~0xffff)
#define CUST_CMD(x)             (x<<8)
#define SPI_CTRL_BUSY           (1<<31)

/* spi_auto_cfg */
#define PIO_TRIGGER             (1<<21)

enum SPI_PIO_DATA_BYTE
{
	BYTE_0 = 0<<4,
	BYTE_1 = 1<<4,
	BYTE_2 = 2<<4,
	BYTE_3 = 3<<4,
	BYTE_4 = 4<<4
};

enum SPI_PIO_CMD
{
	CMD_READ = 0<<2,
	CMD_WRITE = 1<<2
};

enum SPI_PIO_ADDRESS_BYTE
{
	ADDR_0B = 0,
	ADDR_1B = 1,
	ADDR_2B = 2,
	ADDR_3B = 3
};


typedef volatile struct {
	// Group 022 : SPI_FLASH
	unsigned int  spi_ctrl;
	unsigned int  spi_timing;
	unsigned int  spi_page_addr;
	unsigned int  spi_data;
	unsigned int  spi_status;
	unsigned int  spi_auto_cfg;
	unsigned int  spi_cfg0;
	unsigned int  spi_cfg1;
	unsigned int  spi_cfg2;
	unsigned int  spi_data64;
	unsigned int  spi_buf_addr;
	unsigned int  spi_status_2;
	unsigned int  spi_err_status;
	unsigned int  spi_mem_data_addr;
	unsigned int  spi_mem_parity_addr;
	unsigned int  spi_col_addr;
	unsigned int  spi_bch;
	unsigned int  spi_intr_msk;
	unsigned int  spi_intr_sts;
	unsigned int  spi_page_size;
	unsigned int  G22_RESERVED[12];
} SPI_CTRL;

unsigned int SPI_nor_read_id(unsigned char cmd)
{
	SPI_CTRL *spi_reg = (SPI_CTRL *)map_sysmem(SPI_NOR_CTRL_BASE,0x80);
	unsigned int ctrl;
	unsigned int id;

	dev_dbg("%s\n", __FUNCTION__);

	// Setup Read JEDEC ID command.
	ctrl = spi_reg->spi_ctrl & CLEAR_CUST_CMD;
	ctrl = ctrl | CMD_READ | BYTE_3 | ADDR_0B | CUST_CMD(cmd);
	while ((spi_reg->spi_ctrl & SPI_CTRL_BUSY) != 0) {
		dev_dbg("wait spi_reg->spi_ctrl = 0x%08x\n", spi_reg->spi_ctrl);
	}
	spi_reg->spi_ctrl = ctrl;

	// Issue PIO command.
	spi_reg->spi_auto_cfg |= PIO_TRIGGER;
	while ((spi_reg->spi_auto_cfg & PIO_TRIGGER) != 0) {
		dev_dbg("wait PIO_TRIGGER\n");
	}
	dev_dbg("spi_reg->spi_data = 0x%08x\n", spi_reg->spi_data);
	id = spi_reg->spi_data;

	unmap_sysmem((void*)spi_reg);
	return (id&0xff0000) | ((id&0xff00)>>8) | ((id&0xff)<<8);
}

void SPI_nor_set_clk_div(int clkd)
{
	SPI_CTRL *spi_reg = (SPI_CTRL *)map_sysmem(SPI_NOR_CTRL_BASE,0x80);
	unsigned int ctrl;

	// Set clock divisor.
	ctrl = spi_reg->spi_ctrl & ~SPI_CLK_DIV_MASK;
	ctrl = ctrl | clkd << 16;
	while ((spi_reg->spi_ctrl & SPI_CTRL_BUSY) != 0) {
		dev_dbg("wait spi_reg->spi_ctrl = 0x%08x\n", spi_reg->spi_ctrl);
	}
	spi_reg->spi_ctrl = ctrl;
	dev_dbg("spi_reg->spi_ctrl = 0x%08x\n", spi_reg->spi_ctrl);

	unmap_sysmem((void*)spi_reg);
}

void SPI_nor_restore_cfg2(void)
{
	SPI_CTRL *spi_reg = (SPI_CTRL *)map_sysmem(SPI_NOR_CTRL_BASE,0x80);

	spi_reg->spi_cfg2 = 0x00150095; // restore default after seeting spi_ctrl

	unmap_sysmem((void*)spi_reg);
}

void SPI_nor_speed_up_clk(void)
{
	unsigned int id;

	id = SPI_nor_read_id(DEFAULT_READ_ID_CMD);
	printf("SPI:   Manufacturer id = 0x%02X, Device id = 0x%04X ", id>>16, id&0xffff);

	if ((id != EYS3D_ROMTER_ID) && (id != 0) && (id != 0xFFFFFF)) {
		printf("\n");
#if defined(CONFIG_SYS_ENV_ZEBU)
		SPI_nor_set_clk_div(SPI_CLK_D_2);
#else
		SPI_nor_set_clk_div(SPI_CLK_D_8);
#endif
	} else {
		if (id == EYS3D_ROMTER_ID)
			printf("(eYs3D romter)\n");
		else
			printf("\n");
		//SPI_nor_set_clk_div(SPI_CLK_D_16);
	}

	SPI_nor_restore_cfg2();
}

#endif
