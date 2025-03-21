#include "compiler.h"

enum {
	MODE_GEN_INFO,
	MODE_GEN_DATA
};

enum {
	FILE_TYPE_UNKNOWN,
	FILE_TYPE_8BPP_BMP,
	FILE_TYPE_8BPP_RLE8_BMP,
	FILE_TYPE_16BPP_BMP,
	FILE_TYPE_24BPP_BMP,
	FILE_TYPE_32BPP_BMP,
	FILE_TYPE_GZIP,
	FILE_TYPE_MAX
};

typedef struct bitmap_s {		/* bitmap description */
	uint32_t file_size;
	uint16_t width;
	uint16_t height;
	uint16_t hdr_size;
	uint16_t planes;
	uint16_t bpix;
	uint16_t compres;
	uint8_t	palette[256*4];
	uint8_t	*data;
} bitmap_t;

#define DEFAULT_CMAP_SIZE	0	/* size of default color map	*/
#define LOGO_DATA_ALIGN		16	/* logo data align */

void usage(const char *prog)
{
	fprintf(stderr, "Usage: %s [--gen-info|--gen-data] file\n", prog);
}

/*
 * Neutralize little endians.
 */
uint16_t le_short(uint16_t x)
{
    uint16_t val;
    uint8_t *p = (uint8_t *)(&x);

    val =  (*p++ & 0xff) << 0;
    val |= (*p & 0xff) << 8;

    return val;
}

void skip_bytes (FILE *fp, int n)
{
	while (n-- > 0)
		fgetc (fp);
}

__attribute__ ((__noreturn__))
int error (char * msg, FILE *fp)
{
	fprintf (stderr, "ERROR: %s\n", msg);

	fclose (fp);

	exit (EXIT_FAILURE);
}

void gen_info(FILE *fp, bitmap_t *b, uint16_t n_colors, uint32_t ffile_size, int ffile_type)
{

	printf("/*\n"
		" * Automatically generated by \"tools/dm_video_bmp_logo_ecv5546\"\n"
		" *\n"
		" * DO NOT EDIT\n"
		" *\n"
		" */\n\n\n"
		"#ifndef __eCV5546_BMP_LOGO_H__\n"
		"#define __eCV5546_BMP_LOGO_H__\n\n");

	if (ffile_type == FILE_TYPE_GZIP) {
		printf("/* FILE_TYPE_GZIP */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\t/*depend*/\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\t/*depend*/\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\t/*depend*/\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\t/*depend*/\n\n",
			ffile_size, 250, 100, 0, 0);
	}
	else if (ffile_type == FILE_TYPE_8BPP_BMP) {
		printf("/* FILE_TYPE_8BPP_BMP */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\n\n",
			ffile_size, b->width, b->height, 0, 0);
	}
	else if (ffile_type == FILE_TYPE_8BPP_RLE8_BMP) {
		printf("/* FILE_TYPE_8BPP_RLE8_BMP */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\n\n",
			ffile_size, b->width, b->height, 0, 0);
	}
	else if (ffile_type == FILE_TYPE_16BPP_BMP) {
		printf("/* FILE_TYPE_16BPP_BMP */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\n\n",
			ffile_size, b->width, b->height, 0, 0);
	}	
	else if (ffile_type == FILE_TYPE_24BPP_BMP) {
		printf("/* FILE_TYPE_24BPP_BMP */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\n\n",
			ffile_size, b->width, b->height, 0, 0);
	}
	else if (ffile_type == FILE_TYPE_32BPP_BMP) {
		printf("/* FILE_TYPE_32BPP_BMP */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\n\n",
			ffile_size, b->width, b->height, 0, 0);
	}		
	else {
		printf("/* FILE_TYPE_UNKNOWN or FILE_TYPE_MAX */\n");
		printf("#define BMP_LOGO_FILE_SIZE\t\t\t\t%d\n"
			"#define eCV5546_LOGO_BMP_WIDTH\t\t\t%d\t/*depend*/\n"
			"#define eCV5546_LOGO_BMP_HEIGHT\t\t\t%d\t/*depend*/\n"
			"#define eCV5546_LOGO_BMP_X_OFFSET\t\t%d\t/*depend*/\n"
			"#define eCV5546_LOGO_BMP_Y_OFFSET\t\t%d\t/*depend*/\n\n",
			ffile_size, 0, 0, 0, 0);
	}

	printf ("#endif /* __eCV5546_BMP_LOGO_H__ */\n");

}

