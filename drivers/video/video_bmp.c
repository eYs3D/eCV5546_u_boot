// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2015 Google, Inc
 */

#include <common.h>
#include <cpu_func.h>
#include <bmp_layout.h>
#include <dm.h>
#include <log.h>
#include <mapmem.h>
#include <splash.h>
#include <video.h>
#include <watchdog.h>
#include <asm/unaligned.h>

//#define debug printf

#define DISP_SWAP16(x)		((((x) & 0x00ff) << 8) | \
				  ((x) >> 8) \
				)
#define DISP_SWAP32(x)		((((x) & 0x000000ff) << 24) | \
				 (((x) & 0x0000ff00) <<  8) | \
				 (((x) & 0x00ff0000) >>  8) | \
				 (((x) & 0xff000000) >> 24)   \
				)

#if defined(CONFIG_VIDEO_eCV5546) && defined(CONFIG_VIDEO_LOGO)
#define VIDEO_CMAP_OFFSET 0
#else
#define VIDEO_CMAP_OFFSET 16
#endif
#define BMP_RLE8_ESCAPE		0
#define BMP_RLE8_EOL		0
#define BMP_RLE8_EOBMP		1
#define BMP_RLE8_DELTA		2

/**
 * get_bmp_col_16bpp() - Convert a colour-table entry into a 16bpp pixel value
 *
 * Return: value to write to the 16bpp frame buffer for this palette entry
 */
static uint get_bmp_col_16bpp(struct bmp_color_table_entry cte, enum video_format eformat)
{
	u8 alpha = 0xff;

	if (eformat == VIDEO_FMT_RGB565)
		return ((cte.red << 8) & 0xf800) |
		((cte.green << 3) & 0x07e0) |
			((cte.blue >> 3) & 0x001f);
	else if (eformat == VIDEO_FMT_ARGB1555)
		return ((cte.red << 7) & 0x7c00) |
			((cte.green << 2) & 0x03e0) |
			((cte.blue >> 3) & 0x001f) |
			((alpha << 8) & 0x8000);
			//((cte.reserved << 8) & 0x8000);
	else if (eformat == VIDEO_FMT_RGBA4444)
		return ((cte.red << 8) & 0xf000) |
			((cte.green << 4) & 0x0f00) |
			((cte.blue >> 0) & 0x00f0) |
			((alpha << 8) & 0x000f);
			//((cte.reserved << 8) & 0x000f);
	else if (eformat == VIDEO_FMT_ARGB4444)
		return ((cte.red << 4) & 0x0f00) |
			((cte.green << 0) & 0x00f0) |
			((cte.blue >> 4) & 0x000f) |
			((alpha << 8) & 0xf000);
			//((cte.reserved << 8) & 0xf000);
	else /* default 16bit use RGB565 */
		return ((cte.red << 8) & 0xf800) |
			((cte.green << 3) & 0x07e0) |
			((cte.blue >> 3) & 0x001f);

}

/**
 * write_bmp_data_to_fb() - Write a pixel from a BMP image into the framebuffer
 *
 * This handles frame buffers with 8, 16, 24 or 32 bits per pixel
 *
 * @fb: Place in frame buffer to update
 * @bpix: Frame buffer bits-per-pixel, which controls how many bytes are written
 * @palette: BMP palette table
 * @bmap: Pointer to BMP bitmap position to write. This contains a single byte
 *	which is either written directly (bpix == 8) or used to look up the
 *	palette to get a colour to write
 */

static void write_bmp_data_to_fb(u8 *fb, uint bpix, enum video_format eformat,
		       struct bmp_color_table_entry *palette, u8 *bmap)
{
	struct bmp_color_table_entry *cte = &palette[*bmap];

