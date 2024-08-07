/**
 * @author lxowalle@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2023.9.8: Add framework, create this file.
 */


#pragma once

#include <stdint.h>
#include "maix_err.hpp"
#include "maix_basic.hpp"
#include "maix_log.hpp"
#include "maix_image.hpp"
#include "maix_time.hpp"
#include "maix_i2c.hpp"
#include "maix_camera_base.hpp"
#include "sophgo_middleware.hpp"
#include <signal.h>

static void try_deinit_mmf()
{
    static uint8_t is_called = 0;
    if (!is_called) {
        mmf_try_deinit(true);
        is_called = 1;
    }
}

static void signal_handle(int signal)
{
    const char *signal_msg = NULL;
    switch (signal) {
    case SIGILL: signal_msg = "SIGILL"; break;
    case SIGTRAP: signal_msg = "SIGTRAP"; break;
    case SIGABRT: signal_msg = "SIGABRT"; break;
    case SIGBUS: signal_msg = "SIGBUS"; break;
    case SIGFPE: signal_msg = "SIGFPE"; break;
    case SIGKILL: signal_msg = "SIGKILL"; break;
    case SIGSEGV: signal_msg = "SIGSEGV"; break;
    default: signal_msg = "UNKNOWN"; break;
    }

    maix::log::error("Trigger signal, code:%s(%d)!\r\n", signal_msg, signal);
    try_deinit_mmf();
    exit(1);
}

// FIXME: move this function to port/maix_vision_maixcam.cpp ?
static __attribute__((constructor)) void maix_vision_register_signal(void)
{
    signal(SIGILL, signal_handle);
    signal(SIGTRAP, signal_handle);
    signal(SIGABRT, signal_handle);
    signal(SIGBUS, signal_handle);
    signal(SIGFPE, signal_handle);
    signal(SIGKILL, signal_handle);
    signal(SIGSEGV, signal_handle);

    maix::util::register_exit_function(try_deinit_mmf);
}

#define MAIX_SENSOR_FPS                  "MAIX_SENSOR_FPS"
namespace maix::camera
{
    static void config_sensor(void)
    {
        #define MMF_SENSOR_NAME "MMF_SENSOR_NAME"
        peripheral::i2c::I2C i2c_obj(4, peripheral::i2c::Mode::MASTER);
        std::vector<int> addr_list = i2c_obj.scan();
        for (size_t i = 0; i < addr_list.size(); i++) {
            printf("i2c addr: 0x%02x\n", addr_list[i]);
            switch (addr_list[i]) {
                case 0x30:
                    printf("found sms_sc035gs, addr 0x30\n" );
                    setenv(MMF_SENSOR_NAME, "sms_sc035gs", 0);
                    return;
                case 0x29: // fall through
                default:
                    printf("found gcore_gc4653, addr 0x29\n" );
                    setenv(MMF_SENSOR_NAME, "gcore_gc4653", 0);
                    return;
            }
        }

        printf("Found not sensor address, use gcore_gc4653\n" );
        setenv(MMF_SENSOR_NAME, "gcore_gc4653", 0);
        return;
    }

    class CameraCviMmf final : public CameraBase
    {
    public:
        CameraCviMmf(const std::string device, int width, int height, image::Format format, int buff_num, int fps)
        {
            this->device = device;
            this->format = format;
            this->width = width;
            this->height = height;
            this->buffer_num = buff_num;
            this->ch = -1;

            config_sensor();

            mmf_sys_cfg_t sys_cfg = {0};
            if (width <= 1280 && height <= 720 && fps > 30) {
                sys_cfg.vb_pool[0].size = 1280 * 720 * 3 / 2;
                sys_cfg.vb_pool[0].count = 3;
                sys_cfg.vb_pool[0].map = 2;
                sys_cfg.max_pool_cnt = 1;

                if (fps > 30 && fps <= 60) {
                    err::check_bool_raise(!setenv(MAIX_SENSOR_FPS, "60", 0), "setenv MAIX_SENSOR_FPS failed");
                } else {
                    err::check_bool_raise(!setenv(MAIX_SENSOR_FPS, "80", 0), "setenv MAIX_SENSOR_FPS failed");
                }
            } else {
                sys_cfg.vb_pool[0].size = 2560 * 1440 * 3 / 2;
                sys_cfg.vb_pool[0].count = 2;
                sys_cfg.vb_pool[0].map = 3;
                sys_cfg.max_pool_cnt = 1;
                err::check_bool_raise(!setenv(MAIX_SENSOR_FPS, "30", 0), "setenv MAIX_SENSOR_FPS failed");
            }
            mmf_pre_config_sys(&sys_cfg);

            if (0 != mmf_init()) {
                err::check_raise(err::ERR_RUNTIME, "mmf init failed");
            }

            mmf_vi_cfg_t cfg = {0};
            cfg.w = width;
            cfg.h = height;
            cfg.fmt = mmf_invert_format_to_mmf(format);
            cfg.depth = buff_num;
            cfg.fps = fps;
            if (0 != mmf_vi_init2(&cfg)) {
                mmf_deinit();
                err::check_raise(err::ERR_RUNTIME, "mmf vi init failed");
            }
        }

