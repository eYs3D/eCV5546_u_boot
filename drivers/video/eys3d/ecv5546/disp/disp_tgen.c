// SPDX-License-Identifier: GPL-2.0+
/*
 * Author(s): Hammer Hsieh <eys3d@eys3d.com>
 */

#include <common.h>
#include "reg_disp.h"
#include "disp_tgen.h"
#include "display2.h"
/**************************************************************************
 *             F U N C T I O N    I M P L E M E N T A T I O N S           *
 **************************************************************************/
void DRV_TGEN_Init(int width, int height)
{
	//printf("DRV_TGEN_Init w %d h %d\n", width, height);

	G197_TGEN_REG->sft_cfg[0] = 0x00000000;
	G197_TGEN_REG->sft_cfg[2] = 0x0000000a;
	G197_TGEN_REG->sft_cfg[3] = 0x0000000a;

	if ( (width == 720) && (height == 480) ) {
		//G197_TGEN_REG->sft_cfg[4] = 0x00000000;

		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		G197_TGEN_REG->sft_cfg[8] = 0x0000035a;
		G197_TGEN_REG->sft_cfg[9] = 0x000002d0;
		G197_TGEN_REG->sft_cfg[10] = 0x0000020d;
		G197_TGEN_REG->sft_cfg[11] = 0x00000205;
		G197_TGEN_REG->sft_cfg[12] = 0x00000024;
	} else if ( (width == 800) && (height == 480) ) {
#if CONFIG_IS_ENABLED(DM_I2C) && defined(CONFIG_eCV5546_RASPBERRYPI_DSI_PANEL)
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		#if 1 //update for sync drm setting
		G197_TGEN_REG->sft_cfg[8] = 0x00000395;
		G197_TGEN_REG->sft_cfg[9] = 0x00000320;
		G197_TGEN_REG->sft_cfg[10] = 0x000001fe;
		G197_TGEN_REG->sft_cfg[11] = 0x000001f8;
		G197_TGEN_REG->sft_cfg[12] = 0x00000017;
		#else
		G197_TGEN_REG->sft_cfg[8] = 0x00000369;
		G197_TGEN_REG->sft_cfg[9] = 0x00000320;
		G197_TGEN_REG->sft_cfg[10] = 0x000001fe;
		G197_TGEN_REG->sft_cfg[11] = 0x000001f8;
		G197_TGEN_REG->sft_cfg[12] = 0x00000017;
		#endif
#else
	;//TBD
#endif
	} else if ( (width == 1024) && (height == 600) ) {
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		G197_TGEN_REG->sft_cfg[8] = 0x00000540;
		G197_TGEN_REG->sft_cfg[9] = 0x00000400;
		G197_TGEN_REG->sft_cfg[10] = 0x0000027b;
		G197_TGEN_REG->sft_cfg[11] = 0x00000271;
		G197_TGEN_REG->sft_cfg[12] = 0x00000018;
	} else if ( (width == 480) && (height == 1280) ) {
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		#if 1 //update for sync drm setting
		G197_TGEN_REG->sft_cfg[8] = 0x000002a6;
		G197_TGEN_REG->sft_cfg[9] = 0x000001e0;
		G197_TGEN_REG->sft_cfg[10] = 0x00000520;
		G197_TGEN_REG->sft_cfg[11] = 0x00000511;
		G197_TGEN_REG->sft_cfg[12] = 0x00000010;
		#else
		G197_TGEN_REG->sft_cfg[8] = 0x0000026c;
		G197_TGEN_REG->sft_cfg[9] = 0x000001e0;
		G197_TGEN_REG->sft_cfg[10] = 0x00000522;
		G197_TGEN_REG->sft_cfg[11] = 0x00000512;
		G197_TGEN_REG->sft_cfg[12] = 0x00000011;
		#endif
	} else if ( (width == 240) && (height == 320) ) {
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		#if 1 //fine tune for screen flicker
		G197_TGEN_REG->sft_cfg[8] = 0x000002AB;
		G197_TGEN_REG->sft_cfg[9] = 0x000000F0;
		G197_TGEN_REG->sft_cfg[10] = 0x00000162;
		G197_TGEN_REG->sft_cfg[11] = 0x0000015B;
		G197_TGEN_REG->sft_cfg[12] = 0x000001A;
		#else
		G197_TGEN_REG->sft_cfg[8] = 0x000002AB;
		G197_TGEN_REG->sft_cfg[9] = 0x000000F0;
		G197_TGEN_REG->sft_cfg[10] = 0x0000016C;
		G197_TGEN_REG->sft_cfg[11] = 0x00000165;
		G197_TGEN_REG->sft_cfg[12] = 0x0000024;
		#endif
	} else if ( (width == 1280) && (height == 480) ) {
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		G197_TGEN_REG->sft_cfg[8] = 0x00000612;
		G197_TGEN_REG->sft_cfg[9] = 0x00000500;
		G197_TGEN_REG->sft_cfg[10] = 0x0000020d;
		G197_TGEN_REG->sft_cfg[11] = 0x00000205;
		G197_TGEN_REG->sft_cfg[12] = 0x00000024;
	} else if ( (width == 1280) && (height == 720) ) {
		//G197_TGEN_REG->sft_cfg[4] = 0x00000200;

		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		G197_TGEN_REG->sft_cfg[8] = 0x00000672;
		G197_TGEN_REG->sft_cfg[9] = 0x00000500;
		G197_TGEN_REG->sft_cfg[10] = 0x000002EE;
		G197_TGEN_REG->sft_cfg[11] = 0x000002EA;
		G197_TGEN_REG->sft_cfg[12] = 0x00000019;

	} else if ( (width == 1920) && (height == 1080) ) {
		//G197_TGEN_REG->sft_cfg[4] = 0x00000300;

		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		G197_TGEN_REG->sft_cfg[8] = 0x00000898;
		G197_TGEN_REG->sft_cfg[9] = 0x00000780;
		G197_TGEN_REG->sft_cfg[10] = 0x00000465;
		G197_TGEN_REG->sft_cfg[11] = 0x00000462;
		G197_TGEN_REG->sft_cfg[12] = 0x00000029;
	} else if ( (width == 3840) && (height == 2880) ) {
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //user mode

		G197_TGEN_REG->sft_cfg[8] = 0x00001200;
		G197_TGEN_REG->sft_cfg[9] = 0x00000F00;
		G197_TGEN_REG->sft_cfg[10] = 0x00000c80;
		G197_TGEN_REG->sft_cfg[11] = 0x00000b68;
		G197_TGEN_REG->sft_cfg[12] = 0x00000027;
	} else {
		printf("DRV_TGEN_Init TBD\n");
	}

	G197_TGEN_REG->sft_cfg[23] = 0x0000100d;
	G197_TGEN_REG->sft_cfg[29] = 0x00000000;
	G197_TGEN_REG->sft_cfg[1] = 0x00000001;

}