	if (bpix == 8) {
		if (eformat == VIDEO_FMT_8BPP_ARGB)
			*fb++ = (*bmap) + VIDEO_CMAP_OFFSET;
		else
			*fb++ = (*bmap);

	} else if (bpix == 16) {
		*(u16 *)fb = get_bmp_col_16bpp(palette[*bmap], eformat);

	} else if (bpix == 24) {
			*fb++ = cte->red;
			*fb++ = cte->green;
			*fb++ = cte->blue;
	} else if (bpix == 32) {
		if (eformat == VIDEO_FMT_ARGB8888) {
			*fb++ = cte->blue;
			*fb++ = cte->green;
			*fb++ = cte->red;
			*fb++ = 0xff;
		} else if (eformat == VIDEO_FMT_RGBA8888) {
			*fb++ = 0xff;
			*fb++ = cte->blue;
			*fb++ = cte->green;
			*fb++ = cte->red;
		} else {
			*fb++ = cte->blue;
			*fb++ = cte->green;
			*fb++ = cte->red;
			*fb++ = 0x00;
		}
	} else
		printf("%s unknown bpix!\n", __func__);

}

static void draw_unencoded_bitmap(u8 **fbp, uint bpix,
				  enum video_format eformat, uchar *bmap,
				  struct bmp_color_table_entry *palette,
				  int cnt)
{
	u8 *fb = *fbp;

	while (cnt > 0) {
		write_bmp_data_to_fb(fb, bpix, eformat, palette, bmap++);
		fb += bpix / 8;
		cnt--;
	}
	*fbp = fb;
}

static void draw_encoded_bitmap(u8 **fbp, uint bpix, enum video_format eformat,
				struct bmp_color_table_entry *palette, u8 *bmap,
				int cnt)
{
	u8 *fb = *fbp;

	while (cnt > 0) {
		write_bmp_data_to_fb(fb, bpix, eformat, palette, bmap);
		fb += bpix / 8;
		cnt--;
	}
	*fbp = fb;
}

static void video_display_rle8_bitmap(struct udevice *dev,
				      struct bmp_image *bmp, uint bpix,
				      struct bmp_color_table_entry *palette, u32 *cmap,
				      uchar *fb, int x_off, int y_off,
				      ulong width, ulong height)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);
	uchar *bmap;
	ulong cnt, runlen;
	int x, y;
	int decode = 1;
	uint bytes_per_pixel = bpix / 8;
	enum video_format eformat = priv->format;

	debug("%s\n", __func__);
	bmap = (uchar *)bmp + get_unaligned_le32(&bmp->header.data_offset);

	x = 0;
	y = height - 1;

	while (decode) {
		if (bmap[0] == BMP_RLE8_ESCAPE) {
			switch (bmap[1]) {
			case BMP_RLE8_EOL:
				/* end of line */
				bmap += 2;
				x = 0;
				y--;
				fb -= width * bytes_per_pixel +
					priv->line_length;
				break;
			case BMP_RLE8_EOBMP:
				/* end of bitmap */
				decode = 0;
				break;
			case BMP_RLE8_DELTA:
				/* delta run */
				x += bmap[2];
				y -= bmap[3];
				fb = (uchar *)(priv->fb +
					(y + y_off - 1) * priv->line_length +
					(x + x_off) * bytes_per_pixel);
				bmap += 4;
				break;
			default:
				/* unencoded run */
				runlen = bmap[1];
				bmap += 2;
				if (y < height) {
					if (x < width) {
						if (x + runlen > width)
							cnt = width - x;
						else
							cnt = runlen;
						draw_unencoded_bitmap(
							&fb, bpix, eformat,
							bmap, palette, cnt);
					}
					x += runlen;
				}
				bmap += runlen;
				if (runlen & 1)
					bmap++;
			}
		} else {
			/* encoded run */
			if (y < height) {
				runlen = bmap[0];
				if (x < width) {
					/* aggregate the same code */
					while (bmap[0] == 0xff &&
					       bmap[2] != BMP_RLE8_ESCAPE &&
					       bmap[1] == bmap[3]) {
						runlen += bmap[2];
						bmap += 2;
					}
					if (x + runlen > width)
						cnt = width - x;
					else
						cnt = runlen;
					draw_encoded_bitmap(&fb, bpix, eformat,
							    palette, &bmap[1],
							    cnt);
				}
				x += runlen;
			}
			bmap += 2;
		}
	}
}

