/**
 * @author lxowalle@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2023.9.8: Add framework, create this file.
 */


#pragma once

#include "maix_display_base.hpp"
#include "maix_thread.hpp"
#include "maix_image.hpp"
#include "maix_util.hpp"
#include "maix_time.hpp"
#include <unistd.h>
#include "maix_pwm.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include "maix_fs.hpp"
#include <memory>
#include <list>
#include "ax_middleware.hpp"
#include "maix_pinmap.hpp"
#include <mutex>

using namespace maix::peripheral;
using namespace maix::middleware;
namespace maix::display
{
    enum class PanelType {
        NORMAL,
        LT9611,
        UNKNOWN,
    };
    // inline static PanelType __g_panel_type = PanelType::UNKNOWN;
    class DisplayAx;

    static std::mutex __g_mutex;
    static int __g_ch[2];
    static maixcam2::SYS *__g_sys[2];
    static maixcam2::VO * __g_vo[2];
    static pwm::PWM * __g_pwm[2];

    static void __release_layer(int layer) {
        auto &mod_param = maixcam2::AxModuleParam::getInstance();
        mod_param.unlock(maixcam2::AX_MOD_VO);
        __g_mutex.lock();
        if (__g_vo[layer]) {
            __g_vo[layer]->del_channel(layer, __g_ch[layer]);
            __g_vo[layer]->deinit();
            delete __g_vo[layer];
            __g_vo[layer] = nullptr;
        }
        if (__g_sys[layer]) {
            delete __g_sys[layer];
            __g_sys[layer] = nullptr;
        }

        if (__g_pwm[layer]) {
            __g_pwm[layer]->duty(0);
        }
        __g_mutex.unlock();
    }

    static void __release_layer0_handler(void) {
        __release_layer(0);
    }

    static void __release_layer1_handler(void) {
        __release_layer(1);
    }

    static void __register_release_vo(int layer, maixcam2::SYS *sys, maixcam2::VO *vo) {
        if (layer == 0) {
            util::register_exit_function(__release_layer0_handler);
        } else if (layer == 1) {
            util::register_exit_function(__release_layer1_handler);
        }
        __g_mutex.lock();
        __g_sys[layer] = sys;
        __g_vo[layer] = vo;
        __g_mutex.unlock();
    }

    static void __unregister_release_vo(int layer) {
        __g_mutex.lock();
        __g_sys[layer] = nullptr;
        __g_vo[layer] = nullptr;
        __g_mutex.unlock();
    }

    static bool __vo_is_ready(int layer) {
        bool is_ready = false;
        __g_mutex.lock();
        if (__g_vo[layer] && __g_vo[layer]) {
            is_ready = true;
        } else {
            is_ready = false;
        }
        __g_mutex.unlock();
        return is_ready;
    }

    __attribute__((unused)) static int _get_vo_max_size(int *width, int *height, int rotate)
    {
        int w = 480, h = 640;

        if (rotate) {
            *width = h;
            *height = w;
        } else {
            *width = w;
            *height = h;
        }
        return 0;
    }
#if 0
    static __attribute__((unused)) void __print_video_frame(AX_VIDEO_FRAME_T *frame) {
        printf(" ===================== video frame ===================== \r\n");
        printf("stVFrame.nWidth:%d\r\n", frame->u32Width);
        printf("stVFrame.u32Height:%d\r\n", frame->u32Height);
        printf("stVFrame.enImgFormat:%d\r\n", (int)frame->enImgFormat);
        printf("stVFrame.enVscanFormat:%d\r\n", (int)frame->enVscanFormat);
        printf("stVFrame.stCompressInfo.enCompressMode:%d\r\n", (int)frame->stCompressInfo.enCompressMode);
        printf("stVFrame.stCompressInfo.u32CompressLevel:%d\r\n", (int)frame->stCompressInfo.u32CompressLevel);
        printf("stVFrame.stDynamicRange:%d\r\n", (int)frame->stDynamicRange);
        printf("stVFrame.stColorGamut:%d\r\n", (int)frame->stColorGamut);
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u32PicStride[%d]:%d\r\n", i, (int)frame->u32PicStride[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u32ExtStride[%d]:%d\r\n", i, (int)frame->u32ExtStride[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u64PhyAddr[%d]:%#x\r\n", i, (int)frame->u64PhyAddr[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u64VirAddr[%d]:%#x\r\n", i, (int)frame->u64VirAddr[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u64ExtPhyAddr[%d]:%#x\r\n", i, (int)frame->u64ExtPhyAddr[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u64ExtVirAddr[%d]:%#x\r\n", i, (int)frame->u64ExtVirAddr[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u32HeaderSize[%d]:%d\r\n", i, (int)frame->u32HeaderSize[i]);
        }
        for (int i = 0; i < AX_MAX_COLOR_COMPONENT; i++) {
            printf("stVFrame.u32BlkId[%d]:%d\r\n", i, (int)frame->u32BlkId[i]);
        }
        printf("stVFrame.s16CropX:%d\r\n", frame->s16CropX);
        printf("stVFrame.s16CropY:%d\r\n", frame->s16CropY);
        printf("stVFrame.s16CropWidth:%d\r\n", frame->s16CropWidth);
        printf("stVFrame.s16CropHeight:%d\r\n", frame->s16CropHeight);

        printf("stVFrame.u32TimeRef:%d\r\n", frame->u32TimeRef);
        printf("stVFrame.u64PTS:%lld\r\n", frame->u64PTS);
        printf("stVFrame.u64SeqNum:%lld\r\n", frame->u64SeqNum);
        printf("stVFrame.u64UserData:%lld\r\n", frame->u64UserData);
        printf("stVFrame.u64PrivateData:%lld\r\n", frame->u64PrivateData);

        printf("stVFrame.u32FrameFlag:%d\r\n", frame->u32FrameFlag);
        printf("stVFrame.u32FrameSize:%d\r\n", frame->u32FrameSize);
    }
#endif
    static void _get_disp_configs(bool &flip, bool &mirror, float &max_backlight) {
        std::string flip_str;
        bool flip_is_found = false;

        auto device_configs = sys::device_configs();
        auto it = device_configs.find("disp_flip");
        if (it != device_configs.end()) {
            flip_str = it->second;
            flip_is_found = true;
        }
        auto it2 = device_configs.find("disp_mirror");
        if (it2 != device_configs.end()) {
            if (it2->second.size() > 0)
                mirror = atoi(it2->second.c_str());
        }
        auto it3 = device_configs.find("disp_max_backlight");
        if (it3 != device_configs.end()) {
            if (it3->second.size() > 0)
                max_backlight = atof(it3->second.c_str());
        }

        if (flip_is_found && flip_str.size() > 0) {
            flip = atoi(flip_str.c_str());
        } else {
            std::string board_id = sys::device_id();
            if (board_id == "maixcam2") {
                flip = true;
            }
        }

        // log::info("disp config flip: %d, mirror: %d, max_backlight: %.1f", flip, mirror, max_backlight);
    }


