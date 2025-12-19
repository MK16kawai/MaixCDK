/**
 * @author 916BGAI@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2025.12.08: Add framework, create this file.
 */

#ifndef __MAIX_WEBRTC_HPP
#define __MAIX_WEBRTC_HPP

#include "maix_err.hpp"
#include "maix_camera.hpp"
#include "maix_display.hpp"
#include "maix_video.hpp"
#include "maix_image.hpp"
#include "maix_basic.hpp"

namespace maix::webrtc
{
    /**
     * The stream type of webrtc
     * @maixpy maix.webrtc.WebRTCStreamType
     */
    enum class WebRTCStreamType
    {
        WEBRTC_STREAM_NONE = 0,  // format invalid
        WEBRTC_STREAM_H264,
        WEBRTC_STREAM_H265,
    };

    /**
     * The rc type of webrtc
     * @maixpy maix.webrtc.WebRTCRCType
     */
    enum class WebRTCRCType
    {
        WEBRTC_RC_NONE = 0,  // format invalid
        WEBRTC_RC_CBR,
        WEBRTC_RC_VBR,
    };

    /**
     * Region class
     * @maixpy maix.webrtc.Region
     */
    class Region
    {
    public:
        /**
         * @brief Construct a new Region object
         * @param x region coordinate x
         * @param y region coordinate y
         * @param width region width
         * @param height region height
         * @param format region format
         * @param camera bind region to camera
         * @maixpy maix.webrtc.Region.__init__
         * @maixcdk maix.webrtc.Region.Region
         */
        Region(int x, int y, int width, int height, image::Format format, camera::Camera *camera);
        ~Region();

        /**
         * @brief Return an image object from region
         * @return image object
         * @maixpy maix.webrtc.Region.get_canvas
        */
        image::Image *get_canvas();

        /**
         * @brief Update canvas
         * @return error code
         * * @maixpy maix.webrtc.Region.update_canvas
        */
        err::Err update_canvas();
    private:
        int _id;
        int _x;
        int _y;
        int _width;
        int _height;
        bool _flip;
        bool _mirror;
        image::Format _format;
        image::Image *_image;
        camera::Camera *_camera;
    };

    /**
     * WebRTC class
     * @maixpy maix.webrtc.WebRTC
     */
    class WebRTC
    {
    public:
        /**
         * @brief Construct a new WebRTC object
         * @param ip listen ip
         * @param port listen port
         * @param stream_type stream type
         * @param rc_type rc type
         * @param bitrate video bitrate
         * @param gop video gop
         * @param signaling_ip signaling server bind ip
         * @param signaling_port signaling server bind port
         * @param stun_server STUN server address
         * @param http_server whether to enable the HTTP server
         * @maixpy maix.webrtc.WebRTC.__init__
         * @maixcdk maix.webrtc.WebRTC.WebRTC
         */
        WebRTC(std::string ip = std::string(), int port = 8000,
               webrtc::WebRTCStreamType stream_type = webrtc::WebRTCStreamType::WEBRTC_STREAM_H264,
               webrtc::WebRTCRCType rc_type = webrtc::WebRTCRCType::WEBRTC_RC_CBR,
               int bitrate = 3000 * 1000, int gop = 60, std::string signaling_ip = std::string(),
               int signaling_port = 8001, const std::string &stun_server = "stun:stun.l.google.com:19302",
               bool http_server = true);
        ~WebRTC();

        /**
         * @brief start webrtc
         * @return error code
         * @maixpy maix.webrtc.WebRTC.start
        */
        err::Err start();

        /**
         * @brief stop webrtc
         * @return error code
         * @maixpy maix.webrtc.WebRTC.stop
        */
        err::Err stop();

        /**
         * @brief Bind camera
         * @param camera camera object
         * @return error code
         * @maixpy maix.webrtc.WebRTC.bind_camera
        */
        err::Err bind_camera(camera::Camera *camera);

        /**
         * @brief Bind audio recorder
         * @note If the audio_recorder object is bound, it cannot be used elsewhere.
         * @param recorder audio recorder object
         * @return error code
         * @maixpy maix.webrtc.WebRTC.bind_audio_recorder
        */
        err::Err bind_audio_recorder(audio::Recorder *recorder);