void DRV_TGEN_Set(DRV_VideoFormat_e fmt, DRV_FrameRate_e fps)
{
	u32 value;

	value = G197_TGEN_REG->sft_cfg[4];
	value &= ~0x00000730;
	if (fmt == DRV_FMT_USER_MODE) {
		G197_TGEN_REG->sft_cfg[4] = 0x00000001; //Set to USER_MODE
	} else {
		G197_TGEN_REG->sft_cfg[4] = ((fmt & 0x7) << 8) | ((fps & 0x3) << 4);
	}
}

int DRV_TGEN_Adjust(DRV_TGEN_Input_e Input, UINT32 Adjust)
{
	switch (Input) {
	case DRV_TGEN_VPP0:
		G197_TGEN_REG->sft_cfg[23] = (G197_TGEN_REG->sft_cfg[23] & ~(0x3F<<8)) | ((Adjust & 0x3F) << 8);
		break;
	case DRV_TGEN_OSD0:
		G197_TGEN_REG->sft_cfg[25] = (G197_TGEN_REG->sft_cfg[25] & ~(0x3F<<0)) | ((Adjust & 0x3F) << 0);
		break;
	case DRV_TGEN_OSD1:
		G197_TGEN_REG->sft_cfg[25] = (G197_TGEN_REG->sft_cfg[25] & ~(0x3F<<8)) | ((Adjust & 0x3F) << 8);
		break;
	case DRV_TGEN_OSD2:
		G197_TGEN_REG->sft_cfg[24] = (G197_TGEN_REG->sft_cfg[24] & ~(0x3F<<0)) | ((Adjust & 0x3F) << 0);
		break;
	case DRV_TGEN_OSD3:
		G197_TGEN_REG->sft_cfg[24] = (G197_TGEN_REG->sft_cfg[24] & ~(0x3F<<8)) | ((Adjust & 0x3F) << 8);
		break;
	case DRV_TGEN_PTG:
		G197_TGEN_REG->sft_cfg[26] = (G197_TGEN_REG->sft_cfg[26] & ~(0x3F<<8)) | ((Adjust & 0x3F) << 8);
		break;
	default:
		printf("Invalidate Input %d\n", Input);
		return DRV_ERR_INVALID_PARAM;
	}

	return DRV_SUCCESS;
}

