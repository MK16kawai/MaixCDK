#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <syslog.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#ifdef ARCH_CV182X
#include <linux/cvi_vip_snsr.h>
#include "cvi_comm_video.h"
#else
#include <linux/vi_snsr.h>
#include <linux/cvi_comm_video.h>
#endif
#include "cvi_sns_ctrl.h"
#include "gc030a_cmos_ex.h"

static void gc030a_linear_480p30_init(VI_PIPE ViPipe);

#define IMAGE_NO_MIRROR
//#define IMAGE_H_MIRROR
//#define IMAGE_V_MIRROR
//#define IMAGE_HV_MIRROR

#ifdef IMAGE_NO_MIRROR
#define MIRROR 0x54
#define STARTY 0x01
#define STARTX 0x01
#endif

#ifdef IMAGE_H_MIRROR
#define MIRROR 0x55
#define STARTY 0x01
#define STARTX 0x00
#endif

#ifdef IMAGE_V_MIRROR
#define MIRROR 0x56
#define STARTY 0x02
#define STARTX 0x01
#endif

#ifdef IMAGE_HV_MIRROR
#define MIRROR 0x57
#define STARTY 0x02
#define STARTX 0x00
#endif

CVI_U8 gc030a_i2c_addr = 0x21;
const CVI_U32 gc030a_addr_byte = 1;
const CVI_U32 gc030a_data_byte = 1;
static int g_fd[VI_MAX_PIPE_NUM] = {[0 ... (VI_MAX_PIPE_NUM - 1)] = -1};

int gc030a_i2c_init(VI_PIPE ViPipe)
{
	char acDevFile[16] = {0};
	CVI_U8 u8DevNum;

	if (g_fd[ViPipe] >= 0)
		return CVI_SUCCESS;
	int ret;

	u8DevNum = g_aunGc030a_BusInfo[ViPipe].s8I2cDev;
	snprintf(acDevFile, sizeof(acDevFile),  "/dev/i2c-%u", u8DevNum);

	g_fd[ViPipe] = open(acDevFile, O_RDWR, 0600);

	if (g_fd[ViPipe] < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "Open /dev/i2c-%u error!\n", u8DevNum);
		return CVI_FAILURE;
	}

	ret = ioctl(g_fd[ViPipe], I2C_SLAVE_FORCE, gc030a_i2c_addr);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_SLAVE_FORCE error!\n");
		close(g_fd[ViPipe]);
		g_fd[ViPipe] = -1;
		return ret;
	}

	return CVI_SUCCESS;
}

int gc030a_i2c_exit(VI_PIPE ViPipe)
{
	if (g_fd[ViPipe] >= 0) {
		close(g_fd[ViPipe]);
		g_fd[ViPipe] = -1;
		return CVI_SUCCESS;
	}
	return CVI_FAILURE;
}

int gc030a_read_register(VI_PIPE ViPipe, int addr)
{
	int ret, data;
	CVI_U8 buf[8];
	CVI_U8 idx = 0;

	if (g_fd[ViPipe] < 0)
		return CVI_FAILURE;

	if (gc030a_addr_byte == 2)
		buf[idx++] = (addr >> 8) & 0xff;

	// add address byte 0
	buf[idx++] = addr & 0xff;

	ret = write(g_fd[ViPipe], buf, gc030a_addr_byte);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_WRITE error!\n");
		return ret;
	}

	buf[0] = 0;
	buf[1] = 0;
	ret = read(g_fd[ViPipe], buf, gc030a_data_byte);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_READ error!\n");
		return ret;
	}

	// pack read back data
	data = 0;
	if (gc030a_data_byte == 2) {
		data = buf[0] << 8;
		data += buf[1];
	} else {
		data = buf[0];
	}

	syslog(LOG_DEBUG, "i2c r 0x%x = 0x%x\n", addr, data);
	return data;
}


int gc030a_write_register(VI_PIPE ViPipe, int addr, int data)
{
	CVI_U8 idx = 0;
	int ret;
	CVI_U8 buf[8];

	if (g_fd[ViPipe] < 0)
		return CVI_SUCCESS;

	if (gc030a_addr_byte == 1) {
		buf[idx] = addr & 0xff;
		idx++;
	}
	if (gc030a_data_byte == 1) {
		buf[idx] = data & 0xff;
		idx++;
	}

	ret = write(g_fd[ViPipe], buf, gc030a_addr_byte + gc030a_data_byte);
	if (ret < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "I2C_WRITE error!\n");
		return CVI_FAILURE;
	}
//	syslog(LOG_DEBUG, "i2c w 0x%x 0x%x\n", addr, data);
	return CVI_SUCCESS;
}