        CameraCviMmf(const std::string device, int ch, int width, int height, image::Format format, int buff_num)
        {
            this->device = device;
            this->format = format;
            this->width = width;
            this->height = height;
            this->buffer_num = buff_num;
            this->ch = ch;

            if (0 != mmf_init()) {
                err::check_raise(err::ERR_RUNTIME, "mmf init failed");
            }

            if (0 != mmf_vi_init()) {
                mmf_deinit();
                err::check_raise(err::ERR_RUNTIME, "mmf vi init failed");
            }
        }

        ~CameraCviMmf()
        {
            mmf_del_vi_channel(this->ch);
            mmf_deinit();
        }

        err::Err open(int width, int height, image::Format format, int buff_num)
        {
            if (format == image::FMT_GRAYSCALE) {
                format = image::FMT_YVU420SP;
            }
            int ch = mmf_get_vi_unused_channel();
            if (ch < 0) {
                log::error("camera open: mmf get vi channel failed");
                return err::ERR_RUNTIME;
            }
            if (0 != mmf_add_vi_channel(ch, width, height, mmf_invert_format_to_mmf(format))) {
                log::error("camera open: mmf add vi channel failed");
                return err::ERR_RUNTIME;
            }

            this->ch = ch;
            this->width = (width == -1) ? this->width : width;
            this->height = (height == -1) ? this->height : height;
            this->align_width = mmf_vi_aligned_width(this->ch);
            this->align_need = (this->width % this->align_width == 0) ? false : true;   // Width need align only
            return err::ERR_NONE;
        } // open

        bool is_support_format(image::Format format)
        {
            if(format == image::Format::FMT_RGB888 || format == image::Format::FMT_BGR888
            || format == image::Format::FMT_YVU420SP|| format == image::Format::FMT_GRAYSCALE)
                return true;
            return false;
        }

        void close()
        {
            if (mmf_vi_chn_is_open(this->ch) == true) {
                if (0 != mmf_del_vi_channel(this->ch)) {
                    log::error("mmf del vi channel failed");
                }
            }
        } // close

        // read
        image::Image *read(void *buff = NULL, size_t buff_size = 0)
        {
            image::Image *img = NULL;

            void *buffer = NULL;
            int buffer_len = 0, width = 0, height = 0, format = 0;

            if (0 == mmf_vi_frame_pop(this->ch, &buffer, &buffer_len, &width, &height, &format)) {
                if (buffer == NULL) {
                    mmf_vi_frame_free(this->ch);
                    return NULL;
                }
                if(buff)
                {
                    if(buff_size < (size_t)buffer_len)
                    {
                        log::error("camera read: buff size not enough, need %d, but %d", buffer_len, buff_size);
                        mmf_vi_frame_free(this->ch);
                        return NULL;
                    }
                    img = new image::Image(width, height, this->format, (uint8_t*)buff, buff_size, false);
                }
                else
                {
                    img = new image::Image(this->width, this->height, this->format);
                }
                void *image_data = img->data();
                switch (img->format()) {
                    case image::Format::FMT_GRAYSCALE:
                        if (this->align_need) {
                            for (int h = 0; h < this->height; h ++) {
                                memcpy((uint8_t *)image_data + h * this->width, (uint8_t *)buffer + h * width, this->width);
                            }
                        } else {
                            memcpy(image_data, buffer, this->width * this->height);
                        }
                        break;
                    case image::Format::FMT_BGR888: // fall through
                    case image::Format::FMT_RGB888:
                        if (this->align_need) {
                            for (int h = 0; h < this->height; h++) {
                                memcpy((uint8_t *)image_data + h * this->width * 3, (uint8_t *)buffer + h * width * 3, this->width * 3);
                            }
                        } else {
                            memcpy(image_data, buffer, this->width * this->height * 3);
                        }
                        break;
                    case image::Format::FMT_YVU420SP:
                        if (this->align_need) {
                            for (int h = 0; h < this->height * 3 / 2; h ++) {
                                memcpy((uint8_t *)image_data + h * this->width, (uint8_t *)buffer + h * width, this->width);
                            }
                        } else {
                            memcpy(image_data, buffer, this->width * this->height * 3 / 2);
                        }
                        break;
                    default:
                        printf("unknown format\n");
                        delete img;
                        mmf_vi_frame_free(this->ch);
                        return NULL;
                }
                mmf_vi_frame_free(this->ch);
                return img;
            }

            return img;
        } // read

        camera::CameraCviMmf *add_channel(int width, int height, image::Format format, int buff_num)
        {
            int new_channel = mmf_get_vi_unused_channel();
            if (new_channel < 0) {
                log::error("Support not more channel!\r\n");
                return NULL;
            }
            return new camera::CameraCviMmf(this->device, new_channel, width, height, format, buff_num);
        }

        void clear_buff()
        {

        }

        bool is_opened() {
            return mmf_vi_chn_is_open(this->ch);
        }

        int get_ch_nums() {
            return 1;
        }