/**
 * video_splash_align_axis() - Align a single coordinate
 *
 *- if a coordinate is 0x7fff then the image will be centred in
 *  that direction
 *- if a coordinate is -ve then it will be offset to the
 *  left/top of the centre by that many pixels
 *- if a coordinate is positive it will be used unchnaged.
 *
 * @axis:	Input and output coordinate
 * @panel_size:	Size of panel in pixels for that axis
 * @picture_size:	Size of bitmap in pixels for that axis
 */
static void video_splash_align_axis(int *axis, unsigned long panel_size,
				    unsigned long picture_size)
{
	long panel_picture_delta = panel_size - picture_size;
	long axis_alignment;

	if (*axis == BMP_ALIGN_CENTER)
		axis_alignment = panel_picture_delta / 2;
	else if (*axis < 0)
		axis_alignment = panel_picture_delta + *axis + 1;
	else
		return;

	*axis = max(0, (int)axis_alignment);
}

void video_bmp_get_info(void *bmp_image, ulong *widthp, ulong *heightp, uint *bpixp)
{
	struct bmp_image *bmp = bmp_image;

	*widthp = get_unaligned_le32(&bmp->header.width);
	*heightp = get_unaligned_le32(&bmp->header.height);
	*bpixp = get_unaligned_le16(&bmp->header.bit_count);
}

static void video_set_cmap(struct udevice *dev,
			   struct bmp_color_table_entry *cte, unsigned colours, int color_used, int pal_sel)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);
	int i;
	u32 *cmap = priv->cmap;

	debug("%s: colours=%d, color_used=%d, cmap_ofs=%d pal_sel=%d\n", __func__,
		colours, color_used, VIDEO_CMAP_OFFSET, pal_sel);

	if (pal_sel) {
		debug("set grey scale palette! \n");
	}
	else {
		debug("load bmp palette! \n");
	}

	cmap += VIDEO_CMAP_OFFSET;
	for (i = VIDEO_CMAP_OFFSET; i < colours; ++i) {
		if (pal_sel) {
			//Palette is ARGB8888 with 256 grey scale
			if (i < VIDEO_CMAP_OFFSET) {
				;//*cmap = 0x000000ff;
				//*cmap = 0x00000032; alpha value modify
			} else if (i > 235) {
				*cmap = 0xffffffff;
				//*cmap = 0xffffff32; alpha value modify
			} else {
				//*cmap = (((u32)(0x32) << 0) | alpha value modify
				*cmap = (((u32)(0xff) << 0) |
						((u32)((u8)(1164*(i-16)/1000)) << 8) |
						((u32)((u8)(1164*(i-16)/1000)) << 16) |
						((u32)((u8)(1164*(i-16)/1000)) << 24));
			}
		}
		else {
			//Palette is ARGB8888 with 256 color
			if (i >= (VIDEO_CMAP_OFFSET + color_used))
				//*cmap = 0x00000032; alpha value modify
				*cmap = 0x000000ff;
			else
				//*cmap = (((u32)(0x32) << 0) | alpha value modify
				*cmap = (((u32)(0xff) << 0) |
						((u32)(cte->red) << 8) |
						((u32)(cte->green) << 16) |
						((u32)(cte->blue) << 24));
		}
		cmap++;
		cte++;
	}
	flush_cache((ulong)priv->cmap, 1024);

	#if 0 //dump palette for debug
	cmap = priv->cmap;
	debug("dump palette!\n");
	for (i = 0; i < colours/4; ++i)
		debug("0x%08x 0x%08x 0x%08x 0x%08x\n", *(cmap+i*4+0), *(cmap+i*4+1), *(cmap+i*4+2), *(cmap+i*4+3));
	#endif
}

__weak void fb_put_byte(uchar **fb, uchar **from)
{
	if((*(*from) + VIDEO_CMAP_OFFSET) > 255)
		*(*fb) = *(*from);
	else
		*(*fb) = *(*from) + VIDEO_CMAP_OFFSET;
	(*fb)++;
	(*from)++;
}

static const char * const bmp_data_type[] = {"BMP_BI_RGB", "BMP_BI_RLE8", "BMP_BI_RLE4"};