        /**
         * @brief Write encoded video (optional, reserved)
         * @maixpy maix.webrtc.WebRTC.write
        */
        err::Err write(video::Frame &frame);

        /**
         * @brief Get signaling or play url
         * @return url
         * @maixpy maix.webrtc.WebRTC.get_url
        */
        std::string get_url();

        /**
         * @brief Get url list
         * @return url list
         * @maixpy maix.webrtc.WebRTC.get_urls
        */
        std::vector<std::string> get_urls();

        /**
         * @brief Get camera object
         * @maixpy maix.webrtc.WebRTC.to_camera
        */
        camera::Camera *to_camera();

        /**
         * @brief webrtc is start
         * @maixcdk maix.webrtc.WebRTC.webrtc_is_start
        */
        bool webrtc_is_start()
        {
            return this->_is_start;
        }

        /**
         * @brief return a region object, you can draw image on the region.(This function will be removed in the future)
         * @param x region coordinate x
         * @param y region coordinate y
         * @param width region width
         * @param height region height
         * @param format region format, support Format::FMT_BGRA8888 only
         * @return the reigon object
         * @maixpy maix.webrtc.WebRTC.add_region
        */
        webrtc::Region *add_region(int x, int y, int width, int height, image::Format format = image::Format::FMT_BGRA8888);

        /**
         * @brief update and show region(This function will be removed in the future)
         * @return error code
         * @maixpy maix.webrtc.WebRTC.update_region
        */
        err::Err update_region(webrtc::Region &region);

        /**
         * @brief del region(This function will be removed in the future)
         * @return error code
         * @maixpy maix.webrtc.WebRTC.del_region
        */
        err::Err del_region(webrtc::Region *region);

        /**
         * @brief return region list(This function will be removed in the future)
         * @attention DO NOT ADD THIS FUNC TO MAIXPY
         * @return return a list of region
        */
        std::vector<webrtc::Region *> get_regions() {
            return this->_region_list;
        }

        /**
         * @brief Draw a rectangle on the canvas(This function will be removed in the future)
         * @param id region id
         * @param x rectangle coordinate x
         * @param y rectangle coordinate y
         * @param width rectangle width
         * @param height rectangle height
         * @param color rectangle color
         * @param thickness rectangle thickness. If you set it to -1, the rectangle will be filled.
         * @return error code
         * @maixpy maix.webrtc.WebRTC.draw_rect
        */
        err::Err draw_rect(int id, int x, int y, int width, int height, image::Color color, int thickness = 1);

        /**
         * @brief Draw a string on the canvas(This function will be removed in the future)
         * @param id region id
         * @param x string coordinate x
         * @param y string coordinate y
         * @param str string
         * @param color string color
         * @param size string size
         * @param thickness string thickness
         * @return error code
         * @maixpy maix.webrtc.WebRTC.draw_string
        */
        err::Err draw_string(int id, int x, int y, const char *str, image::Color color, int size = 16, int thickness = 1);

        /**
         * @brief timestamp api
         */
        uint64_t get_timestamp() {
            return _timestamp;
        }

        void update_timestamp() {
            uint64_t ms = time::ticks_ms();
            this->_timestamp += (ms - this->_last_ms);
            this->_last_ms = ms;
        }

    private:
        std::string _ip;
        int _port;
        std::string _signaling_ip;
        int _signaling_port;
        int _gop;
        webrtc::WebRTCStreamType _stream_type;
        webrtc::WebRTCRCType _rc_type;
        bool _is_start;
        thread::Thread *_video_thread;
        std::string _stun_server;
        bool _http_server;
        std::vector<webrtc::Region *> _region_list;
        std::vector<bool> _region_used_list;
        std::vector<int> _region_type_list;
        std::vector<bool> _region_update_flag;
        uint64_t _timestamp;
        uint64_t _last_ms;
        int _region_max_number;
        void *_param;
    };

} // namespace maix::webrtc

#endif // __MAIX_WEBRTC_HPP