    class DisplayAx final : public DisplayBase
    {
        maixcam2::SYS * __sys;
        maixcam2::VO * __vo;

        static AX_U32 SAMPLE_CALC_IMAGE_SIZE(AX_U32 u32Width, AX_U32 u32Height, AX_IMG_FORMAT_E eImgType, AX_U32 u32Stride) {
            AX_U32 u32Bpp = 0;
            if (u32Width == 0 || u32Height == 0) {
                log::error("Invalid width %d or height %d!", u32Width, u32Height);
                return 0;
            }

            if (0 == u32Stride) {
                u32Stride = u32Width;
            }

            switch (eImgType) {
                case AX_FORMAT_YUV400:
                    u32Bpp = 8;
                    break;
                case AX_FORMAT_YUV420_PLANAR:
                case AX_FORMAT_YUV420_SEMIPLANAR:
                case AX_FORMAT_YUV420_SEMIPLANAR_VU:
                    u32Bpp = 12;
                    break;
                case AX_FORMAT_YUV422_INTERLEAVED_YUYV:
                case AX_FORMAT_YUV422_INTERLEAVED_UYVY:
                    u32Bpp = 16;
                    break;
                case AX_FORMAT_YUV444_PACKED:
                case AX_FORMAT_RGB888:
                case AX_FORMAT_BGR888:
                    u32Bpp = 24;
                    break;
                case AX_FORMAT_RGBA8888:
                case AX_FORMAT_BGRA8888:
                case AX_FORMAT_ARGB8888:
                case AX_FORMAT_ABGR8888:
                    u32Bpp = 32;
                    break;
                default:
                    u32Bpp = 0;
                    break;
            }

            return u32Stride * u32Height * u32Bpp / 8;
        }

        static AX_S32 __ax_ivps_crop_resize_tdp(maixcam2::Frame &in, maixcam2::Frame &out, const AX_IVPS_CROP_RESIZE_ATTR_T *ptAttr, bool update_frame = true) {
            AX_S32 s32Ret = 0;
            AX_VIDEO_FRAME_T stFrameDst = {0}, stFrameSrc = {0};
            err::check_raise(in.get_video_frame(&stFrameSrc), "get video frame failed");
            err::check_raise(out.get_video_frame(&stFrameDst), "get video frame failed");

            // log::info(" ========== SRC ++++++++");
            // __print_video_frame(&stFrameSrc);

            s32Ret = AX_IVPS_CropResizeTdp(&stFrameSrc, &stFrameDst, ptAttr);
            if (s32Ret != 0) {
                return s32Ret;
            }
            // log::info(" ========== DST ++++++++");
            // __print_video_frame(&stFrameDst);static int count = 0;
            // log::info("crop resize count:%d", count ++);
            AX_SYS_MinvalidateCache(stFrameDst.u64PhyAddr[0], (AX_U8* )stFrameDst.u64VirAddr[0], stFrameDst.u32FrameSize);

            if (update_frame) {
                in.set_video_frame(&stFrameSrc);
                out.set_video_frame(&stFrameDst);
            }
            return 0;
        }

        static AX_S32 __ax_ivps_crop_resize_vpp(maixcam2::Frame &in, maixcam2::Frame &out, const AX_IVPS_CROP_RESIZE_ATTR_T *ptAttr, bool update_frame = true) {
            AX_S32 s32Ret = 0;
            AX_VIDEO_FRAME_T stFrameDst = {0}, stFrameSrc = {0};
            err::check_raise(in.get_video_frame(&stFrameSrc), "get video frame failed");
            err::check_raise(out.get_video_frame(&stFrameDst), "get video frame failed");

            // log::info(" ========== SRC ++++++++");
            // __print_video_frame(&stFrameSrc);

            s32Ret = AX_IVPS_CropResizeVpp(&stFrameSrc, &stFrameDst, ptAttr);
            if (s32Ret != 0) {
                return s32Ret;
            }
            // log::info(" ========== DST ++++++++");
            // __print_video_frame(&stFrameDst);static int count = 0;
            // log::info("crop resize count:%d", count ++);
            AX_SYS_MinvalidateCache(stFrameDst.u64PhyAddr[0], (AX_U8* )stFrameDst.u64VirAddr[0], stFrameDst.u32FrameSize);

            if (update_frame) {
                in.set_video_frame(&stFrameSrc);
                out.set_video_frame(&stFrameDst);
            }
            return 0;
        }

