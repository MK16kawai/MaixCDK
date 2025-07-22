/**
 * @author neucrack@sipeed, lxowalle@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2023.9.8: Add framework, create this file.
 */


#include "maix_camera.hpp"
#include "maix_pipeline.hpp"
#include "maix_basic.hpp"
#include "maix_i2c.hpp"
#include <dirent.h>
#include "ax_middleware.hpp"

using namespace maix;
using namespace maix::middleware::maixcam2;
#define ALIGN_UP_2(value) ((value + 0x1) & (~0x1))
#define ALIGN_UP_16(value) ((value + 0xF) & (~0xF))

namespace maix::camera
{
    static bool set_regs_flag = false;

    std::vector<std::string> list_devices()
    {
        log::warn("This device is not driven using device files!");
        return std::vector<std::string>();
    }

    std::string get_device_name()
    {
        peripheral::i2c::I2C i2c_obj(0, peripheral::i2c::Mode::MASTER);

        std::vector<int> addr_list = i2c_obj.scan();
        for (size_t i = 0; i < addr_list.size(); i++) {
            switch (addr_list[i]) {
                case 0x29:  // gcore_gc4653
                    return "gcore_gc4653";
                case 0x30:  // sc850sl
                    return "smartsens_sc850sl";
                case 0x2b:  // lt6911
                    return "lt6911";
                case 0x36:  // ov_os04a10
                    return "ov_os04a10";
                case 0x37:  // gcore_gc02m1
                    return "gcore_gc02m1";
                case 0x48:// fall through
                case 0x3c:  // os04d10 ??ov_ov2685
                    return "ov_os04d10";
                default: break;
            }
        }
        err::check_raise(err::ERR_RUNTIME, "Not found any sensor");
        return "unknown";
    }

    void set_regs_enable(bool enable) {
        log::warn("This operation is not supported!");
    }


    static void __generate_colorbar(image::Image &img)
    {
        int width = img.width();
        int height = img.height();
        int step = width / 8;
        int color_step = 255 / 7;
        int color = 0;
        uint8_t colors[8][3] = {
            {255, 255, 255},
            {255, 0, 0},
            {255, 127, 0},
            {255, 255, 0},
            {0, 255, 0},
            {0, 0, 255},
            {143, 0, 255},
            {0, 0, 0},
        };
        for (int i = 0; i < 8; i++)
        {
            image::Color _color(colors[i][0], colors[i][1], colors[i][2], 0, image::FMT_RGB888);
            img.draw_rect(i * step, 0, step, height, _color, -1);
            color += color_step;
        }
    }

    bool Camera::_check_format(image::Format format) {
        if (format == image::FMT_RGB888 || format == image::FMT_BGR888
        || format == image::FMT_RGBA8888 || format == image::FMT_BGRA8888
        || format == image::FMT_YVU420SP || format == image::FMT_GRAYSCALE) {
            return true;
        } else {
            return false;
        }
    }

    static bool __check_board_config_path()
    {
        if (fs::exists("/boot/board")) {
            return true;
        }
        return false;
    }

    static void __get_cam_flip_mirror(int &new_flip, int &new_mirror) {
        bool flip = 0, mirror = 0;
        bool flip_is_found = false, mirror_is_found = false;

        if (__check_board_config_path()) {
            std::string flip_str;
            std::string mirror_str;
            auto device_configs = sys::device_configs();
            auto it = device_configs.find("cam_flip");
            if (it != device_configs.end()) {
                flip_str = it->second;
                flip_is_found = true;
            }
            auto it2 = device_configs.find("cam_mirror");
            if (it2 != device_configs.end()) {
                mirror_str = it2->second;
                mirror_is_found = true;
            }

            std::string board_id = sys::device_id();
            // log::info("cam flip=%s, cam mirror=%s", flip_str.c_str(), mirror_str.c_str());
            if (flip_is_found && !flip_str.empty()) {
                flip = atoi(flip_str.c_str());
            } else {
                if (board_id == "maixcam2") {
                    flip = true;
                } else {
                    flip = false;
                }
            }

            if (mirror_is_found && !mirror_str.empty()) {
                mirror = atoi(mirror_str.c_str());
            } else {
                std::string board_id = sys::device_id();
                if (board_id == "maixcam2") {
                    mirror = true;
                } else {
                    mirror = false;
                }
            }
        }

        new_flip = flip;
        new_mirror = mirror;
    }

    // static bool _get_board_config_path(char *path, int path_size)
    // {
    //     if (fs::exists("/boot/board")) {
    //         snprintf(path, path_size, "/boot/board");
    //         return true;
    //     }
    //     return false;
    // }

    // static int _get_mclk_id(void) {
    //     char path[64];
    //     int mclk_id = 0;

    //     err::check_bool_raise(_get_board_config_path(path, sizeof(path)), "Can't find board config file");

    //     std::string mclk_id_str;
    //     auto device_configs = sys::device_configs();
    //     auto it = device_configs.find("cam_mclk");
    //     if (it != device_configs.end()) {
    //         mclk_id_str = it->second;
    //     }
    //     if (!mclk_id_str.empty()) {
    //         mclk_id = atoi(mclk_id_str.c_str());
    //     } else {
    //         std::string board_id = sys::device_id();
    //         if (board_id == "maixcam_pro") {
    //             mclk_id = 0;
    //         } else {
    //             mclk_id = 1;
    //         }
    //     }
    //     return mclk_id;
    // }