static void delay_ms(int ms)
{
	usleep(ms * 1000);
}

#define GC030A_CHIP_ID 0x030a
#define GC030A_CHIP_ID_ADDR_H 0xf0
#define GC030A_CHIP_ID_ADDR_L 0xf1

int gc030a_probe(VI_PIPE ViPipe)
{
	int nVal;
	int nVal2;

	usleep(50);
	if (gc030a_i2c_init(ViPipe) != CVI_SUCCESS)
		return CVI_FAILURE;

	nVal  = gc030a_read_register(ViPipe, GC030A_CHIP_ID_ADDR_H);
	nVal2 = gc030a_read_register(ViPipe, GC030A_CHIP_ID_ADDR_L);
	if (nVal < 0 || nVal2 < 0) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "read sensor id error.\n");
		return nVal;
	}

	if ((((nVal & 0xFF) << 8) | (nVal2 & 0xFF)) != GC030A_CHIP_ID) {
		CVI_TRACE_SNS(CVI_DBG_ERR, "Sensor ID Mismatch! Use the wrong sensor??\n");
		return CVI_FAILURE;
	}
	printf("%d\n", ViPipe);
	return CVI_SUCCESS;
}

void gc030a_init(VI_PIPE ViPipe)
{
	gc030a_i2c_init(ViPipe);

	gc030a_linear_480p30_init(ViPipe);

	g_pastGc030a[ViPipe]->bInit = CVI_TRUE;
}

void gc030a_exit(VI_PIPE ViPipe)
{
	gc030a_i2c_exit(ViPipe);
}