        static void __config_vo_param(maixcam2::ax_vo_param_t *new_param, int width, int height, image::Format format, int rotate) {
            memset(new_param, 0, sizeof(maixcam2::ax_vo_param_t));
            AX_VO_SYNC_INFO_T sync_info = {.u16Vact = 640, .u16Vbb = 30, .u16Vfb = 30, .u16Hact = 480, .u16Hbb = 30, .u16Hfb = 30, .u16Hpw = 40, .u16Vpw = 11, .u32Pclk = 24750, .bIdv = AX_TRUE, .bIhs = AX_FALSE, .bIvs = AX_TRUE};
            SAMPLE_VO_CONFIG_S vo_cfg = {
                .u32VDevNr = 1,
                .stVoDev = {{
                    .u32VoDev = 0,
                    .enMode = AX_VO_MODE_OFFLINE,
                    .enVoIntfType = AX_VO_INTF_DSI,
                    .enIntfSync = AX_VO_OUTPUT_USER,
                    .enVoOutfmt = AX_VO_OUT_FMT_UNUSED,
                    .u32SyncIndex = 2,
                    .setCsc = AX_FALSE,
                    .bWbcEn = AX_FALSE,
                }},
                .stVoLayer = {
                    {
                        .bindVoDev = {SAMPLE_VO_DEV_MAX, SAMPLE_VO_DEV_MAX},
                        .enChnFrmFmt = AX_FORMAT_YUV420_SEMIPLANAR,
                    },
                    {
                        .bindVoDev = {SAMPLE_VO_DEV_MAX, SAMPLE_VO_DEV_MAX},
                        .enChnFrmFmt = AX_FORMAT_YUV420_SEMIPLANAR,
                    },
                },
            };
            vo_cfg.stGraphicLayer[0].u32FbNum = 1;
            vo_cfg.stGraphicLayer[0].stFbConf[0].u32Index = 0;
            vo_cfg.stGraphicLayer[0].stFbConf[0].u32Fmt = AX_FORMAT_ARGB8888;
            if (rotate) {
                vo_cfg.stVoLayer[0].stVoLayerAttr.stImageSize.u32Width = height;
                vo_cfg.stVoLayer[0].stVoLayerAttr.stImageSize.u32Height = width;
                vo_cfg.stGraphicLayer[0].stFbConf[0].u32ResoW = height;
                vo_cfg.stGraphicLayer[0].stFbConf[0].u32ResoH = width;
            } else {
                vo_cfg.stVoLayer[0].stVoLayerAttr.stImageSize.u32Width = height;
                vo_cfg.stVoLayer[0].stVoLayerAttr.stImageSize.u32Height = width;
                vo_cfg.stGraphicLayer[0].stFbConf[0].u32ResoW = width;
                vo_cfg.stGraphicLayer[0].stFbConf[0].u32ResoH = height;
            }
            vo_cfg.stVoLayer[0].stVoLayerAttr.enPixFmt = AX_FORMAT_YUV420_SEMIPLANAR;
            vo_cfg.stVoLayer[0].stVoLayerAttr.u32DispatchMode = 1;
            vo_cfg.stVoLayer[0].stVoLayerAttr.f32FrmRate = 60.0;
            vo_cfg.stVoLayer[0].u32ChnNr = 1;
            memcpy(&new_param->vo_cfg, &vo_cfg, sizeof(vo_cfg));
            memcpy(&new_param->sync_info, &sync_info, sizeof(sync_info));
        }

        class CmmPool {
            AX_POOL _pool_id;
            AX_S32 _pool_size;
            AX_S32 _pool_count;

            static AX_POOL __create_pool(AX_S32 size, AX_S32 count) {
                AX_POOL_CONFIG_T stPoolCfg = {0};
                stPoolCfg.MetaSize = 512;
                stPoolCfg.BlkCnt = count;
                stPoolCfg.BlkSize = size;
                stPoolCfg.CacheMode = AX_POOL_CACHE_MODE_NONCACHE;
                strcpy((char *)stPoolCfg.PartitionName, "anonymous");
                AX_POOL pool_id = AX_POOL_CreatePool(&stPoolCfg);
                if (pool_id == AX_INVALID_POOLID) {
                    log::info("AX_POOL_CreatePool failed, u32BlkCnt = %d, u64BlkSize = 0x%llx, u64MetaSize = 0x%llx, ret:%#x", stPoolCfg.BlkCnt,
                        stPoolCfg.BlkSize, stPoolCfg.MetaSize, pool_id);
                    return AX_INVALID_POOLID;
                }
                return pool_id;
            }

            static int __release_pool(AX_POOL pool_id) {
                AX_S32 ret = 0;
                if (pool_id != AX_INVALID_POOLID) {
                    ret = AX_POOL_DestroyPool(pool_id);
                    if (ret != 0) {
                        // do nothing
                        // log::info("AX_POOL_DestroyPool failed, PoolId = 0x%d, ret:%#x", pool_id, ret);
                    }
                }
                return ret;
            }

        public:
            CmmPool(){
                _pool_id = AX_INVALID_POOLID;
                _pool_size = 0;
                _pool_count = 0;
            }

            ~CmmPool() {
                deinit();
            }

            err::Err init(int size, int count) {
                _pool_id = __create_pool(size, count);
                if (_pool_id == AX_INVALID_POOLID) {
                    return err::ERR_NO_MEM;
                } else {
                    return err::ERR_NONE;
                }
            }