    // static std::vector<int> _get_lane_id_from_board_file() {
    //     std::vector<int> lane_id;
    //     auto device_configs = sys::device_configs();
    //     auto it = device_configs.find("lane_id");
    //     if (it != device_configs.end()) {
    //         auto lane_id_str = it->second;
    //         std::string item;
    //         std::stringstream ss(lane_id_str);
    //         while (std::getline(ss, item, ',')) {
    //             lane_id.push_back(std::stoi(item));
    //         }
    //     }

    //     return lane_id;
    // }

    // static std::vector<int> _get_pn_swap_from_board_file() {
    //     std::vector<int> pn_swap;
    //     auto device_configs = sys::device_configs();
    //     auto it = device_configs.find("pn_swap");
    //     if (it != device_configs.end()) {
    //         auto pn_swap_str = it->second;
    //         std::string item;
    //         std::stringstream ss(pn_swap_str);
    //         while (std::getline(ss, item, ',')) {
    //             pn_swap.push_back(std::stoi(item));
    //         }
    //     }

    //     return pn_swap;
    // }

    // static std::vector<bool> _get_cam_flip_mirror(void) {
    //     char path[64];
    //     bool flip = 0, mirror = 0;
    //     bool flip_is_found = false, mirror_is_found = false;

    //     err::check_bool_raise(_get_board_config_path(path, sizeof(path)), "Can't find board config file");

    //     std::string flip_str;
    //     std::string mirror_str;
    //     auto device_configs = sys::device_configs();
    //     auto it = device_configs.find("cam_flip");
    //     if (it != device_configs.end()) {
    //         flip_str = it->second;
    //         flip_is_found = true;
    //     }
    //     auto it2 = device_configs.find("cam_mirror");
    //     if (it2 != device_configs.end()) {
    //         mirror_str = it2->second;
    //         mirror_is_found = true;
    //     }

    //     std::string board_id = sys::device_id();
    //     // log::info("cam flip=%s, cam mirror=%s", flip_str, mirror_str);
    //     if (flip_is_found && !flip_str.empty()) {
    //         flip = atoi(flip_str.c_str());
    //     } else {
    //         if (board_id == "maixcam_pro") {
    //             flip = true;
    //         } else {
    //             flip = false;
    //         }
    //     }

    //     if (mirror_is_found && !mirror_str.empty()) {
    //         mirror = atoi(mirror_str.c_str());
    //     } else {
    //         std::string board_id = sys::device_id();
    //         if (board_id == "maixcam_pro") {
    //             mirror = true;
    //         } else {
    //             mirror = false;
    //         }
    //     }

    //     return {flip, mirror};
    // }

    // static image::Format _get_raw_format_with_size(int w, int h, int total_size, AX_IMG_FORMAT_E bayer_format) {
    //     image::Format format = image::FMT_INVALID;
    //     // int size = w * h;
    //     // if (total_size == size * 0.75) {
    //     //     switch (bayer_format) {
    //     //     case BAYER_FORMAT_BG:
    //     //         format = image::FMT_BGGR6;
    //     //         break;
    //     //     case BAYER_FORMAT_GB:
    //     //         format = image::FMT_GBRG6;
    //     //         break;
    //     //     case BAYER_FORMAT_GR:
    //     //         format = image::FMT_GRBG6;
    //     //         break;
    //     //     case BAYER_FORMAT_RG:
    //     //         format = image::FMT_RGGB6;
    //     //         break;
    //     //     default:
    //     //         return image::FMT_INVALID;
    //     //     }
    //     // } else if (total_size == size * 1) {
    //     //     switch (bayer_format) {
    //     //     case BAYER_FORMAT_BG:
    //     //         format = image::FMT_BGGR8;
    //     //         break;
    //     //     case BAYER_FORMAT_GB:
    //     //         format = image::FMT_GBRG8;
    //     //         break;
    //     //     case BAYER_FORMAT_GR:
    //     //         format = image::FMT_GRBG8;
    //     //         break;
    //     //     case BAYER_FORMAT_RG:
    //     //         format = image::FMT_RGGB8;
    //     //         break;
    //     //     default:
    //     //         return image::FMT_INVALID;
    //     //     }
    //     // } else if (total_size == size * 1.25) {
    //     //     switch (bayer_format) {
    //     //     case BAYER_FORMAT_BG:
    //     //         format = image::FMT_BGGR10;
    //     //         break;
    //     //     case BAYER_FORMAT_GB:
    //     //         format = image::FMT_GBRG10;
    //     //         break;
    //     //     case BAYER_FORMAT_GR:
    //     //         format = image::FMT_GRBG10;
    //     //         break;
    //     //     case BAYER_FORMAT_RG:
    //     //         format = image::FMT_RGGB10;
    //     //         break;
    //     //     default:
    //     //         return image::FMT_INVALID;
    //     //     }
    //     // } else if (total_size == size * 1.5) {
    //     //     switch (bayer_format) {
    //     //     case BAYER_FORMAT_BG:
    //     //         format = image::FMT_BGGR12;
    //     //         break;
    //     //     case BAYER_FORMAT_GB:
    //     //         format = image::FMT_GBRG12;
    //     //         break;
    //     //     case BAYER_FORMAT_GR:
    //     //         format = image::FMT_GRBG12;
    //     //         break;
    //     //     case BAYER_FORMAT_RG:
    //     //         format = image::FMT_RGGB12;
    //     //         break;
    //     //     default:
    //     //         return image::FMT_INVALID;
    //     //     }
    //     // } else {
    //     //     return image::FMT_INVALID;
    //     // }

    //     return format;
    // }

