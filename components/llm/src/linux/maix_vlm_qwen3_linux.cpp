/**
 * LLM Qwen3VL implementation on MaixCam2
 * @license Apache-2.0
 * @author lxo@sipeed
 * @date 2025-11-24
 */

#include "maix_vlm_qwen3.hpp"
#include "maix_nn.hpp"
#include "maix_basic.hpp"

 namespace maix::nn
 {

    Qwen3VL::Qwen3VL(const std::string &model)
    {
        (void)model;
    }

    Qwen3VL::~Qwen3VL()
    {
    }

    void Qwen3VL::set_log_level(log::LogLevel level, bool color)
    {
        (void)level;
        (void)color;
    }

    err::Err Qwen3VL::load(const std::string &model)
    {
        (void)model;
        return err::ERR_NOT_IMPL;
    }

    err::Err Qwen3VL::unload()
    {
        return err::ERR_NOT_IMPL;
    }


    void Qwen3VL::set_system_prompt(const std::string &prompt)
    {
        (void)prompt;
    }

    int Qwen3VL::input_width()
    {
        return 0;
    }

    int Qwen3VL::input_height()
    {
        return 0;
    }

    maix::image::Format Qwen3VL::input_format()
    {
        return maix::image::Format::FMT_RGB888;
    }

    err::Err Qwen3VL::set_image(maix::image::Image &img, maix::image::Fit fit)
    {
        (void)img;
        (void)fit;
        return err::ERR_NONE;
    }

    void Qwen3VL::clear_image()
    {
    }

    bool Qwen3VL::is_image_set()
    {
        return false;
    }

    nn::Qwen3VLResp Qwen3VL::send(const std::string &msg)
    {
        (void)msg;
        return nn::Qwen3VLResp();
    }

    void Qwen3VL::cancel()
    {

    }

    bool Qwen3VL::is_ready() {
        return false;
    }

    err::Err Qwen3VL::start_service() {
        return err::ERR_NOT_IMPL;
    }

    err::Err Qwen3VL::stop_service() {
        return err::ERR_NOT_IMPL;
    }
 } // namespace maix::nn
