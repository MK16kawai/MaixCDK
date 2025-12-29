#ifndef __GC030A_CMOS_EX_H_
#define __GC030A_CMOS_EX_H_

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

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif

enum gc030a_linear_regs_e {
	LINEAR_PAGE_0,
	LINEAR_EXP_H,
	LINEAR_EXP_L,
	LINEAR_AGAIN,
	LINEAR_VTS_H,
	LINEAR_VTS_L,
	LINEAR_MIRROR_FLIP,

	LINEAR_REGS_NUM
};

typedef enum _GC030A_MODE_E {
	GC030A_MODE_640X480P30 = 0,
	GC030A_MODE_NUM
} GC030A_MODE_E;

typedef struct _GC030A_MODE_S {
	ISP_WDR_SIZE_S stImg[2];
	CVI_FLOAT f32MaxFps;
	CVI_FLOAT f32MinFps;
	CVI_U32 u32HtsDef;
	CVI_U32 u32VtsDef;
	SNS_ATTR_S stExp[2];
	SNS_ATTR_LARGE_S stAgain[2];
	SNS_ATTR_LARGE_S stDgain[2];
	char name[64];
} GC030A_MODE_S;

/****************************************************************************
 * external variables and functions                                         *
 ****************************************************************************/

extern ISP_SNS_STATE_S *g_pastGc030a[VI_MAX_PIPE_NUM];
extern ISP_SNS_COMMBUS_U g_aunGc030a_BusInfo[];
extern CVI_U8 gc030a_i2c_addr;
extern const CVI_U32 gc030a_addr_byte;
extern const CVI_U32 gc030a_data_byte;
extern void gc030a_init(VI_PIPE ViPipe);
extern void gc030a_exit(VI_PIPE ViPipe);
extern void gc030a_standby(VI_PIPE ViPipe);
extern void gc030a_restart(VI_PIPE ViPipe);
extern int  gc030a_write_register(VI_PIPE ViPipe, int addr, int data);
extern int  gc030a_read_register(VI_PIPE ViPipe, int addr);
extern void gc030a_mirror_flip(VI_PIPE ViPipe, ISP_SNS_MIRRORFLIP_TYPE_E eSnsMirrorFlip);
extern int  gc030a_probe(VI_PIPE ViPipe);

#ifdef __cplusplus
#if __cplusplus
}
#endif
#endif /* End of #ifdef __cplusplus */


#endif /* __GC030A_CMOS_EX_H_ */