    std::vector<int> get_sensor_size()
    {
        peripheral::i2c::I2C i2c_obj(0, peripheral::i2c::Mode::MASTER);

        std::vector<int> addr_list = i2c_obj.scan();
        for (size_t i = 0; i < addr_list.size(); i++) {
            switch (addr_list[i]) {
                case 0x29:  // gcore_gc4653
                    return {2560, 1440};
                case 0x30:  // sc850sl
                    return {3840, 2160};
                case 0x2b:  // lt6911
                    return {1920, 1080};
                case 0x36:  // ov_os04a10
                    return {2560, 1440};
                case 0x37:  // gcore_gc02m1
                    return {1600, 1200};
                case 0x48:// fall through
                case 0x3c:  // os04d10 ??ov_ov2685
                    return {2560, 1440};
                default: break;
            }
        }
        err::check_raise(err::ERR_RUNTIME, "Not found any sensor");
        return {0, 0};
    }

    typedef struct {
        int i2c_addr;
        bool raw;
        bool flip;
        bool mirror;
        int exptime_max;    // unit:us
        int exptime_min;

        SYS *ax_sys;
        VI *ax_vi;

        struct {
            int id;
            int w;
            int h;
            image::Format fmt;
            int fps;
            int depth;
            int mirror;
            int vflip;
            int fit;
        } chn;
    } camera_priv_t;

    Camera::Camera(int width, int height, image::Format format, const char *device, double fps, int buff_num, bool open, bool raw)
    {
        err::Err e;
        err::check_bool_raise(_check_format(format), "Format not support");

        _width = (width == -1) ? 640 : width;
        _height = (height == -1) ? 480 : height;
        _format = format;
        _buff_num = buff_num > 3 ? buff_num : 3;
        _device =  device ? std::string(device) : "";
        _fps = fps;

        _show_colorbar = false;
        _open_set_regs = set_regs_flag;

        _is_opened = false;

        camera_priv_t *priv = (camera_priv_t *)malloc(sizeof(camera_priv_t));
        err::check_null_raise(priv, "camera_priv_t malloc error");
        memset(priv, 0, sizeof(camera_priv_t));
        priv->raw = raw;
        _param = priv;

        // open camera
        if (open) {
            e = this->open(_width, _height, _format, _fps, _buff_num);
            err::check_raise(e, "camera open failed");
        }
    }

    Camera::~Camera()
    {
        if (this->is_opened()) {
            this->close();
        }

        if (_param) {
            ::free(_param);
        }
    }

    int Camera::get_ch_nums()
    {
        return AX_IVPS_MAX_OUTCHN_NUM;
    }

    int Camera::get_channel()
    {
        camera_priv_t *priv = (camera_priv_t *)_param;
        return priv->chn.id;
    }

    err::Err Camera::open(int width, int height, image::Format format, double fps, int buff_num)
    {

        err::Err err = err::ERR_NONE;
        int width_tmp = (width == -1) ? _width : width;
        int height_tmp = (height == -1) ? _height : height;
        image::Format format_tmp = (format == image::FMT_INVALID) ? _format : format;
        double fps_tmp = (fps == -1) ? _fps : fps;
        int buff_num_tmp =( buff_num == -1) ? _buff_num : buff_num;
        camera_priv_t *priv = (camera_priv_t *)_param;

        // check resolution
        if (format == image::FMT_RGB888) {
            err::check_bool_raise(width_tmp < 1920 && height_tmp < 1080, "The resolution of an image in RGB888 format must not exceed 1920x1080");
        }

        // check format
        err::check_bool_raise(_check_format(format_tmp), "Format not support");

        // init sys
        SYS *ax_sys = new SYS(priv->raw);
        err::check_null_raise(ax_sys, "ax sys malloc error");
        err = ax_sys->init();
        if (err::ERR_NONE != err) {
            log::info("Init ax sys failed");
            return err;
        }

        auto &mod_param = AxModuleParam::getInstance();
        mod_param.lock(AX_MOD_VI);
        auto vi_param = (ax_vi_mod_t *)mod_param.get_param(AX_MOD_VI);
        auto ax_cam = vi_param->cams[0];
        auto p_ax_cam = &ax_cam;
        mod_param.unlock(AX_MOD_VI);
        priv->i2c_addr = p_ax_cam->nI2cAddr;

        // init vi
        VI *ax_vi = new VI();
        err = ax_vi->init();
        if (err != err::ERR_NONE) {
            delete ax_vi;
            delete ax_sys;
            err::check_raise(err, "Init ax vi failed");
        }

        if (priv->raw) {
            AX_VIN_DUMP_ATTR_T stDumpAttr;
            stDumpAttr.bEnable = AX_TRUE;
            stDumpAttr.nDepth = 1;
            AX_S32 axRet = AX_VIN_SetPipeDumpAttr(0, AX_VIN_PIPE_DUMP_NODE_IFE, AX_VIN_DUMP_QUEUE_TYPE_DEV, &stDumpAttr);
            if (0 != axRet) {
                log::error("AX_VIN_SetPipeDumpAttr failed, ret=0x%x\n", axRet);
            }
        }

        auto ch = ax_vi->get_unused_channel();
        if (ch == -1) {
            delete ax_vi;
            delete ax_sys;
            err::check_raise(err::ERR_RUNTIME, "Get unused channel failed");
        }

        int fit = 2;
        err = ax_vi->add_channel(ch, ALIGN_UP_16(width_tmp), ALIGN_UP_2(height_tmp), get_ax_fmt_from_maix(format_tmp), (int)fps_tmp, buff_num_tmp, false, false, fit);
        if (err != err::ERR_NONE) {
            delete ax_vi;
            delete ax_sys;
            err::check_raise(err, "Add channel failed");
        }

        __get_cam_flip_mirror(priv->chn.vflip, priv->chn.mirror);
        priv->ax_sys = ax_sys;
        priv->ax_vi = ax_vi;
        priv->chn.id = ch;
        priv->chn.w = width_tmp;
        priv->chn.h = height_tmp;
        priv->chn.fmt = format_tmp;
        priv->chn.fit = fit;
        _ch = ch;
        _is_opened = true;

        this->vflip(priv->chn.vflip);
        this->hmirror(priv->chn.mirror);
        return err::ERR_NONE;
    }

