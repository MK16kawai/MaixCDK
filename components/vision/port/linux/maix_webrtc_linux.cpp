/**
 * @author 916BGAI@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2025.12.08: Add maixcam2 webrtc framework.
 */

#include "maix_webrtc.hpp"
#include "maix_err.hpp"
#include <dirent.h>
#include <pthread.h>

namespace maix::webrtc
{
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

    WebRTC::WebRTC(std::string ip, int port, int fps,
                   webrtc::WebRTCStreamType stream_type, int bitrate, int gop,
                   std::string signaling_ip, int signaling_port,
                   const std::string &stun_server,
                   bool http_server)
    {
        (void)ip;
        (void)port;
        (void)fps;
        (void)gop;
        (void)stream_type;
        (void)bitrate;
        (void)signaling_ip;
        (void)signaling_port;
        (void)stun_server;
        (void)http_server;
    }

    WebRTC::~WebRTC()
    {

    }

    err::Err WebRTC::start()
    {
        err::Err err = err::ERR_NOT_IMPL;
        return err;
    }

    err::Err WebRTC::stop()
    {
        err::Err err = err::ERR_NOT_IMPL;
        return err;
    }

    err::Err WebRTC::bind_camera(camera::Camera *camera)
    {
        err::Err err = err::ERR_NOT_IMPL;
        (void)camera;
        return err;
    }

    err::Err WebRTC::bind_audio_recorder(audio::Recorder *recorder)
    {
        err::Err err = err::ERR_NOT_IMPL;
        (void)recorder;
        return err;
    }

    err::Err WebRTC::write(video::Frame &frame)
    {
        err::Err err = err::ERR_NOT_IMPL;
        (void)frame;
        return err;
    }

    camera::Camera *WebRTC::to_camera()
    {
        err::check_null_raise(NULL, "The camera object is NULL");
        return NULL;
    }

    std::string WebRTC::get_url()
    {
        return "not supported";
    }

    std::vector<std::string> WebRTC::get_urls()
    {
        return std::vector<std::string>();
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