            err::Err reset(int size, int count) {
                err::Err ret = err::ERR_NONE;
                if (size != _pool_size || count != _pool_count) {
                    if ((ret = deinit()) != err::ERR_NONE) {
                        return ret;
                    }

                    if ((ret = init(size, count)) != err::ERR_NONE) {
                        return ret;
                    }
                }
                return err::ERR_NONE;
            }

            err::Err deinit() {
                if (0 != __release_pool(_pool_id)) {
                    return err::ERR_RUNTIME;
                }
                _pool_id = AX_INVALID_POOLID;
                return err::ERR_NONE;
            }

            AX_POOL pool_id() {
                return _pool_id;
            }

            AX_POOL pool_size() {
                return _pool_size;
            }

            AX_POOL pool_count() {
                return _pool_count;
            }
        };

    public:
        DisplayAx(const string &device, int width, int height, image::Format format)
        {
            bool rotate = 1;
            err::check_bool_raise(!_get_vo_max_size(&_max_width, &_max_height, rotate), "get vo max size failed");
            width = width <= 0 ? _max_width : width;
            height = height <= 0 ? _max_height : height;
            this->_width = width > _max_width ? _max_width : width;
            this->_height = height > _max_height ? _max_height : height;
            this->_format = format;
            this->_opened = false;
            this->_format = format;
            this->_invert_flip = false;
            this->_invert_mirror = false;
            this->_max_backlight = 50.0;
            this->_layer = 0;       // layer 0 means vedio layer
            err::check_bool_raise(_format == image::FMT_RGB888
                                || _format == image::FMT_YVU420SP
                                || _format == image::FMT_YUV420SP
                                || _format == image::FMT_BGRA8888, "Format not support");

            _get_disp_configs(this->_invert_flip, this->_invert_mirror, _max_backlight);

            __sys = new maixcam2::SYS;
            err::check_bool_raise(__sys != nullptr, "display construct sys failed");
            err::check_bool_raise(__sys->init() == err::ERR_NONE, "display init sys failed");

            __vo = new maixcam2::VO;
            err::check_bool_raise(__vo != nullptr, "VO init failed");

            maixcam2::ax_vo_param_t vo_param;
            __config_vo_param(&vo_param, width, height, format, rotate);
            err::check_bool_raise(__vo->init(&vo_param) == err::ERR_NONE, "VO init failed");
            _bl_pwm = nullptr;
            // int pwm_id = 5; // for MaixCAM2 alpha version board
            // pinmap::set_pin_function("B24", "PWM5")
            int pwm_id = 3;
            pinmap::set_pin_function("B22", "PWM3");
            _bl_pwm = new pwm::PWM(pwm_id, 10000, 50);

            _dst_pool.reset(this->_width*this->_height*image::fmt_size[this->format()], 1);

            __g_mutex.lock();
            __g_vo[this->_layer] = __vo;
            __g_sys[this->_layer] = __sys;
            __g_pwm[this->_layer] = _bl_pwm;
            __g_ch[this->_layer] = 0;
            __g_mutex.unlock();
        }

        DisplayAx(int layer, int width, int height, image::Format format)
        {
            bool rotate = 1;
            err::check_bool_raise(!_get_vo_max_size(&_max_width, &_max_height, rotate), "get vo max size failed");
            width = width <= 0 ? _max_width : width;
            height = height <= 0 ? _max_height : height;
            this->_width = width > _max_width ? _max_width : width;
            this->_height = height > _max_height ? _max_height : height;
            this->_format = format;
            this->_opened = false;
            this->_format = format;
            this->_invert_flip = false;
            this->_invert_mirror = false;
            this->_max_backlight = 50.0;
            this->_layer = layer;       // layer 0 means vedio layer
                                        // layer 1 means osd layer
            err::check_bool_raise(_format == image::FMT_BGRA8888, "Format not support");

            _get_disp_configs(this->_invert_flip, this->_invert_mirror, _max_backlight);

            __sys = new maixcam2::SYS;
            err::check_bool_raise(__sys != nullptr, "display construct sys failed");
            err::check_bool_raise(__sys->init() == err::ERR_NONE, "display init sys failed");

            __vo = new maixcam2::VO;
            err::check_bool_raise(__vo != nullptr, "VO init failed");

            maixcam2::ax_vo_param_t vo_param;
            __config_vo_param(&vo_param, width, height, format, rotate);
            err::check_bool_raise(__vo->init(&vo_param) == err::ERR_NONE, "VO init failed");
            _bl_pwm = nullptr;
            _dst_pool.reset(this->_width*this->_height*image::fmt_size[this->format()], 1);

            __g_mutex.lock();
            __g_vo[this->_layer] = __vo;
            __g_sys[this->_layer] = __sys;
            __g_ch[this->_layer] = 0;
            __g_mutex.unlock();
        }

        ~DisplayAx()
        {
            __g_mutex.lock();
            __vo = __g_vo[this->_layer];
            if (__vo) {
                __vo->del_channel(this->_layer, this->_ch);
                __vo->deinit();
                delete __vo;
                __vo = nullptr;
            }

            __sys = __g_sys[this->_layer];
            if (__sys) {
                delete __sys;
                __sys = nullptr;
            }
            __g_mutex.unlock();
            __unregister_release_vo(this->_layer);

            if(_bl_pwm && this->_layer == 0)    // _layer = 0, means video layer
            {
                delete _bl_pwm;
                __g_mutex.lock();
                __g_pwm[this->_layer] = nullptr;
                __g_mutex.unlock();
            }
        }

        int width()
        {
            return this->_width;
        }

        int height()
        {
            return this->_height;
        }

        std::vector<int> size()
        {
            return {this->_width, this->_height};
        }

        image::Format format()
        {
            return this->_format;
        }