int video_bmp_display(struct udevice *dev, ulong bmp_image, int x, int y,
		      bool align)
{
	struct video_priv *priv = dev_get_uclass_priv(dev);
	u32 *cmap_base = NULL;
	int i, j;
	uchar *start, *fb;
	struct bmp_image *bmp = map_sysmem(bmp_image, 0);
	uchar *bmap;
	ushort padded_width;
	unsigned long width, height, byte_width;
	unsigned long pwidth = priv->xsize;
	unsigned colours, bpix, bmp_bpix;
	enum video_format eformat;
	struct bmp_color_table_entry *palette;
	int hdr_size;
	unsigned long color_used, data_offset;
	int ret;

	if (!bmp || !(bmp->header.signature[0] == 'B' &&
	    bmp->header.signature[1] == 'M')) {
		printf("Error: no valid bmp image at %lx\n", bmp_image);

		return -EINVAL;
	}

	video_bmp_get_info(bmp, &width, &height, &bmp_bpix);
	hdr_size = get_unaligned_le16(&bmp->header.size);
	data_offset = get_unaligned_le32(&bmp->header.data_offset);
	color_used = get_unaligned_le32(&bmp->header.colors_used);
	debug("[VID_BMP]video_bmp_display data_offset=%ld hdr %d wxh %04ldx%04ld bmp_bpix %02d color_used=%ld\n",
		data_offset, hdr_size, width, height, bmp_bpix, color_used);

	if( (bmp_bpix == 8) && (color_used == 0) && (data_offset > 54) )
		color_used = 0x100 ;//(data_offset - 54) / 4;

	debug("[VID_BMP]video_bmp_display data_offset=%ld hdr %d wxh %04ldx%04ld bmp_bpix %02d color_used=%ld\n",
		data_offset, hdr_size, width, height, bmp_bpix, color_used);

	palette = (void *)bmp + 14 + hdr_size;

	colours = 1 << bmp_bpix;

	bpix = VNBITS(priv->bpix);
	eformat = priv->format;

	if (bpix != 8 && bpix != 16 && bpix != 32) {
		printf("Error: %d bit/pixel mode, but BMP has %d bit/pixel\n",
		       bpix, bmp_bpix);

		return -EINVAL;
	}
	/*
	 * We support displaying 8bpp/24bpp/32bpp BMPs transfer to eCV5546 display engine
	 * and eCV5546 display engine can select 8bpp(palette=ARGB),RGB565,ARGB8888
	 */
	if (bpix != bmp_bpix &&
	    !(bmp_bpix == 8 && bpix == 16) &&
	    !(bmp_bpix == 8 && bpix == 32) &&
			!(bmp_bpix == 24 && bpix == 8) &&
	    !(bmp_bpix == 24 && bpix == 16) &&
			!(bmp_bpix == 24 && bpix == 32) &&
			!(bmp_bpix == 32 && bpix == 8) &&
			!(bmp_bpix == 32 && bpix == 16) &&
			!(bmp_bpix == 32 && bpix == 32)) {

		printf("Error: %d bit/pixel mode, but BMP has %d bit/pixel\n",
		       bpix, colours);
		return -EPERM;
	}

	/* set cmap for 8bpp */
	debug("Load-bmp: (%d bmp_bpix) convert to osd (%d bpix)\n", bmp_bpix, bpix);
	if (bmp_bpix == 8)
		video_set_cmap(dev, palette, colours, color_used, 0);

	if (((bmp_bpix == 32) || (bmp_bpix == 24)) && (bpix == 8)) {
		colours = 0x100;
		video_set_cmap(dev, palette, colours, color_used, 1);
	}

	debug("Display-bmp: %d x %d  with %d colours, bmp_bpix %d\n",
	      (int)width, (int)height, (int)colours, bmp_bpix);

	padded_width = (width & 0x3 ? (width & ~0x3) + 4 : width);

	if (align) {
		video_splash_align_axis(&x, priv->xsize, width);
		video_splash_align_axis(&y, priv->ysize, height);
	}

	if ((x + width) > pwidth)
		width = pwidth - x;
	if ((y + height) > priv->ysize)
		height = priv->ysize - y;

	bmap = (uchar *)bmp + get_unaligned_le32(&bmp->header.data_offset);
	start = (uchar *)(priv->fb +
		(y + height) * priv->line_length + x * bpix / 8);

	/* Move back to the final line to be drawn */
	fb = start - priv->line_length;

	switch (bmp_bpix) {
	case 8:
		cmap_base = priv->cmap;
		if (CONFIG_IS_ENABLED(VIDEO_BMP_RLE8)) {
			u32 compression = get_unaligned_le32(&bmp->header.compression);
			debug("check compressed type=%d(%s)\n", compression, bmp_data_type[compression]);
			if (compression == BMP_BI_RLE8) {
				video_display_rle8_bitmap(dev, bmp, bpix, palette, cmap_base, fb,
							  x, y, width, height);
				break;
			}
		}

		/* Not compressed */
		if (bpix == 8)
			byte_width = width;
		else if (bpix == 16)
			byte_width = width * 2;
		else if (bpix == 24)
			;//eCV5546 didn't support this setting
		else if (bpix == 32)
			byte_width = width * 4;

		for (i = 0; i < height; ++i) {
			schedule();
			for (j = 0; j < width; j++) {
				if (bpix == 8) {
					//fmt is 8bpp(palette=ARGB)
					fb_put_byte(&fb, &bmap);
				} else if (bpix == 16) {
					//fmt is RGB565
					write_bmp_data_to_fb(fb, bpix, eformat, palette, bmap++);
					fb += sizeof(uint16_t) / sizeof(*fb);
				} else if (bpix == 24) {
					;//eCV5546 didn't support this setting
				} else if (bpix == 32) {
					//fmt is ARGB8888
					write_bmp_data_to_fb(fb, bpix, eformat, palette, bmap++);
					fb += sizeof(uint32_t) / sizeof(*fb);
				}

			}
			bmap += (padded_width - width);
			fb -= byte_width + priv->line_length;
		}
		break;
	case 16:
		if (CONFIG_IS_ENABLED(BMP_16BPP)) {
			for (i = 0; i < height; ++i) {
				schedule();
				for (j = 0; j < width; j++) {
					*fb++ = *bmap++;
					*fb++ = *bmap++;
				}
				bmap += (padded_width - width);
				fb -= width * 2 + priv->line_length;
			}
		}
		break;
	case 24:
		if (CONFIG_IS_ENABLED(BMP_24BPP)) {
			if (bpix == 8) {
				debug("BMP RGB888 to OSD 8bpp \n");
			} else if (bpix == 16) {
				debug("BMP RGB888 to OSD 16bpp \n");
			} else if (bpix == 24) {
				debug("BMP RGB888 to OSD 24bpp \n");
				debug("eCV5546 not support! \n");
			} else { //bpix == 32
				debug("BMP RGB888 to OSD 32bpp \n");
			}

			for (i = 0; i < height; ++i) {
				for (j = 0; j < width; j++) {
					if (bpix == 8) {
						/* 8bpp (palette=ARGB) format */
						#if 1
						unsigned int tmp_val24;
						tmp_val24 = ((299 * bmap[2] + 587 * bmap[1] + 114 * bmap[0])/1000);
						if (tmp_val24 < VIDEO_CMAP_OFFSET)
							*(uchar *)fb = (uchar)VIDEO_CMAP_OFFSET;
						else if (tmp_val24 > 0xff)
							*(uchar *)fb = (uchar)0xff;
						else 
							*(uchar *)fb = (uchar)tmp_val24;
						#else
						*(uchar *)fb = (uchar)((299 * bmap[2] + 587 * bmap[1] + 114 * bmap[0])/1000);
						#endif
						bmap += 3;
						fb ++;
					} else if (bpix == 16) {
						/* 16bit 565RGB format */
						*(u16 *)fb = ((bmap[2] >> 3) << 11) |
							((bmap[1] >> 2) << 5) |
							(bmap[0] >> 3);
						bmap += 3;
						fb += 2;
					} else if (bpix == 24) {
						;//eCV5546 didn't support
					} else if (bpix == 32) { //bpix == 32
						if (eformat == VIDEO_X2R10G10B10) {
						u32 pix;

						pix = *bmap++ << 2U;
						pix |= *bmap++ << 12U;
						pix |= *bmap++ << 22U;
						*fb++ = pix & 0xff;
						*fb++ = (pix >> 8) & 0xff;
						*fb++ = (pix >> 16) & 0xff;
						*fb++ = pix >> 24;
					} else if (eformat == VIDEO_RGBA8888) {
						u32 pix;

						pix = *bmap++ << 8U; /* blue */
						pix |= *bmap++ << 16U; /* green */
						pix |= *bmap++ << 24U; /* red */

						*fb++ = (pix >> 24) & 0xff;
						*fb++ = (pix >> 16) & 0xff;
						*fb++ = (pix >> 8) & 0xff;
						*fb++ = 0xff;
					} else {
							*(fb++) = *(bmap++);
							*(fb++) = *(bmap++);
							*(fb++) = *(bmap++);
							*(fb++) = 0xff;
						}
					} else { //undef
						*fb++ = *bmap++;
						*fb++ = *bmap++;
						*fb++ = *bmap++;
						*fb++ = 0;
					}
				}
				fb -= priv->line_length + width * (bpix / 8);
				bmap += (padded_width - width) * 3;
			}
		}
		break;
	case 32:
		if (CONFIG_IS_ENABLED(BMP_32BPP)) {
			for (i = 0; i < height; ++i) {
				for (j = 0; j < width; j++) {
					if (bpix == 8) {
						#if 1 //deal with upper/lower limit
						unsigned int tmp_val32;
						tmp_val32 = ((299 * bmap[2] + 587 * bmap[1] + 114 * bmap[0])/1000);
						if (tmp_val32 < VIDEO_CMAP_OFFSET)
							*(uchar *)fb = (uchar)VIDEO_CMAP_OFFSET;
						else if (tmp_val32 > 0xff)
							*(uchar *)fb = (uchar)0xff;
						else 
							*(uchar *)fb = (uchar)tmp_val32;
						#else 
						*(uchar *)fb = (uchar)((299 * bmap[2] + 587 * bmap[1] + 114 * bmap[0])/1000);
						#endif
						bmap += 4;
						fb ++;
					} else if (bpix == 16) {
						*(u16 *)fb = ((bmap[2] >> 3) << 11) |
							((bmap[1] >> 2) << 5) |
							(bmap[0] >> 3);
						bmap += 4;
						fb += 2;
					} else if (bpix == 24) {
						;//eCV5546 didn't support
					} else { //bpix == 32
					if (eformat == VIDEO_X2R10G10B10) {
						u32 pix;

						pix = *bmap++ << 2U;
						pix |= *bmap++ << 12U;
						pix |= *bmap++ << 22U;
						pix |= (*bmap++ >> 6) << 30U;
						*fb++ = pix & 0xff;
						*fb++ = (pix >> 8) & 0xff;
						*fb++ = (pix >> 16) & 0xff;
						*fb++ = pix >> 24;
					} else if (eformat == VIDEO_RGBA8888) {
						u32 pix;

						pix = *bmap++ << 8U; /* blue */
						pix |= *bmap++ << 16U; /* green */
						pix |= *bmap++ << 24U; /* red */
						bmap++;
						*fb++ = (pix >> 24) & 0xff;
						*fb++ = (pix >> 16) & 0xff;
						*fb++ = (pix >> 8) & 0xff;
						*fb++ = 0xff; /* opacity */
					} else {
							*(fb++) = *(bmap++);
							*(fb++) = *(bmap++);
							*(fb++) = *(bmap++);
							*(fb++) = *(bmap++);
						}
					}
				}
				fb -= priv->line_length + width * (bpix / 8);
				bmap += (padded_width - width) * 4;
			}
		}
		break;
	default:
		break;
	};

	/* Find the position of the top left of the image in the framebuffer */
	fb = (uchar *)(priv->fb + y * priv->line_length + x * bpix / 8);
	ret = video_sync_copy(dev, start, fb);
	if (ret)
		return log_ret(ret);

	return video_sync(dev, false);
}
