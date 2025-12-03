/**
 * VLM SmolVLM
 * @license: Apache-2.0
 * @author: neucrack@sipeed
 * @date: 2025-05-30
 */
#pragma once

#include "maix_basic.hpp"
#include "maix_image.hpp"
#include "maix_llm_qwen.hpp"

namespace maix::nn
{
        /**
     * SmolVLM model response
     * @maixpy maix.nn.SmolVLMResp
     */
     class SmolVLMResp
     {
     public:
     SmolVLMResp()
         {
             err_code = err::ERR_NONE;
             err_msg = "";
         }

         /**
          * Model response full message.
          * @maixpy maix.nn.SmolVLMResp.msg
          */
         std::string msg;

         /**
          * Model response new message.
          * @maixpy maix.nn.SmolVLMResp.msg_new
          */
         std::string msg_new;

         /**
          * Model response error code, maix.Err type, should be err.Err.ERR_NONE if no error.
          * @maixpy maix.nn.SmolVLMResp.err_code
          */
         err::Err err_code;

         /**
          * Model response error message.
          * @maixpy maix.nn.SmolVLMResp.err_msg
          */
         std::string err_msg;
     };

     /**
      * SmolVLM model post config
      * @maixpy maix.nn.SmolVLMPostConfig
      */
     class SmolVLMPostConfig
     {
     public:
        SmolVLMPostConfig()
         {
             enable_temperature = true;
             temperature = 0.9;

             enable_repetition_penalty = false;
             repetition_penalty = 1.2;
             penalty_window = 20;

             enable_top_p_sampling = false;
             top_p = 0.8;

             enable_top_k_sampling = true;
             top_k = 10;
         }

         /**
          * Enable temperature sampling
          * @maixpy maix.nn.SmolVLMPostConfig.enable_temperature
          */
         bool enable_temperature;

         /**
          * Temperature sampling value
          * @maixpy maix.nn.SmolVLMPostConfig.temperature
          */
         float temperature;

         /**
          * Enable repetition penalty
          * @maixpy maix.nn.SmolVLMPostConfig.enable_repetition_penalty
          */
         bool enable_repetition_penalty;

         /**
          * Repetition penalty value
          * @maixpy maix.nn.SmolVLMPostConfig.repetition_penalty
          */
         float repetition_penalty;

         /**
          * Repetition penalty window
          * @maixpy maix.nn.SmolVLMPostConfig.penalty_window
          */
         int penalty_window;

         /**
          * Enable diversity penalty
          * @maixpy maix.nn.SmolVLMPostConfig.enable_top_p_sampling
          */
         bool enable_top_p_sampling;

         /**
          * Diversity penalty value
          * @maixpy maix.nn.SmolVLMPostConfig.top_p
          */
         float top_p;

         /**
          * Enable top k sampling
          * @maixpy maix.nn.SmolVLMPostConfig.enable_top_k_sampling
          */
         bool enable_top_k_sampling;

         /**
          * Top k sampling value
          * @maixpy maix.nn.SmolVLMPostConfig.top_k
          */
         int top_k;
     };

    /**
     * SmolVLM model
     * @maixpy maix.nn.SmolVLM
     */
    class SmolVLM
    {
    public:
        /**
         * SmolVLM constructor
         * @param[in] model model file path, model format can be MUD(model universal describe file) file.
         *                  If model_path set, will load model from file, load failed will raise err.Exception.
         *                  If model_path not set, you can load model later by load function.
         * @maixpy maix.nn.SmolVLM.__init__
         * @maixcdk maix.nn.SmolVLM.SmolVLM
         */
         SmolVLM(const std::string &model);

        ~SmolVLM();

        /**
         * Load model from file
         * @param[in] model model file path, model format can be MUD(model universal describe file) file.
         * @return error code, if load success, return err::ERR_NONE
         * @maixpy maix.nn.SmolVLM.load
         */
        err::Err load(const std::string &model);

