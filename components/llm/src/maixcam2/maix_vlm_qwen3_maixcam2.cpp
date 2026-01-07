/**
 * LLM Qwen3VL implementation on MaixCam2
 * @license Apache-2.0
 * @author lxo@sipeed
 * @date 2025-11-24
 */

#include "maix_vlm_qwen3.hpp"
#include "maix_nn.hpp"
#include "ax_middleware.hpp"
#include "tokenizer_service_util.hpp"
#include "llm_service_util.hpp"
#include "httplib.h"
#include "http_utils.hpp"
#include "maix_basic.hpp"
#include "VLM_Qwen3.hpp"
#include "queue"

 namespace maix::nn
 {
    class Qwen3VLObj
    {
        public:
        MUD mud;
        std::vector<std::vector<unsigned short>> k_caches, v_caches;
        int precompute_len = 0;
        Qwen3VLResp resp;
        Qwen3VL *obj;
        int image_w;
        int image_h;
        maix::image::Format image_fmt;
        std::vector<unsigned short> img_embed;
        std::shared_ptr<httplib::Client> cli;
        VLM_Qwen3::LLMAttrType attr;
        std::queue<std::string> reply_queue;
        std::mutex mutex;
        bool exit_flag;
        bool update_system_prompt = false;
    };

    Qwen3VL::Qwen3VL(const std::string &model)
    {
        _data = new Qwen3VLObj();
        ((Qwen3VLObj*)_data)->obj = this;
        _model_path = model;
        _system_prompt = "You are Qwen3VL. You are a helpful vision-to-text assistant.";
        set_log_level(log::get_log_level(), log::get_log_use_color());
        if(!model.empty())
        {
            err::Err e = load(model);
            if(e != err::ERR_NONE)
            {
                char buf[128];
                snprintf(buf, sizeof(buf), "load model %s failed", model.c_str());
                throw err::Exception(e, buf);
            }
        }
    }

    Qwen3VL::~Qwen3VL()
    {
        unload();
        if (_data)
        {
            delete (Qwen3VLObj *)_data;
            _data = nullptr;
        }
    }

    void Qwen3VL::set_log_level(log::LogLevel level, bool color)
    {
        ax_log_use_color = color;
        switch(level)
        {
        case log::LogLevel::LEVEL_DEBUG:
            ax_log_level = SAMPLE_LOG_DEBUG;
            break;
        case log::LogLevel::LEVEL_WARN:
            ax_log_level = SAMPLE_LOG_WARN;
            break;
        case log::LogLevel::LEVEL_ERROR:
            ax_log_level = SAMPLE_LOG_ERROR;
            break;
        default:
            ax_log_level = SAMPLE_LOG_INFO;
            break;
        }
    }

    err::Err Qwen3VL::create_environment_file(nn::MUD &mud)
    {
        auto configs = mud.items;
        auto work_dir = fs::dirname(mud.model_path);
        auto path = fs::join({work_dir, configs["extra"]["service_env_path"]});
        auto exec_app_path = fs::join({work_dir, configs["extra"]["exec_app"]});
        auto template_filename = fs::join({work_dir, configs["basic"]["model_npu"]});
        auto model_num = configs["extra"]["model_num"];
        auto encoder_model_path = fs::join({work_dir, configs["extra"]["vpm_resampler_model"]});
        auto use_mmap_load_embed = configs["extra"]["use_mmap_load_embed"];
        auto tokenizer_url = configs["extra"]["tokenizer_url"];
        auto post_model_path = fs::join({work_dir, configs["extra"]["post_model"]});
        auto tokens_embed_path = fs::join({work_dir, configs["extra"]["tokens_embed"]});
        auto tokens_embed_num = configs["extra"]["tokens_embed_num"];
        auto tokens_embed_size = configs["extra"]["tokens_embed_size"];
        auto patch_size = configs["extra"]["patch_size"];
        auto img_width = configs["extra"]["img_w"];
        auto img_height = configs["extra"]["img_h"];
        auto vision_start_token_id = configs["extra"]["vision_start_token_id"];
        auto post_config_path = fs::join({work_dir, configs["extra"]["post_config_path"]});

        auto f = fs::open(path, "w+");
        if (!f) {
            log::error("open qwen3_vl.service.env failed");
            return err::ERR_RUNTIME;
        }
        f->write("WORK_DIR=" + work_dir + "\n");
        f->write("MAIN_API=" + exec_app_path + "\n");
        f->write("TEMPLATE_FILENAME_AXMODEL=" + template_filename + "\n");
        f->write("AXMODEL_NUM=" + model_num + "\n");
        f->write("ENCODER_MODEL_PATH=" + encoder_model_path + "\n");
        f->write("USE_MMAP_LOAD_EMBED=" + use_mmap_load_embed + "\n");
        f->write("TOKENIZER_URL=" + tokenizer_url + "\n");
        f->write("POST_MODEL_PATH=" + post_model_path + "\n");
        f->write("TOKENS_EMBED_PATH=" + tokens_embed_path + "\n");
        f->write("TOKENS_EMBED_NUM=" + tokens_embed_num + "\n");
        f->write("TOKENS_EMBED_SIZE=" + tokens_embed_size + "\n");
        f->write("PATCH_SIZE=" + patch_size + "\n");
        f->write("IMG_WIDTH=" + img_width + "\n");
        f->write("IMG_HEIGHT=" + img_height + "\n");
        f->write("VISION_START_TOKEN_ID=" + vision_start_token_id + "\n");
        f->write("POST_CONFIG_PATH=" + post_config_path + "\n");

        f->close();
        return err::ERR_NONE;
    }

    err::Err Qwen3VL::load(const std::string &model)
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        _model_path = model;

        if (!fs::exists(model)) {
            log::error("model %s not exists", model.c_str());
            return err::ERR_RUNTIME;
        }

        err::Err e = obj->mud.load(model);
        if(e != err::ERR_NONE)
            return e;

        e = this->create_environment_file(obj->mud);
        if(e != err::ERR_NONE) {
            log::error("create environment file failed, err:%d", e);
            return e;
        }

        std::string model_dir = fs::dirname(model);
        // init llm model
        VLM_Qwen3::LLMAttrType attr;
        attr.system_prompt = _system_prompt;
        attr.tokenizer_type = TKT_HTTP;
        bool ai_isp_on = app::get_sys_config_kv("npu", "ai_isp", "0") == "1" ? true : false;
        if(ai_isp_on)
        {
            log::warn("npu_ai_isp_on from config is on, but LLM model only support npu model, please not use camera or turn off ai_isp");
        }
        try
        {
            _version = obj->mud.items["extra"]["model_type"];
            _tokenizer_type = _version;
            attr.url_tokenizer_model = obj->mud.items["extra"]["tokenizer_url"];
            attr.url_llm_service = obj->mud.items["extra"]["llm_service_url"];
            attr.llm_service = obj->mud.items["extra"]["llm_service"];
            attr.filename_tokens_embed = fs::join({model_dir, obj->mud.items["extra"]["tokens_embed"]});
            attr.filename_post_axmodel = fs::join({model_dir, obj->mud.items["extra"]["post_model"]});
            attr.template_filename_axmodel = fs::join({model_dir, obj->mud.items["basic"]["model_npu"]});
            attr.axmodel_num = std::stoi(obj->mud.items["extra"]["model_num"]);
            attr.tokens_embed_num = std::stoi(obj->mud.items["extra"]["tokens_embed_num"]);
            attr.tokens_embed_size = std::stoi(obj->mud.items["extra"]["tokens_embed_size"]);
            attr.b_use_mmap_load_embed = (obj->mud.items["extra"]["use_mmap_load_embed"] == "true" || obj->mud.items["extra"]["use_mmap_load_embed"] == "1") ? true : false;
            attr.filename_vpm_resampler_axmodedl = fs::join({model_dir, obj->mud.items["extra"]["vpm_resampler_model"]});
            attr.vpm_len = std::stoi(obj->mud.items["extra"]["vpm_len"]);
            obj->image_w = std::stoi(obj->mud.items["extra"]["img_w"]);
            obj->image_h = std::stoi(obj->mud.items["extra"]["img_h"]);
            obj->image_fmt = image::Format::FMT_RGB888;
        }
        catch(...)
        {
            log::error("load model failed, key-value error in mud's extra section");
            return err::ERR_ARGS;
        }
        try
        {
            auto post_config_file = fs::join({model_dir, obj->mud.items["extra"]["post_config_path"]});
            std::ifstream file(post_config_file);
            nlohmann::json config = nlohmann::json::parse(file);
            post_config.enable_temperature = config.value("enable_temperature", true);
            post_config.temperature = config.value("temperature", 0.7f);
            post_config.enable_repetition_penalty = config.value("enable_repetition_penalty", false);
            post_config.repetition_penalty = config.value("repetition_penalty", 1.0f);
            post_config.penalty_window = config.value("penalty_window", 30);
            post_config.enable_top_p_sampling = config.value("enable_top_p_sampling", false);
            post_config.top_p = config.value("top_p", 0.8f);
            post_config.enable_top_k_sampling = config.value("enable_top_k_sampling", true);
            post_config.top_k = config.value("top_k", 20);
        }
        catch(...)
        {
            log::error("load model failed, key-value error in mud's post_config section");
            return err::ERR_ARGS;
        }

        if(obj->mud.items["extra"].find("tokenizer_type") != obj->mud.items["extra"].end())
        {
            _tokenizer_type = obj->mud.items["extra"]["tokenizer_type"];
        }

        attr.runing_callback = nullptr;
        attr.reserve = obj;

        // check tokenizer service
        // find http://127.0.0.1 in obj->mud.items["extra"]["tokenizer_url"]
        e = check_start_tokenizer_service(obj->mud.items["extra"]["tokenizer_url"]);
        if(e != err::ERR_NONE)
        {
            log::error("start tokenizer service failed");
            return e;
        }
        log::info("tokenizer service started");

        // check qwen3-vl service
        e = check_start_llm_service(obj->mud.items["extra"]["llm_service"]);
        if(e != err::ERR_NONE)
        {
            log::error("start qwen3-vl service failed");
            return e;
        }
        log::info("tokenizer %s started", obj->mud.items["extra"]["llm_service"].c_str());

        obj->image_fmt = maix::image::Format::FMT_RGB888;

        // print params
        log::info("model info:");
        log::print(log::LogLevel::LEVEL_INFO, "\tmodel type: %s\n", _version.c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\tmodel path: %s\n", obj->mud.items["basic"]["model_npu"].c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\tpost model path: %s\n", obj->mud.items["extra"]["post_model"].c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\ttokens embed path: %s\n", obj->mud.items["extra"]["tokens_embed"].c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\tuse_mmap_load_embed: %s\n", obj->mud.items["extra"]["use_mmap_load_embed"].c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\tmodel num: %d\n", attr.axmodel_num);
        log::print(log::LogLevel::LEVEL_INFO, "\ttokens embed num: %d\n", attr.tokens_embed_num);
        log::print(log::LogLevel::LEVEL_INFO, "\ttokens embed size: %d\n", attr.tokens_embed_size);
        log::print(log::LogLevel::LEVEL_INFO, "\ttokenizer url: %s\n", attr.url_tokenizer_model.c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\tinput image size: %d x %d\n", obj->image_w, obj->image_h);
        log::print(log::LogLevel::LEVEL_INFO, "\tinput image format: %s\n", maix::image::format_name(obj->image_fmt).c_str());
        log::print(log::LogLevel::LEVEL_INFO, "\n");
        obj->attr = attr;
        _loaded = true;
        return err::ERR_NONE;
    }

    err::Err Qwen3VL::unload()
    {
        err::Err e = err::ERR_NONE;
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        e = check_stop_llm_service(obj->attr.llm_service);
        _loaded = false;
        return e;
    }


    void Qwen3VL::set_system_prompt(const std::string &prompt)
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        _system_prompt = prompt;
        obj->update_system_prompt = true;
    }

    int Qwen3VL::input_width()
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        return obj->image_w;
    }

    int Qwen3VL::input_height()
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        return obj->image_h;
    }

    maix::image::Format Qwen3VL::input_format()
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        return obj->image_fmt;
    }

    err::Err Qwen3VL::set_image(maix::image::Image &img, maix::image::Fit fit)
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        maix::image::Image *p_img = &img;
        bool need_free = false;
        if(img.width() != obj->image_w || img.height() != obj->image_h)
        {
            p_img = img.resize(obj->image_w, obj->image_h, fit);
            need_free = true;
        }
        auto ret = p_img->save("/tmp/vlm_img.jpg");
        if (need_free)
            delete p_img;
        if(ret != err::ERR_NONE)
        {
            log::error("Encode image failed, ret: %d", ret);
            return err::ERR_RUNTIME;
        }
        return err::ERR_NONE;
    }

    void Qwen3VL::clear_image()
    {
        system("rm /tmp/vlm_img.jpg");
    }

    bool Qwen3VL::is_image_set()
    {
        return fs::exists("/tmp/vlm_img.jpg");
    }

    nn::Qwen3VLResp Qwen3VL::send(const std::string &msg)
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        if (this->is_ready() == false) {
            log::error("Model not ready");
            return nn::Qwen3VLResp();
        }
        auto cli = obj->cli;

        nlohmann::json j = {
            {"model", "AXERA-TECH/Qwen3-VL-2B-Instruct-GPTQ-Int4"},
            {"stream", true},
            {"messages", nlohmann::json::array({
                {
                    {"role", "user"},
                    {"content", nlohmann::json::array({
                        {
                            {"type", "text"},
                            {"text", msg}
                        },
                        {
                            {"type", "image_url"},
                            {"image_url", "/tmp/vlm_img.jpg"}
                        }
                    })}
                }
            })}
        };

        if (obj->update_system_prompt) {
            obj->update_system_prompt = false;
            nlohmann::json system_msg = {
                {"role", "system"},
                {"content", _system_prompt}
            };
            j["messages"].push_back(system_msg);
        }

        if (post_config.enable_temperature)
            j["temperature"] = post_config.temperature;
        if (post_config.enable_repetition_penalty)
            j["repetition_penalty"] = post_config.repetition_penalty;
        if (post_config.enable_top_p_sampling)
            j["top_p"] = post_config.top_p;
        if (post_config.enable_top_k_sampling)
            j["top_k"] = post_config.top_k;

        httplib::Headers headers {
            {"Content-Type", "application/json"}
        };

        obj->resp.msg = "";
        obj->resp.err_code = err::ERR_NONE;
        obj->resp.err_msg = "";
        obj->reply_queue = std::queue<std::string>();
        // log::info("json:%s", j.dump().c_str());
        auto ret = cli->Post("/v1/chat/completions", headers, j.dump(), "application/json", [&obj](const char *data, size_t data_length) {
            std::string chunk(data, data_length);
            // log::info("size:%d chunk:%s", data_length, chunk.c_str());
            bool json_parse_ok = false;

            nlohmann::json j_rep = nlohmann::json::parse(chunk, nullptr, false);
            if (!j_rep.is_discarded()) {
                auto choices_arr = j_rep.value("choices", nlohmann::json::array());
                if (choices_arr.size() > 0) {
                    // get content from {"choices":[{"delta":{"content":"0."}}]}
                    auto delta = choices_arr[0].value("delta", nlohmann::json::object());
                    if (delta.contains("content")) {
                        auto content = delta.value("content", "");
                        obj->resp.msg_new = content;
                        obj->resp.msg += content;
                        obj->resp.err_code = err::ERR_NONE;
                        obj->resp.err_msg = "";
                        json_parse_ok = true;
                    // get content from {"choices":[{"delta":{"content":"0.", ""}}]}
                    } else if (delta.contains("finish_reason")) {
                        auto finish_reason = delta.value("finish_reason", "");
                        if (finish_reason == "stop") {
                            obj->resp.err_code = err::ERR_NONE;
                            obj->resp.err_msg = "";
                            json_parse_ok = true;
                        }
                    }
                }
            }

            if (json_parse_ok) {
                auto callback = obj->obj->get_reply_callback();
                if (callback) {
                    callback(*obj->obj, obj->resp);
                }
            } else {
                // log::error("json parse error");
            }

            if (obj->exit_flag || app::need_exit()) {
                return false;
            }
            return true;
        });

        return obj->resp;
    }

    void Qwen3VL::cancel()
    {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        obj->exit_flag = true;
    }

    bool Qwen3VL::is_ready() {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        try {
            if (!obj->cli) {
                obj->cli = std::make_shared<httplib::Client>(obj->attr.url_llm_service);
                if (!obj->cli) {
                    log::error("Failed connect to llm service, url:%s", obj->attr.url_llm_service.c_str());
                    return false;
                }

                obj->cli->set_connection_timeout(10);
                obj->cli->set_read_timeout(10);
                obj->cli->set_write_timeout(10);
                obj->cli->set_keep_alive(true);
                auto res = obj->cli->Get("/v1/models");
                if (res != nullptr) {
                    auto rep = res.value();
                    return rep.status == 200 ? true : false;
                } else {
                    return false;
                }
            } else {
                if (obj->cli->is_socket_open()) {
                    auto res = obj->cli->Get("/v1/models");
                    if (res != nullptr) {
                        auto rep = res.value();
                        return rep.status == 200 ? true : false;
                    } else {
                        return false;
                    }
                } else {
                    obj->cli = std::make_shared<httplib::Client>(obj->attr.url_llm_service);
                    if (!obj->cli) {
                        log::error("Failed connect to llm service, url:%s", obj->attr.url_llm_service.c_str());
                        return false;
                    }

                    obj->cli->set_connection_timeout(10);
                    obj->cli->set_read_timeout(10);
                    obj->cli->set_write_timeout(10);
                    obj->cli->set_keep_alive(true);

                    auto res = obj->cli->Get("/v1/models");
                    if (res != nullptr) {
                        auto rep = res.value();
                        return rep.status == 200 ? true : false;
                    } else {
                        return false;
                    }
                }
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << e.what() << '\n';
            return false;
        }
    }

    err::Err Qwen3VL::start_service() {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;
        auto e = check_start_tokenizer_service(obj->mud.items["extra"]["tokenizer_url"]);
        if(e != err::ERR_NONE)
        {
            log::error("start tokenizer service failed");
            return e;
        }

        // check qwen3-vl service
        e = check_start_llm_service(obj->mud.items["extra"]["llm_service"]);
        if(e != err::ERR_NONE)
        {
            log::error("start qwen3-vl service failed");
            return e;
        }

        return err::ERR_NONE;
    }

    err::Err Qwen3VL::stop_service() {
        Qwen3VLObj *obj = (Qwen3VLObj *)_data;

        // check qwen3-vl service
        auto e = check_start_llm_service(obj->mud.items["extra"]["llm_service"]);
        if(e != err::ERR_NONE)
        {
            log::error("start qwen3-vl service failed");
            return e;
        }

        return err::ERR_NONE;
    }
 } // namespace maix::nn
