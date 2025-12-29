#ifndef __GC030A_CMOS_PARAM_H_
#define __GC030A_CMOS_PARAM_H_

#ifdef __cplusplus
#if __cplusplus
extern "C" {
#endif
#endif

#ifdef ARCH_CV182X
#include <linux/cvi_vip_cif.h>
#include <linux/cvi_vip_snsr.h>
#include "cvi_type.h"
#else
#include <linux/cif_uapi.h>
#include <linux/vi_snsr.h>
#include <linux/cvi_type.h>
#endif
#include "cvi_sns_ctrl.h"
#include "gc030a_cmos_ex.h"

static const GC030A_MODE_S g_astGc030a_mode[GC030A_MODE_NUM] = {
	[GC030A_MODE_640X480P30] = {
		.name = "640X480P30",
		.stImg[0] = {
			.stSnsSize = {
				.u32Width = 640,
				.u32Height = 480,
			},
			.stWndRect = {
				.s32X = 0,
				.s32Y = 0,
				.u32Width = 640,
				.u32Height = 480,
			},
			.stMaxSize = {
				.u32Width = 640,
				.u32Height = 480,
			},
		},
		.f32MaxFps = 30,
		.f32MinFps = 0.92, /* 506 * 30 / 0x3FFF */
		.u32HtsDef = 1579,
		.u32VtsDef = 506,
		.stExp[0] = {
			.u16Min = 1,
			.u16Max = 1268 - 8,
			.u16Def = 100,
			.u16Step = 1,
		},
		.stAgain[0] = {
			.u32Min = 1024,
			.u32Max = 16 * 646,
			.u32Def = 1024,
			.u32Step = 1,
		},
		.stDgain[0] = {
			.u32Min = 1024,
			.u32Max = 1024,
			.u32Def = 1024,
			.u32Step = 1,
		},
	}
};



static ISP_CMOS_BLACK_LEVEL_S g_stIspBlcCalibratio = {
	.bUpdate = CVI_TRUE,
	.blcAttr = {
		.Enable = 1,
		.enOpType = OP_TYPE_AUTO,
		.stManual = {256, 256, 256, 256, 0, 0, 0, 0
#ifdef ARCH_CV182X
		, 1092, 1092, 1092, 1092
#endif
		},
		.stAuto = {
			{256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256 },
			{256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256 },
			{256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256 },
			{256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256, 256 },
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
			{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
#ifdef ARCH_CV182X
			{1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092,
				1092, 1092},
			{1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092,
				1092, 1092},
			{1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092,
				1092, 1092},
			{1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092, 1092,
				1092, 1092},
#endif
		},
	},
};

struct combo_dev_attr_s gc030a_rx_attr = {
	.input_mode = INPUT_MODE_MIPI,
	.mac_clk = RX_MAC_CLK_200M,
	.mipi_attr = {
		.raw_data_type = RAW_DATA_10BIT,
		.lane_id = {0, 1, -1, -1, -1},
		.pn_swap = {1, 1, 0, 0, 0},
		.wdr_mode = CVI_MIPI_WDR_MODE_NONE,
	},
	.mclk = {
		.cam = 0,
		.freq = CAMPLL_FREQ_24M,
	},
	.devno = 0,
};

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* __GC030A_CMOS_PARAM_H_ */