    void Camera::close()
    {
        camera_priv_t *priv = (camera_priv_t *)_param;
        if (this->is_closed())
            return;

        priv->ax_vi->del_channel(priv->chn.id);
        priv->ax_vi->deinit();
        delete priv->ax_vi;
        delete priv->ax_sys;

        _is_opened = false;
    }

    camera::Camera *Camera::add_channel(int width, int height, image::Format format, double fps, int buff_num, bool open)
    {
        err::check_bool_raise(_check_format(format), "Format not support");

        int width_tmp = (width <= 0) ? _width : width;
        int height_tmp = (height <= 0) ? _height : height;
        image::Format format_tmp = (format == image::Format::FMT_INVALID) ? _format : format;
        double fps_tmp = (fps == -1) ? _fps : fps;
        int buff_num_tmp = buff_num == -1 ? _buff_num : buff_num;

        Camera *cam = new Camera(width_tmp, height_tmp, format_tmp, _device.c_str(), fps_tmp, buff_num_tmp, true);
        return cam;
    }

    bool Camera::is_opened()
    {
        return _is_opened;
    }

    image::Image *Camera::read(void *buff, size_t buff_size, bool block, int block_ms)
    {
        auto *priv = (camera_priv_t *)_param;
        auto vi = priv->ax_vi;
        if (!this->is_opened()) {
            err::Err e = open(_width, _height, _format, _fps, _buff_num);
            err::check_raise(e, "open camera failed");
        }

        if (_show_colorbar) {
            image::Image *img = new image::Image(_width, _height);
            __generate_colorbar(*img);
            err::check_null_raise(img, "camera read failed");
            return img;
        } else {
            if (block) block_ms = -1;
            auto frame = vi->pop(priv->chn.id, block_ms);
            if (frame == nullptr) {
                err::check_raise(err::ERR_BUFF_EMPTY, "read camera failed");
            }

            auto pipeline_frame = pipeline::Frame(frame, true);
            auto img = pipeline_frame.to_image();
            return img;
        }
    }

    image::Image *Camera::read_raw() {
        if (!this->is_opened()) {
            err::Err e = open(_width, _height, _format, _fps, _buff_num);
            err::check_raise(e, "open camera failed");
        }

        camera_priv_t *priv = (camera_priv_t *)this->_param;
        if (!priv->raw) {
            err::check_raise(err::ERR_NOT_READY, "you need to enable the raw parameter when constructing the Camera object.");
        }
        auto vi = priv->ax_vi;

        auto frame = vi->pop_raw(priv->chn.id, 5000);
        if (frame == nullptr) {
            err::check_raise(err::ERR_BUFF_EMPTY, "read camera failed");
        }

        auto img = new image::Image(frame->w, frame->h, image::FMT_RGGB10, (uint8_t *)frame->data, frame->len, false);
        delete frame;
        return img;
    }

    pipeline::Frame *Camera::pop(int block_ms) {
        auto *priv = (camera_priv_t *)_param;
        auto vi = priv->ax_vi;
        if (!this->is_opened()) {
            err::Err e = open(_width, _height, _format, _fps, _buff_num);
            err::check_raise(e, "open camera failed");
        }

        auto frame = vi->pop(priv->chn.id, block_ms);
        if (!frame)
            return nullptr;

        return new pipeline::Frame(frame, true);
    }

    err::Err Camera::show_colorbar(bool enable)
    {
        // only set variable now
        // should control camera to show colorbar
        _show_colorbar = enable;
        return err::ERR_NONE;
    }

    void Camera::clear_buff()
    {
        log::warn("This operation is not supported!");
    }

    void Camera::skip_frames(int num)
    {
        for(int i = 0; i < num; i++)
        {
            image::Image *img = this->read();
            delete img;
        }
    }

    err::Err Camera::set_resolution(int width, int height)
    {
        err::Err ret = err::ERR_NONE;
        auto *priv = (camera_priv_t *)_param;
        auto vi = priv->ax_vi;

        ret = vi->del_channel(priv->chn.id);
        err::check_raise(ret, "del channel failed");
        ret = vi->add_channel(priv->chn.id, width, height, get_ax_fmt_from_maix(priv->chn.fmt), priv->chn.fps,
                            priv->chn.depth, false, false, priv->chn.fit);
        err::check_raise(ret, "del channel failed");
        return ret;
    }

    err::Err Camera::set_fps(double fps) {
        err::Err ret = err::ERR_NONE;
        this->exposure(1000 / fps * 1000);
        _fps = fps;
        return ret;
    }