        static int __reset_src_pool(CmmPool &pool, int width, int height, image::Format format) {
            int ret = 0;
            int block_count = 2;
            int block_size = width * height * image::fmt_size[format];
            if (width * height * image::fmt_size[format] >= 2560 * 1440 * 3 / 2) {
                block_count = 3;
            } else {
                block_count = 2;
            }
            if ((ret = pool.reset(block_size, block_count)) != err::ERR_NONE) {
                return ret;
            }
            return ret;
        }

        err::Err open(int width, int height, image::Format format)
        {
            err::Err ret = err::ERR_NONE;
            width = (width < 0 || width > _max_width) ? _max_width : width;
            height = (height < 0 || height > _max_height) ? _max_height : height;
            if(this->_opened)
            {
                return err::ERR_NONE;
            }

            int ch = __vo->get_unused_channel(this->_layer);
            if (ch < 0) {
                log::error("vo get unused channel failed\n");
                return err::ERR_RUNTIME;
            }

            maixcam2::ax_vo_channel_param_t param ={
                .width = width,
                .height = height,
                .format_in = maixcam2::get_ax_fmt_from_maix(format),
                .format_out = maixcam2::get_ax_fmt_from_maix(image::FMT_YVU420SP),
                .fps = 60,
                .depth = 0,
                .mirror = _invert_mirror,
                .vflip = _invert_flip,
                .fit = 0,
                .rotate = 90,
                .pool_num_in = -1,
                .pool_num_out = -1,
            };
            if (err::ERR_NONE != (ret = __vo->add_channel(this->_layer, ch, &param))) {
                log::error("vo add channel failed, ret:%d", ret);
                return err::ERR_RUNTIME;
            }

            if (this->_layer == 0) {
                if ( 0 != __reset_src_pool(_src_pool, width, height, format)) {
                    log::warn("Failed to reset src pool");
                }
            }

            this->_ch = ch;
            this->_opened = true;

            // run after _opened is true
            __g_mutex.lock();
            __g_ch[this->_layer] = this->_ch;
            __g_mutex.unlock();
            __register_release_vo(this->_layer, this->__sys, this->__vo);
            return err::ERR_NONE;
        }

        err::Err close()
        {
            if (!this->_opened)
                return err::ERR_NONE;

            __g_mutex.lock();
            __vo = __g_vo[this->_layer];
            if (__vo) {
                __vo->del_channel(this->_layer, this->_ch);
            }
            __g_mutex.unlock();

            this->_opened = false;

            // run after _opened is false
            __unregister_release_vo(this->_layer);
            return err::ERR_NONE;
        }

        display::DisplayAx *add_channel(int width, int height, image::Format format)
        {
            int new_width = 0;
            int new_height = 0;
            image::Format new_format = format;
            if (width == -1) {
                new_width = this->_width;
            } else {
                new_width = width > this->_width ? this->_width : width;
            }
            if (height == -1) {
                new_height = this->_height;
            } else {
                new_height = height > this->_height ? this->_height : height;
            }

            _format = new_format;
            DisplayAx *disp = new DisplayAx(1, new_width, new_height, new_format);
            return disp;
        }

        bool is_opened()
        {
            return this->_opened;
        }