static void gc030a_linear_480p30_init(VI_PIPE ViPipe)
{
    /*SYS*/
    gc030a_write_register(ViPipe, 0xfe, 0x80);
    gc030a_write_register(ViPipe, 0xfe, 0x80);
    gc030a_write_register(ViPipe, 0xfe, 0x80);
    gc030a_write_register(ViPipe, 0xf7, 0x01);
    gc030a_write_register(ViPipe, 0xf8, 0x05);
    gc030a_write_register(ViPipe, 0xf9, 0x0f);
    gc030a_write_register(ViPipe, 0xfa, 0x00);
    gc030a_write_register(ViPipe, 0xfc, 0x0f);
    gc030a_write_register(ViPipe, 0xfe, 0x00);

    /*ANALOG & CISCTL*/
    gc030a_write_register(ViPipe, 0x03, 0x01);
    gc030a_write_register(ViPipe, 0x04, 0xc8);
    gc030a_write_register(ViPipe, 0x05, 0x03);
    gc030a_write_register(ViPipe, 0x06, 0x7b);
    gc030a_write_register(ViPipe, 0x07, 0x00);
    gc030a_write_register(ViPipe, 0x08, 0x06);
    gc030a_write_register(ViPipe, 0x0a, 0x00);
    gc030a_write_register(ViPipe, 0x0c, 0x08);
    gc030a_write_register(ViPipe, 0x0d, 0x01);
    gc030a_write_register(ViPipe, 0x0e, 0xe8);
    gc030a_write_register(ViPipe, 0x0f, 0x02);
    gc030a_write_register(ViPipe, 0x10, 0x88);
    gc030a_write_register(ViPipe, 0x17, MIRROR);//Don't Change Here!!!
    gc030a_write_register(ViPipe, 0x18, 0x12);
    gc030a_write_register(ViPipe, 0x19, 0x07);
    gc030a_write_register(ViPipe, 0x1a, 0x1b);
    gc030a_write_register(ViPipe, 0x1d, 0x40);
    gc030a_write_register(ViPipe, 0x1e, 0x50);
    gc030a_write_register(ViPipe, 0x1f, 0x80);
    gc030a_write_register(ViPipe, 0x23, 0x01);
    gc030a_write_register(ViPipe, 0x24, 0xc8);
    gc030a_write_register(ViPipe, 0x27, 0xaf);
    gc030a_write_register(ViPipe, 0x28, 0x24);
    gc030a_write_register(ViPipe, 0x29, 0x1a);
    gc030a_write_register(ViPipe, 0x2f, 0x14);
    gc030a_write_register(ViPipe, 0x30, 0x00);
    gc030a_write_register(ViPipe, 0x31, 0x04);
    gc030a_write_register(ViPipe, 0x32, 0x08);
    gc030a_write_register(ViPipe, 0x33, 0x0c);
    gc030a_write_register(ViPipe, 0x34, 0x0d);
    gc030a_write_register(ViPipe, 0x35, 0x0e);
    gc030a_write_register(ViPipe, 0x36, 0x0f);
    gc030a_write_register(ViPipe, 0x72, 0x98);
    gc030a_write_register(ViPipe, 0x73, 0x9a);
    gc030a_write_register(ViPipe, 0x74, 0x47);
    gc030a_write_register(ViPipe, 0x76, 0x82);
    gc030a_write_register(ViPipe, 0x7a, 0xcb);
    gc030a_write_register(ViPipe, 0xc2, 0x0c);
    gc030a_write_register(ViPipe, 0xce, 0x03);
    gc030a_write_register(ViPipe, 0xcf, 0x48);
    gc030a_write_register(ViPipe, 0xd0, 0x10);
    gc030a_write_register(ViPipe, 0xdc, 0x75);
    gc030a_write_register(ViPipe, 0xeb, 0x78);

    /*ISP*/
    gc030a_write_register(ViPipe, 0x90, 0x01);
    gc030a_write_register(ViPipe, 0x92, STARTY);//Don't Change Here!!!
    gc030a_write_register(ViPipe, 0x94, STARTX);//Don't Change Here!!!
    gc030a_write_register(ViPipe, 0x95, 0x01);
    gc030a_write_register(ViPipe, 0x96, 0xe0);
    gc030a_write_register(ViPipe, 0x97, 0x02);
    gc030a_write_register(ViPipe, 0x98, 0x80);

    /*Gain*/
    gc030a_write_register(ViPipe, 0xb0, 0x46);
    gc030a_write_register(ViPipe, 0xb1, 0x01);
    gc030a_write_register(ViPipe, 0xb2, 0x00);
    gc030a_write_register(ViPipe, 0xb3, 0x40);
    gc030a_write_register(ViPipe, 0xb4, 0x40);
    gc030a_write_register(ViPipe, 0xb5, 0x40);
    gc030a_write_register(ViPipe, 0xb6, 0x00);

    /*BLK*/
    gc030a_write_register(ViPipe, 0x40, 0x26);
    gc030a_write_register(ViPipe, 0x4e, 0x00);
    gc030a_write_register(ViPipe, 0x4f, 0x3c);

    /*Dark Sun*/
    gc030a_write_register(ViPipe, 0xe0, 0x9f);
    gc030a_write_register(ViPipe, 0xe1, 0x90);
    gc030a_write_register(ViPipe, 0xe4, 0x0f);
    gc030a_write_register(ViPipe, 0xe5, 0xff);

    /*MIPI*/
    gc030a_write_register(ViPipe, 0xfe, 0x03);
    gc030a_write_register(ViPipe, 0x10, 0x00);
    gc030a_write_register(ViPipe, 0x01, 0x03);
    gc030a_write_register(ViPipe, 0x02, 0x33);
    gc030a_write_register(ViPipe, 0x03, 0x96);
    gc030a_write_register(ViPipe, 0x04, 0x01);
    gc030a_write_register(ViPipe, 0x05, 0x00);
    gc030a_write_register(ViPipe, 0x06, 0x80);
    gc030a_write_register(ViPipe, 0x11, 0x2b);
    gc030a_write_register(ViPipe, 0x12, 0x20);
    gc030a_write_register(ViPipe, 0x13, 0x03);
    gc030a_write_register(ViPipe, 0x15, 0x00);
    gc030a_write_register(ViPipe, 0x21, 0x10);
    gc030a_write_register(ViPipe, 0x22, 0x00);
    gc030a_write_register(ViPipe, 0x23, 0x30);
    gc030a_write_register(ViPipe, 0x24, 0x02);
    gc030a_write_register(ViPipe, 0x25, 0x12);
    gc030a_write_register(ViPipe, 0x26, 0x02);
    gc030a_write_register(ViPipe, 0x29, 0x01);
    gc030a_write_register(ViPipe, 0x2a, 0x0a);
    gc030a_write_register(ViPipe, 0x2b, 0x08);
    gc030a_write_register(ViPipe, 0xfe, 0x00);
    gc030a_write_register(ViPipe, 0xf9, 0x0e);
    gc030a_write_register(ViPipe, 0xfc, 0x0e);
    gc030a_write_register(ViPipe, 0xfe, 0x00);
    gc030a_write_register(ViPipe, 0x25, 0xa2);
    gc030a_write_register(ViPipe, 0x3f, 0x1a);
    delay_ms(100);
    gc030a_write_register(ViPipe, 0x25, 0xe2);

    gc030a_write_register(ViPipe, 0xfe, 0x03);
    gc030a_write_register(ViPipe, 0x10, 0x90);
    gc030a_write_register(ViPipe, 0xfe, 0x00);

	printf("ViPipe:%d,===GC030A 480P 30fps RAW Init OK!===\n", ViPipe);
}