        int get_channel() {
            return this->ch;
        }

        int hmirror(int en)
        {
            bool out;
            if (en == -1) {
                mmf_get_vi_hmirror(this->ch, &out);
            } else {
                bool need_open = false;
                if (this->is_opened()) {
                    this->close();
                    need_open = true;
                }

                mmf_set_vi_hmirror(this->ch, en);

                if (need_open) {
                    err::check_raise(this->open(this->width, this->height, this->format, this->buffer_num), "Open failed");
                }
                out = en;
            }

            return out;
        }

        int vflip(int en)
        {
            bool out;
            if (en == -1) {
                mmf_get_vi_vflip(this->ch, &out);
            } else {
                bool need_open = false;
                if (this->is_opened()) {
                    this->close();
                    need_open = true;
                }

                mmf_set_vi_vflip(this->ch, en);

                if (need_open) {
                    err::check_raise(this->open(this->width, this->height, this->format, this->buffer_num), "Open failed");
                }
                out = en;
            }
            return out;
        }

        int luma(int value)
        {
            uint32_t out;
            if (value == -1) {
                mmf_get_luma(this->ch, &out);
            } else {
                mmf_set_luma(this->ch, value);
                out = value;
            }
            return out;
        }

        int constrast(int value)
        {
            uint32_t out;
            if (value == -1) {
                mmf_get_constrast(this->ch, &out);
            } else {
                mmf_set_constrast(this->ch, value);
                out = value;
            }
            return out;
        }

        int saturation(int value)
        {
            uint32_t out;
            if (value == -1) {
                mmf_get_saturation(this->ch, &out);
            } else {
                mmf_set_saturation(this->ch, value);
                out = value;
            }
            return out;
        }

        int exposure(int value)
        {
            uint32_t out;
            if (value == -1) {
                mmf_get_exptime(this->ch, &out);
            } else {
                mmf_set_exptime(this->ch, value);
                out = value;
            }
            return out;
        }

        int gain(int value)
        {
            uint32_t out;
            if (value == -1) {
                mmf_get_again(this->ch, &out);
            } else {
                mmf_set_again(this->ch, value);
                out = value;
            }
            return out;
        }

        int awb_mode(int value)
        {
            uint32_t out;
            if (value == -1) {
                out = mmf_get_wb_mode(this->ch);
            } else {
                mmf_set_wb_mode(this->ch, value);
                out = value;
            }

            err::check_bool_raise(out >= 0, "set white balance failed");
            return out;
        }

        int exp_mode(int value)
        {
            uint32_t out;
            if (value == -1) {
                out = mmf_get_exp_mode(this->ch);
            } else {
                mmf_set_exp_mode(this->ch, value);
                out = value;
            }

            err::check_bool_raise(out >= 0, "set exposure failed");
            return out;
        }

        int set_windowing(std::vector<int> roi)
        {
            int max_height, max_width;
            char log_msg[100];
            int x = 0, y = 0, w = 0, h = 0;

            err::check_bool_raise(!mmf_vi_get_max_size(&max_width, &max_height), "get max size of camera failed");

            if (roi.size() == 4) {
                x = roi[0], y = roi[1], w = roi[2], h = roi[3];
            } else if (roi.size() == 2) {
                w = roi[0], h = roi[1];
                x = (max_width - w) / 2;
                y = (max_height - h) / 2;
            } else {
                err::check_raise(err::ERR_RUNTIME, "roi size must be 4 or 2");
            }

            snprintf(log_msg, sizeof(log_msg), "Width must be a multiple of 64.");
            err::check_bool_raise(w % 64 == 0, std::string(log_msg));
            snprintf(log_msg, sizeof(log_msg), "the coordinate x range needs to be [0,%d].", max_width - 1);
            err::check_bool_raise(x >= 0 || x < max_width, std::string(log_msg));
            snprintf(log_msg, sizeof(log_msg), "the coordinate y range needs to be [0,%d].", max_height - 1);
            err::check_bool_raise(y >= 0 || y < max_height, std::string(log_msg));
            snprintf(log_msg, sizeof(log_msg), "the row of the window is larger than the maximum, try x=%d, w=%d.", x, max_width - x);
            err::check_bool_raise(x + w <= max_width, std::string(log_msg));
            snprintf(log_msg, sizeof(log_msg), "the column of the window is larger than the maximum, try y=%d, h=%d.", y, max_height - y);
            err::check_bool_raise(y + h <= max_height, std::string(log_msg));

            // this->width = w;
            // this->height = h;
            err::check_bool_raise(!mmf_vi_channel_set_windowing(ch,  x, y, w, h), "set windowing failed.");

            return 0;
        }
    private:
        std::string device;
        image::Format format;
        int fd;
        int ch;
        uint32_t raw_format;
        std::vector<void*> buffers;
        std::vector<int> buffers_len;
        int buffer_num;
        int queue_id; // user directly used buffer id
        int width;
        int height;
        void *buff;
        bool buff_alloc;
        int align_width;
        bool align_need;
    };

} // namespace maix::camera