        err::Err show(image::Image &img, image::Fit fit)
        {
            if (!__vo_is_ready(this->_layer)) {
                return err::ERR_NOT_READY;
            }

            err::Err ret = err::ERR_NONE;
            image::Image *input_img = &img;
            maixcam2::Frame *input_frame = nullptr, *output_frame = nullptr;
            bool need_delete_input_img = false;
            bool input_img_too_short = false;
            int input_img_w = img.width(), input_img_h = img.height();
            int format = img.format();
            if (img.width() % 16 != 0) {
                input_img_w = ((img.width() + 16) >> 4 )<< 4;
                need_delete_input_img = true;
            }
            if (img.height() % 2 != 0) {
                input_img_h = ((img.height() + 2) >> 1 )<< 1;
                need_delete_input_img = true;
            }
            #define IMG_MIN_WIDTH     (64)
            #define IMG_MIN_HEIGHT    (64)
            if (img.width() < IMG_MIN_WIDTH || img.height() < IMG_MIN_HEIGHT) {
                input_img_too_short = true;
                need_delete_input_img = true;
            }

            if (need_delete_input_img) {
                if (input_img_too_short) {
                    auto input_img_w_scale = (double)input_img_w / IMG_MIN_WIDTH;
                    auto input_img_h_scale = (double)input_img_h / IMG_MIN_HEIGHT;
                    double scale = std::min(input_img_h_scale, input_img_w_scale);
                    input_img_w = input_img_w / scale;
                    input_img_h = input_img_h / scale;
                    input_img_w = ((input_img_w + 16) >> 4 )<< 4;
                    input_img_h = ((input_img_h + 2) >> 1 )<< 1;
                    input_img = img.resize(input_img_w, input_img_h);
                    // log::info("resize %dx%d to %dx%d", img.width(), img.height(), input_img_w, input_img_h);
                } else {
                    input_img = img.resize(input_img_w, input_img_h);
                    // log::info("resize %dx%d to %dx%d", img.width(), img.height(), input_img_w, input_img_h);
                }
                if (input_img == nullptr) {
                    need_delete_input_img = false;
                    log::warn("Failed to resize image to %dx%d", input_img_w, input_img_h);
                    ret = err::ERR_RUNTIME;
                    goto _exit;
                }
            }

            if (this->_layer == 0) {
                if (0 !=__reset_src_pool(_src_pool, input_img->width(), input_img->height(), input_img->format())) {
                    log::warn("Failed to reset src pool");
                }
                while (input_frame == nullptr && !app::need_exit()) {
                    try {
                        input_frame = new maixcam2::Frame(_src_pool.pool_id(), input_img->width(), input_img->height(), input_img->data(), input_img->data_size(), maixcam2::get_ax_fmt_from_maix(input_img->format()));
                    } catch (...) {
                        time::sleep_ms(5);
                    }
                }

                if (!input_frame) {
                    ret = err::ERR_RUNTIME;
                    goto _exit;
                }

                switch (fit) {
                case image::FIT_CONTAIN:
                {
                    while (output_frame == nullptr && !app::need_exit()) {
                        try {
                            auto dst_pool_id = _dst_pool.pool_id();
                            auto new_frame_format = input_img->format();
                            output_frame = new maixcam2::Frame(dst_pool_id, _width, _height, nullptr, 0, maixcam2::get_ax_fmt_from_maix(new_frame_format));
                            if (new_frame_format >= image::FMT_YVU420SP && new_frame_format <= image::FMT_YUV420P) {
                                memset(output_frame->data, 0, _width * _height);
                                memset((uint8_t *)(output_frame->data) + _width * _height, 128, _width * _height / 2);
                            } else {
                                memset(output_frame->data, 0, output_frame->len);
                            }
                        } catch (...) {
                            time::sleep_ms(5);
                        }
                    }

                    if (!output_frame) {
                        ret = err::ERR_RUNTIME;
                        goto _exit;
                    }

                    AX_IVPS_CROP_RESIZE_ATTR_T crop_resize_attr;
                    memset(&crop_resize_attr, 0, sizeof(AX_IVPS_CROP_RESIZE_ATTR_T));
                    crop_resize_attr.tAspectRatio.nBgColor = (AX_U32)0;
                    crop_resize_attr.eSclType = AX_IVPS_SCL_TYPE_AUTO;
                    crop_resize_attr.eSclInput = AX_IVPS_SCL_INPUT_SHARE;
                    crop_resize_attr.tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_AUTO;
                    crop_resize_attr.tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
                    crop_resize_attr.tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
                    crop_resize_attr.tAspectRatio.nBgColor = (AX_U32)0;
                    AX_S32 ret = __ax_ivps_crop_resize_tdp(*input_frame, *output_frame, &crop_resize_attr, false);
                    if (ret != 0) {
                        log::info("Failed to fit image, ret:%#x", ret);
                        ret = err::ERR_RUNTIME;
                        goto _exit;
                    }

                    // fit success, use new frame
                    delete input_frame;
                    input_frame = nullptr;
                    break;
                }
                case image::FIT_COVER:
                {
                    float scale_x = (float)_width / input_frame->w;
                    float scale_y = (float)_height / input_frame->h;
                    float scale = (float)_width / _height;
                    int crop_x = 0, crop_y = 0, crop_w = input_frame->w, crop_h = input_frame->h;
                    // log::info("_width:%d _height:%d", _width, _height);
                    // log::info("scale x %f, y %f, scale %f", scale_x, scale_y, scale);
                    if (input_frame->w <= _width && input_frame->h <= _height) {
                        // 图像在显示屏内部, 使用比例小的边
                        if (scale_x > scale_y) {        // x边小
                            crop_w = input_frame->w;
                            crop_h = crop_w / scale;    // _w / _h = crop_w / crop_h; crop_h = crop_w * _h / _w;
                            crop_w = ALIGN_UP(crop_w, 16);
                            crop_h = ALIGN_UP(crop_h, 2);
                            crop_x = 0;
                            crop_y = (input_frame->h - crop_h) / 2;
                        } else {                        // y边小
                            crop_h = input_frame->h;
                            crop_w = crop_h * scale;    // _w / _h = crop_w / crop_h; crop_w = crop_h * _w / _h;
                            crop_w = ALIGN_UP(crop_w, 16);
                            crop_h = ALIGN_UP(crop_h, 2);
                            crop_x = (input_frame->w - crop_w) / 2;
                            crop_y = 0;
                        }
                    } else {
                        // 图像在显示屏外部, 使用比例小的边
                        if (scale_x > scale_y) {        // y边小
                            crop_w = input_frame->w;
                            crop_h = crop_w / scale;    // _w / _h = crop_w / crop_h; crop_w = crop_h * _w / _h;
                            crop_w = ALIGN_UP(crop_w, 16);
                            crop_h = ALIGN_UP(crop_h, 2);
                            crop_x = 0;
                            crop_y = (input_frame->h - crop_h) / 2;
                        } else {                        // x边小
                            crop_h = input_frame->h;
                            crop_w = crop_h * scale;    // _w / _h = crop_w / crop_h; crop_h = crop_w * _h / _w;
                            crop_w = ALIGN_UP(crop_w, 16);
                            crop_h = ALIGN_UP(crop_h, 2);
                            crop_x = (input_frame->w - crop_w) / 2;
                            crop_y = 0;
                        }
                    }
                    AX_VIDEO_FRAME_T tmp_frame;
                    input_frame->get_video_frame(&tmp_frame);
                    tmp_frame.s16CropX = crop_x;
                    tmp_frame.s16CropY = crop_y;
                    tmp_frame.s16CropWidth = crop_w;
                    tmp_frame.s16CropHeight = crop_h;
                    input_frame->set_video_frame(&tmp_frame);

                    // fit success, use new frame
                    output_frame = input_frame;
                    input_frame = nullptr;
                    break;
                }
                default:
                    // do nothing, include image::FIT_FILL
                    output_frame = input_frame;
                    input_frame = nullptr;
                    break;
                }

                if (!output_frame) {
                    log::error("Unable to create a frame for display");
                    ret = err::ERR_RUNTIME;
                    goto _exit;
                }

                switch (format)
                {
                case image::FMT_GRAYSCALE:  // fall through
                case image::FMT_RGB888:
                case image::FMT_YVU420SP:
                case image::FMT_YUV420SP:
                    if (err::ERR_NONE != __vo->push(this->_layer, this->_ch, output_frame)) {
                        log::warn("There's a problem with vo, but if it's just logging during exit, you can disregard it.");
                        ret = err::ERR_RUNTIME;
                        goto _exit;
                    }
                    break;
                case image::FMT_BGRA8888:
                {
                    int width = input_img->width(), height = input_img->height();
                    image::Image *rgb = new image::Image(width, height, image::FMT_RGB888);
                    uint8_t *src = (uint8_t *)input_img->data(), *dst = (uint8_t *)rgb->data();
                    for (int i = 0; i < height; i ++) {
                        for (int j = 0; j < width; j ++) {
                            dst[(i * width + j) * 3 + 0] = src[(i * width + j) * 4 + 2];
                            dst[(i * width + j) * 3 + 1] = src[(i * width + j) * 4 + 1];
                            dst[(i * width + j) * 3 + 2] = src[(i * width + j) * 4 + 0];
                        }
                    }
                    if (0 != __vo->push(this->_layer, this->_ch, output_frame)) {
                        log::warn("There's a problem with vo, but if it's just logging during exit, you can disregard it.");
                        ret = err::ERR_RUNTIME;
                        goto _exit;
                    }
                    delete rgb;
                    break;
                }
                case image::FMT_RGBA8888:
                {
                    int width = input_img->width(), height = input_img->height();
                    image::Image *rgb = new image::Image(width, height, image::FMT_RGB888);
                    uint8_t *src = (uint8_t *)input_img->data(), *dst = (uint8_t *)rgb->data();
                    for (int i = 0; i < height; i ++) {
                        for (int j = 0; j < width; j ++) {
                            dst[(i * width + j) * 3 + 0] = src[(i * width + j) * 4 + 0];
                            dst[(i * width + j) * 3 + 1] = src[(i * width + j) * 4 + 1];
                            dst[(i * width + j) * 3 + 2] = src[(i * width + j) * 4 + 2];
                        }
                    }
                    if (0 != __vo->push(this->_layer, this->_ch, output_frame)) {
                        log::warn("There's a problem with vo, but if it's just logging during exit, you can disregard it.");
                        ret = err::ERR_RUNTIME;
                        goto _exit;
                    }
                    delete rgb;
                    break;
                }
                default:
                    log::error("display layer 0 not support format: %d\n", format);
                    ret = err::ERR_RUNTIME;
                    goto _exit;
                }
            } else if (this->_layer == 1) {
                image::Image *new_img = input_img;
                bool need_delete_new_img = false;
                if (format != image::FMT_BGRA8888) {
                    new_img = input_img->to_format(image::FMT_BGRA8888);
                    err::check_null_raise(new_img, "This image format is not supported, try image::Format::FMT_BGRA8888");
                    need_delete_new_img = true;
                }

                if (input_img->width() != _width || input_img->height() != _height) {
                    log::error("image size not match, you must pass in an image of size %dx%d", _width, _height);
                    err::check_raise(err::ERR_RUNTIME, "image size not match");
                }

                maixcam2::Frame *frame = new maixcam2::Frame(input_img->width(), input_img->height(), input_img->data(), input_img->data_size(), maixcam2::get_ax_fmt_from_maix(input_img->format()));
                if (!frame) {
                    log::error("Failed to create frame");
                    ret = err::ERR_RUNTIME;
                    goto _exit;
                }
                if (0 != __vo->push(this->_layer, this->_ch, frame)) {
                    log::warn("There's a problem with vo, but if it's just logging during exit, you can disregard it.");
                    delete frame;
                    frame = nullptr;
                    ret = err::ERR_RUNTIME;
                    goto _exit;
                }

                if (frame) {
                    delete frame;
                    frame = nullptr;
                }

                if (need_delete_new_img) {
                    delete new_img;
                }
            } else {
                log::error("not support layer: %d\n", this->_layer);
                    ret = err::ERR_ARGS;
                    goto _exit;
            }
_exit:
            if (need_delete_input_img) {
                delete input_img;
                input_img = nullptr;
            }

            if (input_frame) {
                delete input_frame;
                input_frame = nullptr;
            }

            if (output_frame) {
                delete output_frame;
                output_frame = nullptr;
            }
            return ret;
        }

