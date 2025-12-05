/**
 * LLM SmolVLM implementation on Linux
 * @license Apache-2.0
 * @author lxo@sipeed
 * @date 2025-11-24
 */

#include "maix_vlm_smolvlm.hpp"
#include "maix_nn.hpp"

namespace maix::nn
{
    SmolVLM::SmolVLM(const std::string &model)
    {
        (void)model;
    }

    SmolVLM::~SmolVLM()
    {

    }

    void SmolVLM::set_log_level(log::LogLevel level, bool color)
    {
        (void)level;
        (void)color;
    }

    err::Err SmolVLM::load(const std::string &model)
    {
        (void)model;
        return err::ERR_NOT_IMPL;
    }

    err::Err SmolVLM::unload()
    {
        return err::ERR_NOT_IMPL;
    }


    void SmolVLM::set_system_prompt(const std::string &prompt)
    {
        (void)prompt;
    }

    int SmolVLM::input_width()
    {
        return 0;
    }

    int SmolVLM::input_height()
    {
        return 0;
    }

    maix::image::Format SmolVLM::input_format()
    {
        return maix::image::Format::FMT_RGB888;
    }

    err::Err SmolVLM::set_image(maix::image::Image &img, maix::image::Fit fit)
    {
        (void)img;
        (void)fit;
        return err::ERR_NOT_IMPL;
    }

    void SmolVLM::clear_image()
    {
    }

    bool SmolVLM::is_image_set()
    {
    }

    nn::SmolVLMResp SmolVLM::send(const std::string &msg)
    {
        (void)msg;
        return nn::SmolVLMResp();
    }

    void SmolVLM::cancel()
    {
    }

} // namespace maix::nn