void gen_data(FILE *fp, bitmap_t *b, uint16_t n_colors, uint32_t ffile_size, int ffile_type)
{
	unsigned char temp;
	int i;

	printf("/*\n"
		" * Automatically generated by \"tools/dm_video_bmp_logo_ecv5546\"\n"
		" *\n"
		" * DO NOT EDIT\n"
		" *\n"
		" */\n\n\n"
		"#ifndef __eCV5546_BMP_LOGO_DATA_H__\n"
		"#define __eCV5546_BMP_LOGO_DATA_H__\n\n");

	if (ffile_type == FILE_TYPE_GZIP)
		printf("/* FILE_TYPE_GZIP */\n");
	else if (ffile_type == FILE_TYPE_8BPP_RLE8_BMP)
		printf("/* FILE_TYPE_8BPP_RLE8_BMP */\n");
	else if (ffile_type == FILE_TYPE_8BPP_BMP)
		printf("/* FILE_TYPE_8BPP_BMP */\n");
	else if (ffile_type == FILE_TYPE_16BPP_BMP)
		printf("/* FILE_TYPE_16BPP_BMP */\n");
	else if (ffile_type == FILE_TYPE_24BPP_BMP)
		printf("/* FILE_TYPE_24BPP_BMP */\n");
	else if (ffile_type == FILE_TYPE_32BPP_BMP)
		printf("/* FILE_TYPE_32BPP_BMP */\n");
	else {
		printf("/* FILE_TYPE_UNKNOWN or FILE_TYPE_MAX */\n");
	}

		/* seek to offset indicated by file header */
		fseek(fp, (long)0, SEEK_SET);

		printf("unsigned char ecv5546_uboot_logo[] = {\n");

		for (i=0; i<(ffile_size); ++i) {
			if (fread (&temp, sizeof (uint8_t), 1, fp) != 1)
				error ("Input file read fail", fp);

			if ((i%LOGO_DATA_ALIGN) == 0)
				putchar ('\t');
			printf ("0x%02X,%c",
				temp,
				((i%LOGO_DATA_ALIGN) == (LOGO_DATA_ALIGN-1)) ? '\n' : ' '
			);
		}

	printf ("\n"
		"};\n\n"
		"#endif /* __eCV5546_BMP_LOGO_DATA_H__ */\n"
	);
}

