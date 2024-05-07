/**
 * @author lxowalle@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2023.9.8: Add framework, create this file.
 */

#include <stdint.h>
#include "maix_err.hpp"
#include "maix_log.hpp"
#include "maix_image.hpp"
#include "maix_time.hpp"
#include "maix_video.hpp"

namespace maix::video
{
    Video::Video(std::string path, int width, int height, image::Format format, int time_base, int framerate, bool open)
    {
        throw err::Exception(err::ERR_NOT_IMPL);
    }

    Video::~Video() {
        if (this->_is_opened) {
            this->close();
        }
    }

    err::Err Video::open(std::string path, double fps)
    {
        throw err::Exception(err::ERR_NOT_IMPL);
        return err::ERR_NONE;
    }

    void Video::close()
    {
        throw err::Exception(err::ERR_NOT_IMPL);
    }

    err::Err Video::bind_camera(camera::Camera *camera) {
        err::Err err = err::ERR_NONE;
        throw err::Exception(err::ERR_NOT_IMPL);
        return err;
    }

    video::Packet Video::encode(image::Image *img) {
        video::Packet packet(NULL, 0);
        return packet;
    }

    image::Image *Video::decode(video::Frame *frame) {
        return NULL;
    }

    err::Err Video::finish() {
        throw err::Exception(err::ERR_NOT_IMPL);
        return err::ERR_NOT_IMPL;
    }
} // namespace maix::video