        /**
         * Unload model
         * @return error code, if unload success, return err::ERR_NONE
         * @maixpy maix.nn.SmolVLM.unload
         */
        err::Err unload();

        /**
         * Is model loaded
         * @return true if model loaded, else false
         * @maixpy maix.nn.SmolVLM.loaded
         */
        bool loaded()
        {
            return _loaded;
        }

        /**
         * Set system prompt
         * @param prompt system prompt
         * @maixpy maix.nn.SmolVLM.set_system_prompt
         */
        void set_system_prompt(const std::string &prompt);

        /**
         * Get system prompt
         * @return system prompt
         * @maixpy maix.nn.SmolVLM.get_system_prompt
         */
        std::string get_system_prompt()
        {
            return _system_prompt;
        }

        /**
         * Set log level
         * @param level log level, @see maix.log.LogLevel
         * @param color true to enable color, false to disable color
         * @maixpy maix.nn.SmolVLM.set_log_level
         */
        void set_log_level(log::LogLevel level, bool color);

        /**
         * Set reply callback.
         * @param callback reply callback, when token(words) generated, this function will be called,
         * so you can get response message in real time in this callback funtion.
         * If set to None(nullptr in C++), you can get response after all response message generated.
         * @maixpy maix.nn.SmolVLM.set_reply_callback
         */
        void set_reply_callback(std::function<void(nn::SmolVLM &, const nn::SmolVLMResp &)> callback = nullptr)
        {
            _callback = callback;
        }

        /**
         * Get reply callback
         * @return reply callback
         * @maixpy maix.nn.SmolVLM.get_reply_callback
         */
        std::function<void(nn::SmolVLM &, const nn::SmolVLMResp &)> get_reply_callback()
        {
            return _callback;
        }

        /**
         * Image input width
         * @return input width.
         * @maixpy maix.nn.SmolVLM.input_width
         */
        int input_width();

        /**
         * Image input height
         * @return input height.
         * @maixpy maix.nn.SmolVLM.input_height
         */
        int input_height();

        /**
         * Image input format
         * @return input format.
         * @maixpy maix.nn.SmolVLM.input_format
         */
        maix::image::Format input_format();

        /**
         * Set image and will encode image.
         * You can set image once and call send multiple times.
         * @param img the image you want to use.
         * @param fit Image resize fit method, only used when img size not equal to model input.
         * @return err.Err return err.Err.ERR_NONE is no error happen.
         * @maixpy maix.nn.SmolVLM.set_image
         */
        err::Err set_image(maix::image::Image &img, maix::image::Fit fit = maix::image::Fit::FIT_CONTAIN);

        /**
         * Clear image, SmolVLM2.5 based on Qwen2.5, so you can clear image and only use LLM function.
         * @maixpy maix.nn.SmolVLM.clear_image
         */
         void clear_image();

         /**
          * Whether image set by set_image
          * @return Return true if image set by set_image function, or return false.
          * @maixpy maix.nn.SmolVLM.is_image_set
          */
         bool is_image_set();

        /**
         * Send message to model
         * @param msg message to send
         * @return model response
         * @maixpy maix.nn.SmolVLM.send
         */
        nn::SmolVLMResp send(const std::string &msg);

        /**
         * Cancel running
         * @maixpy maix.nn.SmolVLM.cancel
         */
         void cancel();

        /**
         * Get model version
         * @return model version
         * @maixpy maix.nn.SmolVLM.version
         */
        std::string version()
        {
            return _version;
        }

    public:
        /**
         * SmolVLM post config, default will read config from model mud file, you can also set it manually here.
         * @maixpy maix.nn.SmolVLM.post_config
         */
        nn::SmolVLMPostConfig post_config;

    private:
        bool _loaded = false;
        std::string _system_prompt;
        std::string _model_path;
        std::string _version;
        std::string _tokenizer_type;
        std::function<void(SmolVLM &, const SmolVLMResp &)> _callback = nullptr;
        void *_data; // for implementation
    };

}