    int Camera::exposure(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res = AX_SUCCESS;
        AX_ISP_IQ_AE_PARAM_T tAeParam;
        int current_exposure_time = 0;

        ax_res = AX_ISP_IQ_GetAeParam(_ch, &tAeParam);
        if (ax_res != AX_SUCCESS) {
            log::info("get ae param failed");
            return err::ERR_RUNTIME;
        }

        if (value > 0) {
            tAeParam.nEnable = false;
            tAeParam.tExpManual.nShutter = value;
            tAeParam.tExpManual.nShortShutter = value / 16;
            tAeParam.tExpManual.nVsShutter = value;
            ax_res = AX_ISP_IQ_SetAeParam(_ch, &tAeParam);
            if (ax_res != AX_SUCCESS) {
                log::info("set ae param failed");
                return err::ERR_RUNTIME;
            }
        }

        ax_res = AX_ISP_IQ_GetAeParam(_ch, &tAeParam);
        if (ax_res != AX_SUCCESS) {
            log::info("get ae param failed");
            return err::ERR_RUNTIME;
        }
        current_exposure_time = tAeParam.tExpManual.nShutter;
        return current_exposure_time;
    }

    int Camera::gain(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res = AX_SUCCESS;
        AX_ISP_IQ_AE_PARAM_T tAeParam;
        int current_gain = 0;

        ax_res = AX_ISP_IQ_GetAeParam(_ch, &tAeParam);
        if (ax_res != AX_SUCCESS) {
            log::info("get ae param failed");
            return err::ERR_RUNTIME;
        }

        if (value > 0) {
            tAeParam.nEnable = false;
            // tAeParam.tExpManual.nSysTotalGain = value;
            // tAeParam.tExpManual.nIspGain = value;
            tAeParam.tExpManual.nAGain = value;
            ax_res = AX_ISP_IQ_SetAeParam(_ch, &tAeParam);
            if (ax_res != AX_SUCCESS) {
                log::info("set ae param failed");
                return err::ERR_RUNTIME;
            }
        }

        ax_res = AX_ISP_IQ_GetAeParam(_ch, &tAeParam);
        if (ax_res != AX_SUCCESS) {
            log::info("get ae param failed");
            return err::ERR_RUNTIME;
        }
        current_gain = tAeParam.tExpManual.nSysTotalGain;
        return current_gain;
    }

    int Camera::iso(int value) {
        int gain_vale = (double)value / 100.0 * 1024;
        return this->gain(gain_vale);
    }

    int Camera::hmirror(int value) {
        if (!this->is_opened()) {
            return -1;
        }

        auto *priv = (camera_priv_t *)_param;
        peripheral::i2c::I2C i2c_obj(0, peripheral::i2c::Mode::MASTER);
        int mirror = false;
        switch (priv->i2c_addr) {
        case 0x30:
        {
            uint8_t reg_val = 0;
            uint16_t reg_addr = 0x3221;
            auto data = i2c_obj.readfrom_mem(priv->i2c_addr, reg_addr, 1, 16);
            if (!data) {
                return -1;
            }
            #define SC850SL_MIRROR_MASK (0x60)
            reg_val = data->data[0];
            mirror = (reg_val & SC850SL_MIRROR_MASK) ? true : false;
            if (value >= 0) {
                mirror = value > 0 ? true : false;
                if (mirror) {
                    reg_val = (reg_val & ~SC850SL_MIRROR_MASK) | SC850SL_MIRROR_MASK;
                } else {
                    reg_val = (reg_val & ~SC850SL_MIRROR_MASK);
                }
                uint8_t temp[1] = {reg_val};
                i2c_obj.writeto_mem(priv->i2c_addr, reg_addr, temp, sizeof(temp), 16);
            } else {

            }
        }
        break;
        case 0x36:
        {
            uint8_t reg_val = 0;
            uint16_t reg_addr = 0x3820;
            auto data = i2c_obj.readfrom_mem(priv->i2c_addr, reg_addr, 1, 16);
            if (!data) {
                return -1;
            }
            #define OS04A10_MIRROR_MASK (0x02)
            reg_val = data->data[0];
            mirror = (reg_val & OS04A10_MIRROR_MASK) ? true : false;
            if (value >= 0) {
                mirror = value > 0 ? true : false;
                if (mirror) {
                    reg_val = (reg_val & ~OS04A10_MIRROR_MASK) | OS04A10_MIRROR_MASK;
                } else {
                    reg_val = (reg_val & ~OS04A10_MIRROR_MASK);
                }
                uint8_t temp[1] = {reg_val};
                i2c_obj.writeto_mem(priv->i2c_addr, reg_addr, temp, sizeof(temp), 16);
            } else {

            }
        }
        break;
        case 0x3c:
        {
            uint8_t reg_val = 0;
            uint16_t reg_addr = 0x32;
            uint8_t temp[1] = {0x01};
            i2c_obj.writeto_mem(priv->i2c_addr, 0xfd, temp, sizeof(temp), 8);   // set page 0
            auto data = i2c_obj.readfrom_mem(priv->i2c_addr, reg_addr, 1, 8);
            if (!data) {
                return -1;
            }
            #define OS04D10_MIRROR_MASK (0x01)
            reg_val = data->data[0];
            mirror = (reg_val & OS04D10_MIRROR_MASK) ? true : false;
            if (value >= 0) {
                mirror = value > 0 ? true : false;
                if (mirror) {
                    reg_val = (reg_val & ~OS04D10_MIRROR_MASK) | OS04D10_MIRROR_MASK;
                } else {
                    reg_val = (reg_val & ~OS04D10_MIRROR_MASK);
                }
                temp[0] = {reg_val};
                i2c_obj.writeto_mem(priv->i2c_addr, reg_addr, temp, sizeof(temp), 8);
            } else {

            }
        }
        break;
        default:
        break;
        }

        priv->chn.mirror = mirror;
        return mirror;
    }