        err::Err push(pipeline::Frame *pipe_frame, image::Fit fit) {
            if (!pipe_frame) return err::ERR_ARGS;
            if (!__vo_is_ready(this->_layer)) {
                return err::ERR_NOT_READY;
            }

            auto frame = (maixcam2::Frame *)pipe_frame->frame();
            err::check_bool_raise((frame->w % 16 == 0), "Image width must be a multiple of 16.");
            err::check_bool_raise((frame->h % 2 == 0), "Image height must be a multiple of 2.");
            maixcam2::Frame *new_frame = nullptr;
            switch (fit) {
            case image::FIT_CONTAIN:
            {
                while (new_frame == nullptr && !app::need_exit()) {
                    try {
                        auto dst_pool_id = _dst_pool.pool_id();
                        auto new_frame_format = pipe_frame->format();
                        new_frame = new maixcam2::Frame(dst_pool_id, _width, _height, nullptr, 0, maixcam2::get_ax_fmt_from_maix(new_frame_format));
                        if (new_frame_format >= image::FMT_YVU420SP && new_frame_format <= image::FMT_YUV420P) {
                            memset(new_frame->data, 0, _width * _height);
                            memset((uint8_t *)(new_frame->data) + _width * _height, 128, _width * _height / 2);
                        } else {
                            memset(new_frame->data, 0, new_frame->len);
                        }
                    } catch (...) {
                        time::sleep_ms(5);
                    }
                }

                if (!new_frame) {
                    return err::ERR_RUNTIME;
                }


                AX_IVPS_CROP_RESIZE_ATTR_T crop_resize_attr;
                memset(&crop_resize_attr, 0, sizeof(AX_IVPS_CROP_RESIZE_ATTR_T));
                crop_resize_attr.tAspectRatio.nBgColor = (AX_U32)0;
                crop_resize_attr.eSclType = AX_IVPS_SCL_TYPE_AUTO;
                crop_resize_attr.eSclInput = AX_IVPS_SCL_INPUT_SHARE;
                crop_resize_attr.tAspectRatio.eMode = AX_IVPS_ASPECT_RATIO_AUTO;
                crop_resize_attr.tAspectRatio.eAligns[0] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
                crop_resize_attr.tAspectRatio.eAligns[1] = AX_IVPS_ASPECT_RATIO_VERTICAL_CENTER;
                crop_resize_attr.tAspectRatio.nBgColor = (AX_U32)0;
                AX_S32 ret = __ax_ivps_crop_resize_tdp(*frame, *new_frame, &crop_resize_attr, false);
                if (ret != 0) {
                    log::info("Failed to fit image, ret:%#x", ret);
                    delete new_frame;
                    return err::ERR_RUNTIME;
                }

                // fit success, use new frame
                frame = new_frame;
                break;
            }
            case image::FIT_COVER:
            {
                float scale_x = (float)_width / frame->w;
                float scale_y = (float)_height / frame->h;
                float scale = (float)_width / _height;
                int crop_x = 0, crop_y = 0, crop_w = frame->w, crop_h = frame->h;
                // log::info("_width:%d _height:%d", _width, _height);
                // log::info("scale x %f, y %f, scale %f", scale_x, scale_y, scale);
                if (frame->w <= _width && frame->h <= _height) {
                    // 图像在显示屏内部, 使用比例小的边
                    if (scale_x > scale_y) {        // x边小
                        crop_w = frame->w;
                        crop_h = crop_w / scale;    // _w / _h = crop_w / crop_h; crop_h = crop_w * _h / _w;
                        crop_x = 0;
                        crop_y = (frame->h - crop_h) / 2;
                    } else {                        // y边小
                        crop_h = frame->h;
                        crop_w = crop_h * scale;    // _w / _h = crop_w / crop_h; crop_w = crop_h * _w / _h;
                        crop_x = (frame->w - crop_w) / 2;
                        crop_y = 0;
                    }
                } else {
                    // 图像在显示屏外部, 使用比例小的边
                    if (scale_x > scale_y) {        // y边小
                        crop_w = frame->w;
                        crop_h = crop_w / scale;    // _w / _h = crop_w / crop_h; crop_w = crop_h * _w / _h;
                        crop_x = 0;
                        crop_y = (frame->h - crop_h) / 2;
                    } else {                        // x边小
                        crop_h = frame->h;
                        crop_w = crop_h * scale;    // _w / _h = crop_w / crop_h; crop_h = crop_w * _h / _w;
                        crop_x = (frame->w - crop_w) / 2;
                        crop_y = 0;
                    }
                }
                AX_VIDEO_FRAME_T tmp_frame;
                frame->get_video_frame(&tmp_frame);
                tmp_frame.s16CropX = crop_x;
                tmp_frame.s16CropY = crop_y;
                tmp_frame.s16CropWidth = crop_w;
                tmp_frame.s16CropHeight = crop_h;
                frame->set_video_frame(&tmp_frame);
                break;
            }
            default:
                // do nothing, include image::FIT_FILL
                break;
            }

            if (0 != __vo->push(this->_layer, this->_ch, frame)) {
                log::error("vo_frame_push failed\n");
                if (new_frame) {
                    delete new_frame;
                    new_frame = nullptr;
                }
                return err::ERR_RUNTIME;
            }

            if (new_frame) {
                delete new_frame;
                new_frame = nullptr;
            }
            return err::ERR_NONE;
        }