int main (int argc, char *argv[])
{
	int	mode;
	uint32_t file_size;
	FILE	*fp;
	bitmap_t bmp;
	bitmap_t *b = &bmp;
	uint16_t data_offset, n_colors;

	if (argc < 3) {
		usage(argv[0]);
		exit (EXIT_FAILURE);
	}

	if (!strcmp(argv[1], "--gen-info"))
		mode = MODE_GEN_INFO;
	else if (!strcmp(argv[1], "--gen-data"))
		mode = MODE_GEN_DATA;
	else {
		usage(argv[0]);
		exit(EXIT_FAILURE);
	}

	fp = fopen(argv[2], "rb");
	if (!fp) {
		perror(argv[2]);
		exit (EXIT_FAILURE);
	}

	//Get input file size
	fseek(fp, 0, SEEK_END);
	file_size = ftell(fp);
	fseek(fp, 0, SEEK_SET);

	//Transfer Gzip file to text file
	if (fgetc (fp) == 0x1f || fgetc (fp) == 0x8b) {
		if (mode == MODE_GEN_INFO)
			gen_info(fp, b, 0, file_size, FILE_TYPE_GZIP);
		else /*if (mode == MODE_GEN_DATA)*/
			gen_data(fp, b, 0, file_size, FILE_TYPE_GZIP);
		goto out;
	}

	fseek(fp, 0, SEEK_SET);

	if (fgetc (fp) != 'B' || fgetc (fp) != 'M')
		error ("Input file is not a bitmap", fp);
	/*
	 * read width and height of the image, and the number of colors used;
	 * ignore the rest
	 */
	if (fread (&b->file_size, sizeof (uint32_t), 1, fp) != 1)
		error ("Couldn't read bitmap file size", fp);

	skip_bytes (fp, 4);
	if (fread (&data_offset, sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap data offset", fp);

	skip_bytes(fp, 2);
	if (fread(&b->hdr_size,   sizeof(uint16_t), 1, fp) != 1)
		error("Couldn't read bitmap header size", fp);

	if (b->hdr_size < 40)
		error("Invalid bitmap header", fp);

	skip_bytes(fp, 2);
	if (fread (&b->width,   sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap width", fp);

	skip_bytes (fp, 2);
	if (fread (&b->height,  sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap height", fp);

	skip_bytes (fp, 2);
	if (fread (&b->planes, sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap planed", fp);	

	if (fread (&b->bpix, sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap bpix", fp);

	if (fread (&b->compres, sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap compress", fp);

	skip_bytes (fp, 14);
	if (fread (&n_colors, sizeof (uint16_t), 1, fp) != 1)
		error ("Couldn't read bitmap colors", fp);
	skip_bytes(fp, b->hdr_size - 34);

	/*
	 * Repair endianess.
	 */
	data_offset = le_short(data_offset);
	b->width = le_short(b->width);
	b->height = le_short(b->height);
	b->bpix = le_short(b->bpix);
	n_colors = le_short(n_colors);

	/* In case of 8-bit file */
	if ((n_colors == 0) || (n_colors > 256 - DEFAULT_CMAP_SIZE)) {
		/* reserve DEFAULT_CMAP_SIZE color map entries for default map */
		n_colors = 256 - DEFAULT_CMAP_SIZE;
	}

	/* Create define */
	if (mode == MODE_GEN_INFO) {
		if((b->bpix == 8) && (b->compres == 1))
			gen_info(fp, b, n_colors, file_size, FILE_TYPE_8BPP_RLE8_BMP);
		else if((b->bpix == 8) && (b->compres == 0))
			gen_info(fp, b, n_colors, file_size, FILE_TYPE_8BPP_BMP);
		else if(b->bpix == 16)
			gen_info(fp, b, 0, file_size, FILE_TYPE_16BPP_BMP);		
		else if((b->bpix == 24) && (b->compres == 0))
			gen_info(fp, b, 0, file_size, FILE_TYPE_24BPP_BMP);
		else if(b->bpix == 32)
			gen_info(fp, b, 0, file_size, FILE_TYPE_32BPP_BMP);
		else
			gen_info(fp, b, 0, file_size, FILE_TYPE_MAX);
		goto out;
	}

	/* Create data */
	if (mode == MODE_GEN_DATA) {
		if((b->bpix == 8) && (b->compres == 1))
			gen_data(fp, b, n_colors, file_size, FILE_TYPE_8BPP_RLE8_BMP);
		else if((b->bpix == 8) && (b->compres == 0))
			gen_data(fp, b, n_colors, file_size, FILE_TYPE_8BPP_BMP);
		else if(b->bpix == 16)
			gen_data(fp, b, 0, file_size, FILE_TYPE_16BPP_BMP);		
		else if((b->bpix == 24) && (b->compres == 0))
			gen_data(fp, b, 0, file_size, FILE_TYPE_24BPP_BMP);
		else if(b->bpix == 32)
			gen_data(fp, b, 0, file_size, FILE_TYPE_32BPP_BMP);
		else
			gen_data(fp, b, 0, file_size, FILE_TYPE_MAX);
		goto out;
	}

out:
	fclose(fp);
	return 0;
}