    int Camera::vflip(int value) {
        if (!this->is_opened()) {
            return -1;
        }

        auto *priv = (camera_priv_t *)_param;
        peripheral::i2c::I2C i2c_obj(0, peripheral::i2c::Mode::MASTER);
        int flip = false;
        switch (priv->i2c_addr) {
        case 0x30:
        {
            uint8_t reg_val = 0;
            uint16_t reg_addr = 0x3221;
            auto data = i2c_obj.readfrom_mem(priv->i2c_addr, reg_addr, 1, 16);
            if (!data) {
                return -1;
            }
            #define SC850SL_FLIP_MASK (0x6)
            reg_val = data->data[0];
            flip = (reg_val & SC850SL_FLIP_MASK) ? true : false;
            if (value >= 0) {
                flip = value > 0 ? true : false;
                if (flip) {
                    reg_val = (reg_val & ~SC850SL_FLIP_MASK) | SC850SL_FLIP_MASK;
                } else {
                    reg_val = (reg_val & ~SC850SL_FLIP_MASK);
                }
                uint8_t temp[1] = {reg_val};
                i2c_obj.writeto_mem(priv->i2c_addr, reg_addr, temp, sizeof(temp), 16);
            } else {

            }
        }
        break;
        case 0x36:
        {
            uint8_t reg_val = 0;
            uint16_t reg_addr = 0x3820;
            auto data = i2c_obj.readfrom_mem(priv->i2c_addr, reg_addr, 1, 16);
            if (!data) {
                return -1;
            }
            #define OS04A10_FLIP_MASK (0x04)
            reg_val = data->data[0];
            flip = (reg_val & OS04A10_FLIP_MASK) ? true : false;
            if (value >= 0) {
                flip = value > 0 ? true : false;
                if (flip) {
                    reg_val = (reg_val & ~OS04A10_FLIP_MASK) | OS04A10_FLIP_MASK;
                } else {
                    reg_val = (reg_val & ~OS04A10_FLIP_MASK);
                }
                uint8_t temp[1] = {reg_val};
                i2c_obj.writeto_mem(priv->i2c_addr, reg_addr, temp, sizeof(temp), 16);
            } else {

            }
        }
        break;
        case 0x3c:
        {
            uint8_t reg_val = 0;
            uint16_t reg_addr = 0x32;
            uint8_t temp[1] = {0x01};
            i2c_obj.writeto_mem(priv->i2c_addr, 0xfd, temp, sizeof(temp), 8);   // set page 0
            auto data = i2c_obj.readfrom_mem(priv->i2c_addr, reg_addr, 1, 8);
            if (!data) {
                return -1;
            }
            #define OS04D10_FLIP_MASK (0x02)
            reg_val = data->data[0];
            flip = (reg_val & OS04D10_FLIP_MASK) ? true : false;
            if (value >= 0) {
                flip = value > 0 ? true : false;
                if (flip) {
                    reg_val = (reg_val & ~OS04D10_FLIP_MASK) | OS04D10_FLIP_MASK;
                } else {
                    reg_val = (reg_val & ~OS04D10_FLIP_MASK);
                }
                temp[0] = reg_val;
                i2c_obj.writeto_mem(priv->i2c_addr, reg_addr, temp, sizeof(temp), 8);
            } else {

            }
        }
        default:
        break;
        }

        priv->chn.vflip = flip;
        return flip;
    }

    int Camera::luma(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res = AX_SUCCESS;
        AX_ISP_IQ_YCPROC_PARAM_T param;
        int current_value = 0;
        if (value >= 0) {
            ax_res = AX_ISP_IQ_GetYcprocParam(_ch, &param);
            if (ax_res != AX_SUCCESS) {
                log::error("AX_ISP_IQ_GetYcprocParam failed: %d", ax_res);
                return current_value;
            }

            param.nBrightness = ((double)value / 100) * 4096;
            param.nBrightness = param.nBrightness < 0 ? 0 : param.nBrightness;
            param.nBrightness = param.nBrightness > 4095 ? 4095 : param.nBrightness;
            ax_res = AX_ISP_IQ_SetYcprocParam(_ch, &param);
            if (ax_res != AX_SUCCESS) {
                log::error("AX_ISP_IQ_SetYcprocParam failed: %d", ax_res);
                return current_value;
            }
        }

        ax_res = AX_ISP_IQ_GetYcprocParam(_ch, &param);
        if (ax_res != AX_SUCCESS) {
            log::error("AX_ISP_IQ_GetYcprocParam failed: %d", ax_res);
            return current_value;
        }

        current_value = (float)param.nBrightness / 4096 * 100;
        return current_value;
    }

    int Camera::constrast(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res = AX_SUCCESS;
        AX_ISP_IQ_YCPROC_PARAM_T param;
        int current_value = 0;
        if (value >= 0) {
            ax_res = AX_ISP_IQ_GetYcprocParam(_ch, &param);
            if (ax_res != AX_SUCCESS) {
                log::error("AX_ISP_IQ_GetYcprocParam failed: %d", ax_res);
                return current_value;
            }

            param.nContrast = ((double)value / 100) * 8192 - 4096;
            param.nContrast = param.nContrast < -4096 ? -4096 : param.nContrast;
            param.nContrast = param.nContrast > 4095 ? 4095 : param.nContrast;
            ax_res = AX_ISP_IQ_SetYcprocParam(_ch, &param);
            if (ax_res != AX_SUCCESS) {
                log::error("AX_ISP_IQ_SetYcprocParam failed: %d", ax_res);
                return current_value;
            }
        }

        ax_res = AX_ISP_IQ_GetYcprocParam(_ch, &param);
        if (ax_res != AX_SUCCESS) {
            log::error("AX_ISP_IQ_GetYcprocParam failed: %d", ax_res);
            return current_value;
        }

        current_value = (float)(param.nContrast + 4096) / 8196 * 100;
        return current_value;
    }