        void set_backlight(float value)
        {
            if (_bl_pwm) {
                _bl_pwm->duty(value * _max_backlight / 100.0);
                // _bl_pwm->disable();
                if(value == 0)
                    return;
                // _bl_pwm->enable();
            }
        }

        float get_backlight()
        {
            return _bl_pwm->duty() / _max_backlight * 100;
        }

        int get_ch_nums()
        {
            return 2;
        }

        err::Err set_hmirror(bool en) {
            _invert_mirror = en;

            bool need_open = false;
            if (this->_opened) {
                this->close();
                need_open = true;
            }

            if (need_open) {
                err::check_raise(this->open(this->_width, this->_height, this->_format), "Open failed");
            }
            return err::ERR_NONE;
        }

        err::Err set_vflip(bool en) {
            _invert_flip = en;

            bool need_open = false;
            if (this->_opened) {
                this->close();
                need_open = true;
            }

            if (need_open) {
                err::check_raise(this->open(this->_width, this->_height, this->_format), "Open failed");
            }
            return err::ERR_NONE;
        }

    private:
        int _width;
        int _height;
        int _max_width;
        int _max_height;
        image::Format _format;
        int _layer;
        int _ch;
        bool _opened;
        bool _invert_flip;
        bool _invert_mirror;
        float _max_backlight;
        CmmPool _src_pool;
        CmmPool _dst_pool;
        pwm::PWM *_bl_pwm;
    };
}
