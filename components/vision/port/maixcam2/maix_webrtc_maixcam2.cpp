/**
 * @author 916BGAI@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2025.12.08: Add maixcam2 webrtc framework.
 */

#include "maix_webrtc.hpp"
#include "maix_err.hpp"
#include "maix_basic.hpp"
#include "maix_camera.hpp"
#include "maix_video.hpp"
#include "maix_audio.hpp"
#include "ax_middleware.hpp"
#include "maix_webrtc_server.hpp"
#include "nlohmann/json.hpp"
#include <vector>
#include <string>
#include <atomic>
#include <ifaddrs.h>
#include <netdb.h>

namespace maix::webrtc
{
    using namespace maix::middleware::maixcam2;
    Region::Region(int x, int y, int width, int height, image::Format format, camera::Camera *camera)
    {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)format;
        (void)camera;
    }

    Region::~Region() {

    }

    image::Image *Region::get_canvas() {
        return NULL;
    }

    err::Err Region::update_canvas() {
        return err::ERR_NOT_IMPL;
    }

    enum WebRTCStatus
    {
        WEBRTC_IDLE = 0,
        WEBRTC_RUNNING,
        WEBRTC_STOP,
    };

    typedef struct
    {
        WebRTC *self;

        enum WebRTCStatus status;

        camera::Camera *camera;
        video::Encoder *encoder;
        audio::Recorder *audio_recorder;

        bool bind_camera;
        bool bind_audio_recorder;

        int encoder_bitrate;
        int gop;

        MaixWebRTCServer *webrtc_server;

    } webrtc_param_t;

    static uint64_t get_video_timestamp() {
        return time::ticks_ms() * 90;
    }

    static void _camera_push_thread(void *args)
    {
        webrtc_param_t *param = (webrtc_param_t *)args;

        int vi_ch = param->camera->get_channel();
        size_t video_pts = 0;
        bool found_first_pps_sps = false;

        auto *vi = (VI *)param->camera->get_driver();
        auto *venc = (VENC *)param->encoder->get_driver();

        while (param->status == WEBRTC_RUNNING && !app::need_exit()) {
            bool found_camera_frame = false;
            bool found_venc_stream = false;
            AX_VENC_STREAM_T venc_stream = {0};

            if (param->webrtc_server == nullptr || !param->webrtc_server->is_video_track_open()) {
                time::sleep_ms(10);
                continue;
            }

            auto camera_frame = vi->pop(vi_ch);
            if (camera_frame) {
                found_camera_frame = true;
            }

            Frame *venc_frame = venc->pop(100);
            if (venc_frame) {
                found_venc_stream = true;
                venc_frame->get_venc_stream(&venc_stream);
            } else {
                if (found_first_pps_sps) {
                    log::error("get venc data timeout");
                }
            }

            if (found_venc_stream) {
                if (!found_first_pps_sps) {
                    if (venc_stream.stPack.u32NaluNum > 1) {
                        found_first_pps_sps = true;
                        video_pts = 0;
                    }
                }

                if (found_first_pps_sps) {
                    int venc_output_data_size = venc_stream.stPack.u32Len;
                    uint8_t *venc_output_data = nullptr;
                    venc_output_data = (uint8_t *)malloc(venc_output_data_size);
                    if (venc_output_data) {
                        memcpy(venc_output_data, venc_stream.stPack.pu8Addr, venc_output_data_size);
                        video_pts = get_video_timestamp();

                        if (param->webrtc_server && param->webrtc_server->is_video_track_open()) {
                            param->webrtc_server->video_frame_push(venc_output_data, venc_output_data_size, video_pts);
                        }

                        free(venc_output_data);
                    }
                }
            }

            if (found_venc_stream) {
                delete venc_frame;
            }

            if (found_camera_frame) {
                venc->push(camera_frame, 1000);
                delete camera_frame;
            }
        }

        param->status = WEBRTC_IDLE;
    }

    static video::VideoType get_video_type( maix::webrtc::WebRTCStreamType stream, maix::webrtc::WebRTCRCType rc)
    {
        using stream_type = maix::webrtc::WebRTCStreamType;
        using rc_type = maix::webrtc::WebRTCRCType;

        if (stream == stream_type::WEBRTC_STREAM_H264) {
            if (rc == rc_type::WEBRTC_RC_CBR) { return video::VIDEO_H264_CBR; }
            if (rc == rc_type::WEBRTC_RC_VBR) { return video::VIDEO_H264_VBR; }
        }

        if (stream == stream_type::WEBRTC_STREAM_H265) {
            if (rc == rc_type::WEBRTC_RC_CBR) { return video::VIDEO_H265_CBR; }
            if (rc == rc_type::WEBRTC_RC_VBR) { return video::VIDEO_H265_VBR; }
        }

        log::error("Unsupported video type stream=%d rc=%d", (int)stream, (int)rc);
        return video::VIDEO_H264_CBR;
    }

    WebRTC::WebRTC(std::string ip, int port, webrtc::WebRTCStreamType stream_type,
                   webrtc::WebRTCRCType rc_type, int bitrate, int gop, std::string signaling_ip,
                   int signaling_port, const std::string &stun_server, bool http_server)
    {
        this->_ip = ip.size() ? ip : "0.0.0.0";
        this->_port = port;
        this->_signaling_ip = signaling_ip;
        this->_signaling_port = signaling_port;
        this->_stun_server = stun_server;
        this->_gop = gop;
        this->_stream_type = stream_type;
        this->_rc_type = rc_type;
        this->_is_start = false;
        this->_video_thread = nullptr;
        this->_http_server = http_server;
        this->_timestamp = 0;
        this->_last_ms = 0;
        this->_region_max_number = 16;
        for (int i = 0; i < this->_region_max_number; i ++) {
            this->_region_list.push_back(NULL);
            this->_region_type_list.push_back(0);
            this->_region_used_list.push_back(false);
        }

        auto *param = new webrtc_param_t();
        param->self = this;
        param->status = WEBRTC_IDLE;
        param->camera = nullptr;
        param->encoder = nullptr;
        param->audio_recorder = nullptr;
        param->bind_camera = false;
        param->bind_audio_recorder = false;
        param->encoder_bitrate = bitrate;
        param->gop = gop;

        param->webrtc_server = nullptr;

        _param = (void *)param;
    }

    WebRTC::~WebRTC()
    {
        webrtc_param_t *param = (webrtc_param_t *)_param;
        if (param) {
            if (param->status == WEBRTC_RUNNING) {
                stop();
            }

            if (param->encoder) {
                delete param->encoder;
                param->encoder = nullptr;
            }

            if (param->webrtc_server) {
                delete param->webrtc_server;
                param->webrtc_server = nullptr;
            }

            delete param;
            _param = nullptr;
        }

        for (auto &region : this->_region_list) {
            delete region;
        }
    }

    err::Err WebRTC::start()
    {
        webrtc_param_t *param = (webrtc_param_t *)_param;
        if (!param) {
            return err::ERR_RUNTIME;
        }

        if (param->status != WEBRTC_IDLE) {
            return err::ERR_BUSY;
        }

        if (!param->bind_camera || !param->camera) {
            log::error("You need bind a camera!");
            return err::ERR_RUNTIME;
        }

        if (param->camera->width() % 32 != 0) {
            log::error("camera width must be multiple of 32!\r\n");
            return err::ERR_RUNTIME;
        }

        if (param->encoder) {
            delete param->encoder;
            param->encoder = nullptr;
        }

        video::VideoType video_type = get_video_type(this->_stream_type, this->_rc_type);

        param->encoder = new video::Encoder("", param->camera->width(), param->camera->height(), image::Format::FMT_YVU420SP, video_type, param->camera->fps(), param->gop, param->encoder_bitrate);
        err::check_null_raise(param->encoder, "Create video encoder failed!");

        if (param->bind_audio_recorder) {
            param->audio_recorder->reset(true);
        }

        if (param->webrtc_server) {
            delete param->webrtc_server;
            param->webrtc_server = nullptr;
        }

        MaixWebRTCServerBuilder builder;
        builder.set_ice_server(this->_stun_server);

        if (this->_stream_type == maix::webrtc::WebRTCStreamType::WEBRTC_STREAM_H265) {
            builder.set_video_codec(VideoCodec::H265);
            log::info("Using H.265 codec for WebRTC");
        } else {
            builder.set_video_codec(VideoCodec::H264);
            log::info("Using H.264 codec for WebRTC");
        }

        if (this->_http_server) {
            builder.set_http_server(true, this->_ip, this->_port);
            builder.set_ws_port(this->_signaling_port);
            builder.set_stun_server(this->_stun_server);
            log::info("HTTP server will be started on %s:%d", this->_ip.c_str(), this->_port);
        }

        param->webrtc_server = builder.build();
        if (!param->webrtc_server) {
            log::error("Failed to create WebRTC server");
            return err::ERR_RUNTIME;
        }

        if (!param->webrtc_server->connect_signaling(this->_signaling_ip, this->_signaling_port)) {
            log::error("Failed to connect to signaling server");
            return err::ERR_RUNTIME;
        }

        param->webrtc_server->start();

        param->status = WEBRTC_RUNNING;
        _video_thread = new thread::Thread(_camera_push_thread, param);
        if (!_video_thread) {
            param->status = WEBRTC_IDLE;
            return err::ERR_RUNTIME;
        }

        _is_start = true;
        return err::ERR_NONE;
    }

    err::Err WebRTC::stop()
    {
        webrtc_param_t *param = (webrtc_param_t *)_param;
        if (!param) return err::ERR_NONE;

        if (param->status != WEBRTC_RUNNING) {
            return err::ERR_NONE;
        }

        param->status = WEBRTC_STOP;

        if (_video_thread) {
            _video_thread->join();
            delete _video_thread;
            _video_thread = nullptr;
        }

        param->status = WEBRTC_IDLE;
        _is_start = false;
        return err::ERR_NONE;
    }

    err::Err WebRTC::bind_camera(camera::Camera *camera)
    {
        webrtc_param_t *param = (webrtc_param_t *)_param;
        if (!param || !camera) {
            return err::ERR_RUNTIME;
        }

        if (camera->format() != image::Format::FMT_YVU420SP) {
            err::check_raise(err::ERR_RUNTIME, "bind camera failed! support FMT_YVU420SP only!\r\n");
            return err::ERR_RUNTIME;
        }

        param->camera = camera;
        param->bind_camera = true;
        return err::ERR_NONE;
    }

    err::Err WebRTC::bind_audio_recorder(audio::Recorder *recorder)
    {
        err::Err err = err::ERR_NOT_IMPL;
        (void)recorder;
        return err;
    }

    err::Err WebRTC::write(video::Frame &frame)
    {
        err::Err err = err::ERR_NONE;
        err::check_raise(err::ERR_NOT_IMPL, "write frame not impl!");
        return err;
    }

    camera::Camera *WebRTC::to_camera()
    {
        webrtc_param_t *param = (webrtc_param_t *)_param;
        if (!param || !param->camera) {
            return nullptr;
        }
        return param->camera;
    }

    std::string WebRTC::get_url()
    {
        return "http://" + _ip + ":" + std::to_string(_port);
    }

    static int get_ip(char *hw, char ip[16])
    {
        struct ifaddrs *ifaddr, *ifa;
        int family, s;
        char host[NI_MAXHOST];

        if (getifaddrs(&ifaddr) == -1) {
            perror("getifaddrs");
            return -1;
        }

        for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == NULL) {
                continue;
            }

            family = ifa->ifa_addr->sa_family;

            if (family == AF_INET) {
                s = getnameinfo(ifa->ifa_addr, (family == AF_INET) ? sizeof(struct sockaddr_in) :
                                sizeof(struct sockaddr_in6), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
                if (s != 0) {
                    printf("getnameinfo() failed: %s\n", gai_strerror(s));
                    return -1;
                }

                if (!strcmp(ifa->ifa_name, hw)) {
                    strncpy(ip, host, 16);
                    freeifaddrs(ifaddr);
                    return 0;
                }
            }
        }

        freeifaddrs(ifaddr);
        return -1;
    }

    static std::vector<std::string> webrtc_get_server_urls(std::string ip, int port)
    {
        char new_ip[16] = {0};

        std::vector<std::string> ip_list;

        if (!strcmp("0.0.0.0", ip.c_str())) {
            if (!get_ip((char *)"eth0", new_ip)) {
                ip_list.push_back("http://" + std::string(new_ip) + ":" + std::to_string(port));
            }
            if (!get_ip((char *)"usb0", new_ip)) {
                ip_list.push_back("http://" + std::string(new_ip) + ":" + std::to_string(port));
            }
            if (!get_ip((char *)"usb1", new_ip)) {
                ip_list.push_back("http://" + std::string(new_ip) + ":" + std::to_string(port));
            }
            if (!get_ip((char *)"wlan0", new_ip)) {
                ip_list.push_back("http://" + std::string(new_ip) + ":" + std::to_string(port));
            }
            if (!get_ip((char *)"tailscale0", new_ip)) {
                ip_list.push_back("http://" + std::string(new_ip) + ":" + std::to_string(port));
            }
        } else {
            ip_list.push_back("http://" + ip + ":" + std::to_string(port));
        }

        return ip_list;
    }

    std::vector<std::string> WebRTC::get_urls()
    {
        return webrtc_get_server_urls(_ip, _port);
    }

    webrtc::Region *WebRTC::add_region(int x, int y, int width, int height, image::Format format)
    {
        (void)x;
        (void)y;
        (void)width;
        (void)height;
        (void)format;
        return NULL;
    }

    err::Err WebRTC::update_region(webrtc::Region &region)
    {
        return region.update_canvas();
    }

    err::Err WebRTC::del_region(webrtc::Region *region)
    {
        return err::ERR_NOT_IMPL;
    }

    err::Err WebRTC::draw_rect(int id, int x, int y, int width, int height, image::Color color, int thickness)
    {
        return err::ERR_NOT_IMPL;
    }

    err::Err WebRTC::draw_string(int id, int x, int y, const char *str, image::Color color, int size, int thickness)
    {
        return err::ERR_NOT_IMPL;
    }

} // namespace maix::webrtc