    int Camera::saturation(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res = AX_SUCCESS;
        AX_ISP_IQ_YCPROC_PARAM_T param;
        int current_value = 0;
        if (value >= 0) {
            ax_res = AX_ISP_IQ_GetYcprocParam(_ch, &param);
            if (ax_res != AX_SUCCESS) {
                log::error("AX_ISP_IQ_GetYcprocParam failed: %d", ax_res);
                return current_value;
            }

            param.nSaturation = ((double)value / 100) * 65535;
            param.nSaturation = param.nSaturation < 0 ? 0 : param.nSaturation;
            param.nSaturation = param.nSaturation > 65535 ? 65535 : param.nSaturation;
            ax_res = AX_ISP_IQ_SetYcprocParam(_ch, &param);
            if (ax_res != AX_SUCCESS) {
                log::error("AX_ISP_IQ_SetYcprocParam failed: %d", ax_res);
                return current_value;
            }
        }

        ax_res = AX_ISP_IQ_GetYcprocParam(_ch, &param);
        if (ax_res != AX_SUCCESS) {
            log::error("AX_ISP_IQ_GetYcprocParam failed: %d", ax_res);
            return current_value;
        }

        current_value = (float)param.nSaturation / 65536 * 100;log::info("2param.nSaturation: %d", param.nSaturation);
        return current_value;
    }

    int Camera::awb_mode(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res;
        AX_ISP_IQ_AWB_PARAM_T param;
        int current_mode = 0;

        if (value >= 0) {
            ax_res = AX_ISP_IQ_GetAwbParam(_ch, &param);
            if (ax_res != 0) {
                log::error("AX_ISP_IQ_GetAwbParam failed: %d", ax_res);
                return current_mode;
            }

            current_mode = value ? 0 : 1; // 0,manual; 1,auto
            param.nEnable = current_mode;
            ax_res = AX_ISP_IQ_SetAwbParam(_ch, &param);
            if (ax_res != 0) {
                log::error("AX_ISP_IQ_SetAwbParam failed: %d", ax_res);
                return current_mode;
            }
        }

        ax_res = AX_ISP_IQ_GetAwbParam(_ch, &param);
        if (ax_res != 0) {
            log::error("AX_ISP_IQ_GetAwbParam failed: %d", ax_res);
            return current_mode;
        }

        current_mode = param.nEnable ? 0 : 1;
        return current_mode;
    }

    int Camera::set_awb(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }
        err::check_raise(err::ERR_NOT_IMPL, "set awb failed");
        return -1;
    }

    static uint16_t get_gain_float2u16(float gain) {
        uint16_t new_gain = 0;
        gain = gain > 1 ? 1 : gain;
        gain = gain < 0 ? 0 : gain;
        new_gain = (4095 - 256) * gain + 256;
        return new_gain;
    }

    static float get_gain_u162float(uint16_t gain) {
        float new_gain = 0;
        new_gain = (gain - 256) / (4095 - 256);
        return new_gain;
    }

    std::vector<float> Camera::set_wb_gain(std::vector<float> gains) {
        std::vector<float> new_gains;
        if (!this->is_opened()) {
            log::warn("Camera is not opened");
            return new_gains;
        }

        AX_S32 ax_res;
        AX_ISP_IQ_AWB_PARAM_T param;
        if (gains.size() >= 4) {
            ax_res = AX_ISP_IQ_GetAwbParam(_ch, &param);
            if (ax_res != 0) {
                log::error("AX_ISP_IQ_GetAwbParam failed: %d", ax_res);
                return new_gains;
            }

            param.nEnable = 0;  // 0,manual; 1,auto
            param.tManualParam.tGain.nGainR = get_gain_float2u16(gains[0]);
            param.tManualParam.tGain.nGainGr = get_gain_float2u16(gains[1]);
            param.tManualParam.tGain.nGainGb = get_gain_float2u16(gains[2]);
            param.tManualParam.tGain.nGainB = get_gain_float2u16(gains[3]);
            ax_res = AX_ISP_IQ_SetAwbParam(_ch, &param);
            if (ax_res != 0) {
                log::error("AX_ISP_IQ_SetAwbParam failed: %d", ax_res);
                return new_gains;
            }
        }

        // ax_res = AX_ISP_IQ_GetAwbParam(_ch, &param);
        // if (ax_res != 0) {
        //     log::error("AX_ISP_IQ_GetAwbParam failed: %d", ax_res);
        //     return new_gains;
        // }
        // log::info("get awb gains %d %d %d %d", param.tManualParam.tGain.nGainR, param.tManualParam.tGain.nGainGr, param.tManualParam.tGain.nGainGb, param.tManualParam.tGain.nGainB);


        AX_ISP_IQ_AWB_STATUS_T status;
        ax_res = AX_ISP_IQ_GetAwbStatus(_ch, &status);
        if (ax_res != 0) {
            log::error("AX_ISP_IQ_GetAwbStatus failed: %d", ax_res);
            return new_gains;
        }
        new_gains.push_back(get_gain_u162float(status.tGainStatus.nGainR));
        new_gains.push_back(get_gain_u162float(status.tGainStatus.nGainGr));
        new_gains.push_back(get_gain_u162float(status.tGainStatus.nGainGb));
        new_gains.push_back(get_gain_u162float(status.tGainStatus.nGainB));
        // log::info("awb gains:[%d, %d, %d, %d] colot temperature: %d", status.tGainStatus.nGainR, status.tGainStatus.nGainGr, status.tGainStatus.nGainGb, status.tGainStatus.nGainB, status.tAlgoStatus.nCct);

        return new_gains;
    }

    int Camera::exp_mode(int value) {
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        AX_S32 ax_res;
        AX_ISP_IQ_AE_PARAM_T param;
        int current_mode = 0;

        if (value >= 0) {
            ax_res = AX_ISP_IQ_GetAeParam(_ch, &param);
            if (ax_res != 0) {
                log::error("AX_ISP_IQ_GetAeParam failed: %d", ax_res);
                return current_mode;
            }

            current_mode = value ? 0 : 1; // 0,auto; 1,manual
            param.nEnable = current_mode;
            ax_res = AX_ISP_IQ_SetAeParam(_ch, &param);
            if (ax_res != 0) {
                log::error("AX_ISP_IQ_GetAeParam failed: %d", ax_res);
                return current_mode;
            }
        }

        ax_res = AX_ISP_IQ_GetAeParam(_ch, &param);
        if (ax_res != 0) {
            log::error("AX_ISP_IQ_GetAeParam failed: %d", ax_res);
            return current_mode;
        }

        current_mode = param.nEnable ? 0 : 1;
        return current_mode;
    }

    err::Err Camera::set_windowing(std::vector<int> roi) {
        auto &mod_param = AxModuleParam::getInstance();
        mod_param.lock(AX_MOD_VI);
        auto vi_param = (ax_vi_mod_t *)mod_param.get_param(AX_MOD_VI);
        auto ax_cam = vi_param->cams[0];
        mod_param.unlock(AX_MOD_VI);

        err::Err ret = err::ERR_NONE;
        auto *priv = (camera_priv_t *)_param;
        auto ax_vi = priv->ax_vi;

        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }
        if (!this->is_opened()) {
            return err::ERR_NOT_OPEN;
        }

        int max_width = ax_cam.tSnsAttr.nWidth;
        int max_height = ax_cam.tSnsAttr.nHeight;
        char log_msg[100];
        int x = 0, y = 0, w = 0, h = 0;

        if (roi.size() == 4) {
            x = roi[0], y = roi[1], w = roi[2], h = roi[3];
        } else if (roi.size() == 2) {
            w = roi[0], h = roi[1];
            x = (max_width - w) / 2;
            y = (max_height - h) / 2;
        } else {
            err::check_raise(err::ERR_RUNTIME, "roi size must be 4 or 2");
        }

        snprintf(log_msg, sizeof(log_msg), "Width must be a multiple of 2.");
        err::check_bool_raise(w % 2 == 0, std::string(log_msg));
        snprintf(log_msg, sizeof(log_msg), "the coordinate x range needs to be [0,%d].", max_width - 1);
        err::check_bool_raise(x >= 0 || x < max_width, std::string(log_msg));
        snprintf(log_msg, sizeof(log_msg), "the coordinate y range needs to be [0,%d].", max_height - 1);
        err::check_bool_raise(y >= 0 || y < max_height, std::string(log_msg));
        snprintf(log_msg, sizeof(log_msg), "the row of the window is larger than the maximum, try x=%d, w=%d.", x, max_width - x);
        err::check_bool_raise(x + w <= max_width, std::string(log_msg));
        snprintf(log_msg, sizeof(log_msg), "the column of the window is larger than the maximum, try y=%d, h=%d.", y, max_height - y);
        err::check_bool_raise(y + h <= max_height, std::string(log_msg));

        bool is_vflip = priv->chn.vflip, is_hmirror = priv->chn.mirror;
        if (!is_vflip) {
            y = max_height - y - h;
        }
        if (!is_hmirror) {
            x = max_width - x - w;
        }
        err::check_bool_raise(!ax_vi->set_windowing(priv->chn.id,  x, y, w, h), "set windowing failed.");
        return ret;
    }

    std::vector<int> Camera::get_sensor_size() {
        auto &mod_param = AxModuleParam::getInstance();
        mod_param.lock(AX_MOD_VI);
        auto vi_param = (ax_vi_mod_t *)mod_param.get_param(AX_MOD_VI);
        auto ax_cam = vi_param->cams[0];
        mod_param.unlock(AX_MOD_VI);

        int max_width = ax_cam.tSnsAttr.nWidth;
        int max_height = ax_cam.tSnsAttr.nHeight;
        return {max_width, max_height};
    }

    err::Err Camera::write_reg(int addr, int data, int bit_width)
    {
        camera_priv_t *priv = (camera_priv_t *)_param;
        (void)bit_width;
        peripheral::i2c::I2C i2c_obj(0, peripheral::i2c::Mode::MASTER);
        uint8_t temp[1];
        temp[0] = (uint8_t)data;
        i2c_obj.writeto_mem(priv->i2c_addr, addr, temp, sizeof(temp), 16);
        return err::ERR_NONE;
    }

    int Camera::read_reg(int addr, int bit_width)
    {
        camera_priv_t *priv = (camera_priv_t *)_param;
        (void)bit_width;
        peripheral::i2c::I2C i2c_obj(0, peripheral::i2c::Mode::MASTER);
        Bytes *data = i2c_obj.readfrom_mem(priv->i2c_addr, addr, 1, 16);
        int out = -1;log::info("addr:%#x", priv->i2c_addr);
        if (data) {
            if (data->size() > 0) {
                out = data->data[0];
            }
            delete data;
        }

        return out;
    }

    void *Camera::get_driver()
    {
        auto *priv = (camera_priv_t *)_param;
        auto ax_vi = priv->ax_vi;
        return ax_vi;
    }
}

