/**
 * @author neucrack@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2025.8.29: support mixformer v2.
 */

#pragma once
#include "maix_basic.hpp"
#include "maix_nn.hpp"
#include "maix_image.hpp"
#include "maix_nn_F.hpp"
#include "maix_nn_object.hpp"
#include <cmath>

namespace maix::nn
{
    /**
     * MixFormerV2 class
     * @maixpy maix.nn.MixFormerV2
     */
    class MixFormerV2
    {
    public:
        /**
         * Constructor of MixFormerV2 class
         * @param model model path, default empty, you can load model later by load function.
         * @throw If model arg is not empty and load failed, will throw err::Exception.
         * @maixpy maix.nn.MixFormerV2.__init__
         * @maixcdk maix.nn.MixFormerV2.MixFormerV2
         */
         MixFormerV2(const string &model = "")
        {
            _model = nullptr;
            _dual_buff = false;
            _template_input = nullptr;
            _template_online_input = nullptr;
            _search_input = nullptr;
            _last_template_crop_size = 0;

            // mixformer params
            _search_factor = 4.5;
            _template_factor = 2.0;
            _template_update_interval = 200; // ms

            // load model
            if (!model.empty())
            {
                err::Err e = load(model);
                if (e != err::ERR_NONE)
                {
                    throw err::Exception(e, "load model failed");
                }
            }
        }

        ~MixFormerV2()
        {
            if (_template_input)
            {
                delete _template_input;
                _template_input = nullptr;
            }
            if (_template_online_input)
            {
                delete _template_online_input;
                _template_online_input = nullptr;
            }
            if (_search_input)
            {
                delete _search_input;
                _search_input = nullptr;
            }
            _free_models();
        }

        /**
         * Load model from file
         * @param model Model path want to load
         * @return err::Err
         * @maixpy maix.nn.MixFormerV2.load
         */
        err::Err load(const string &model)
        {
            _free_models();
            _model = new nn::NN(model, _dual_buff);
            if (!_model)
            {
                return err::ERR_NO_MEM;
            }
            _extra_info = _model->extra_info();
            if (_extra_info.find("model_type") != _extra_info.end())
            {
                if (_extra_info["model_type"] != "mixformer-v2")
                {
                    log::error("model_type not match, expect 'mixformer-v2', but got '%s'", _extra_info["model_type"].c_str());
                    return err::ERR_ARGS;
                }
            }
            else
            {
                log::error("model_type key not found");
                return err::ERR_ARGS;
            }
            log::info("model info:\n\ttype: mixformer-v2");
            if (_extra_info.find("input_type") != _extra_info.end())
            {
                std::string input_type = _extra_info["input_type"];
                if (input_type == "rgb")
                {
                    _input_img_fmt = maix::image::FMT_RGB888;
                    log::print(log::LogLevel::LEVEL_INFO, "\tinput type: rgb\n");
                }
                else if (input_type == "bgr")
                {
                    _input_img_fmt = maix::image::FMT_BGR888;
                    log::print(log::LogLevel::LEVEL_INFO, "\tinput type: bgr\n");
                }
                else
                {
                    log::error("unknown input type: %s", input_type.c_str());
                    return err::ERR_ARGS;
                }
            }
            else
            {
                log::error("input_type key not found");
                return err::ERR_ARGS;
            }
            if (_extra_info.find("mean") != _extra_info.end())
            {
                std::string mean_str = _extra_info["mean"];
                std::vector<std::string> mean_strs = split(mean_str, ",");
                log::print(log::LogLevel::LEVEL_INFO, "\tmean:");
                for (auto &it : mean_strs)
                {
                    try
                    {
                        this->mean.push_back(std::stof(it));
                    }
                    catch (std::exception &e)
                    {
                        log::error("mean value error, should float");
                        return err::ERR_ARGS;
                    }
                    log::print(log::LogLevel::LEVEL_INFO, "%f ", this->mean.back());
                }
                log::print(log::LogLevel::LEVEL_INFO, "\n");
            }
            else
            {
                log::info("mean use 0");
            }
            if (_extra_info.find("scale") != _extra_info.end())
            {
                std::string scale_str = _extra_info["scale"];
                std::vector<std::string> scale_strs = split(scale_str, ",");
                log::print(log::LogLevel::LEVEL_INFO, "\tscale:");
                for (auto &it : scale_strs)
                {
                    try
                    {
                        this->scale.push_back(std::stof(it));
                    }
                    catch (std::exception &e)
                    {
                        log::error("scale value error, should float");
                        return err::ERR_ARGS;
                    }
                    log::print(log::LogLevel::LEVEL_INFO, "%f ", this->scale.back());
                }
                log::print(log::LogLevel::LEVEL_INFO, "\n");
            }
            else
            {
                log::info("scale use 1");
            }
            std::vector<nn::LayerInfo> inputs = _model->inputs_info();
            int w_idx = 3;
            int h_idx = 2;
            if(inputs[0].shape[3] <= 4) // nhwc
            {
                w_idx = 2;
                h_idx = 1;
            }
            int w = inputs[0].shape[w_idx];
            int h = inputs[0].shape[h_idx];
            int w_template = w;
            int h_template = h;
            for(int i=1; i<3; ++i)
            {
                if(inputs[i].shape[w_idx] > w)
                {
                    w = inputs[i].shape[w_idx];
                    h = inputs[i].shape[h_idx];
                }
                if (inputs[i].shape[w_idx] < w_template)
                {
                    w_template = inputs[i].shape[w_idx];
                    h_template = inputs[i].shape[h_idx];
                }
            }
            _input_size = image::Size(w, h);
            _input_size_template = image::Size(w_template, h_template);
            log::print(log::LogLevel::LEVEL_INFO, "\tinput size: %dx%d, target template size: %dx%d\n\n", _input_size.width(), _input_size.height(), _input_size_template.width(), _input_size_template.height());
            return err::ERR_NONE;
        }

        /**
         * Init tracker, give tacker first target image and target position.
         * @param img Image want to detect, target should be in this image.
         * @param x the target position left top coordinate x.
         * @param y the target position left top coordinate y.
         * @param w the target width.
         * @param h the target height.
         * @throw If image format not match model input format, will throw err::Exception.
         * @maixpy maix.nn.MixFormerV2.init
         */
        void init(image::Image &img, int x, int y, int w, int h)
        {
            if (img.format() != _input_img_fmt)
            {
                throw err::Exception("image format not match, input_type: " + image::fmt_names[_input_img_fmt] + ", image format: " + image::fmt_names[img.format()]);
            }
            // crop image
            int c_x = x + w / 2;
            int c_y = y + h / 2;
            _last_template_crop_size = _get_crop_size(w, h, _template_factor);
            _get_input(img, c_x, c_y, _last_template_crop_size, _input_size_template.width(), _input_size_template.height(), &_template_input);

            _last_target.x = c_x; // center, not left-top
            _last_target.y = c_y; // center, not left-top
            _last_target.w = w;
            _last_target.h = h;
        }

        /**
         * Track object acoording to last object position and the init function learned target feature.
         * @param img image to detect object and track, can be any resolution, before detect it will crop a area according to last time target's position.
         * @param threshold If score < threshold, will see this new detection is invalid, but remain return this new detecion,  default 0.9.
         * @return object, position and score, and detect area in points's first 4 element(x, y, w, h, center_x, center_y, input_size, target_size)
         * @maixpy maix.nn.MixFormerV2.track
        */
        nn::Object track(image::Image &img, float threshold = 0.9)
        {
            if (img.format() != _input_img_fmt)
            {
                throw err::Exception("image format not match, input_type: " + image::fmt_names[_input_img_fmt] + ", image format: " + image::fmt_names[img.format()]);
            }
            // crop search area input tensor
            // printf("last: %d %d %d %d\n", _last_target.x, _last_target.y, _last_target.w, _last_target.h);
            int crop_size = _get_crop_size(_last_target.w, _last_target.h, _search_factor);
            auto crop_info = _get_input(img, _last_target.x, _last_target.y, crop_size, _input_size.width(), _input_size.height(), &_search_input);

            // forward
            tensor::Tensors inputs;
            inputs.add_tensor("img_t", _template_input, false, false);
            inputs.add_tensor("img_ot", _template_online_input == nullptr ? _template_input : _template_online_input, false, false);
            inputs.add_tensor("img_search", _search_input, false, false);
            tensor::Tensors *outputs = _model->forward(inputs, false, true);
            // decode result
            float score = 0;
            int cx = 0, cy = 0, w = 0, h = 0;
            for (auto it = outputs->begin(); it != outputs->end(); it++)
            {
                auto out_tensor = it->second;
                if(out_tensor->size_int() == 1) // score
                {
                    score = *(float*)out_tensor->data();
                    // printf("score: %3.2f\n", score);
                }
                else
                {
                    // cx, cy, w, h [0, 1]
                    float *data = (float*)out_tensor->data();
                    // printf("bbox: %5.4f %5.4f %5.4f %5.4f\n", data[0], data[1],data[2],data[3]);
                    // printf("crop info: %5.4f %5.4f %5.4f %5.4f %5.4f %5.4f\n", crop_info[0], crop_info[1],crop_info[2],crop_info[3], crop_info[4], crop_info[5]);
                    cx = (int)(data[0] * _input_size.width()  / crop_info[4] + crop_info[0]);
                    cy = (int)(data[1] * _input_size.height() / crop_info[5] + crop_info[1]);
                    w  = (int)(data[2] * _input_size.width()  / crop_info[4]);
                    h  = (int)(data[3] * _input_size.height() / crop_info[5]);
                    // printf("bbox int: %d %d %d %d\n", cx, cy, w, h);
                }
            }
            // printf("\n");
            _bbox_clip(cx, cy, w, h, img.width(), img.height());
            delete outputs;

            // update status
            if(score >= threshold)
            {
                _last_target.x = cx;
                _last_target.y = cy;
                _last_target.w = w;
                _last_target.h = h;
            }
            int crop_size_template = _get_crop_size(_last_target.w, _last_target.h, _template_factor);

            // clear data
            // nn::Object res(_last_target.x - _last_target.w / 2, _last_target.y - _last_target.h / 2, _last_target.w, _last_target.h, 0, score);
            nn::Object res(cx - w / 2, cy - h / 2, w, h, 0, score);
            res.points.resize(8);
            res.points.at(0) = (int)(crop_info[0]);
            res.points.at(1) = (int)(crop_info[1]);
            res.points.at(2) = (int)(crop_info[2] - crop_info[0]);
            res.points.at(3) = (int)(crop_info[3] - crop_info[1]);
            res.points.at(4) = cx;
            res.points.at(5) = cy;
            res.points.at(6) = crop_size;
            res.points.at(7) = crop_size_template;
            return res;
        }

        /**
         * Get model input size
         * @return model input size
         * @maixpy maix.nn.MixFormerV2.input_size
         */
        image::Size input_size()
        {
            return _input_size;
        }

        /**
         * Get model input width
         * @return model input size of width
         * @maixpy maix.nn.MixFormerV2.input_width
         */
        int input_width()
        {
            return _input_size.width();
        }

        /**
         * Get model input height
         * @return model input size of height
         * @maixpy maix.nn.MixFormerV2.input_height
         */
        int input_height()
        {
            return _input_size.height();
        }

        /**
         * Get input image format
         * @return input image format, image::Format type.
         * @maixpy maix.nn.MixFormerV2.input_format
         */
        image::Format input_format()
        {
            return _input_img_fmt;
        }

    public:
        /**
         * Get mean value, list type
         * @maixpy maix.nn.MixFormerV2.mean
         */
        std::vector<float> mean;

        /**
         * Get scale value, list type
         * @maixpy maix.nn.MixFormerV2.scale
         */
        std::vector<float> scale;

    private:
        nn::NN *_model;
        float _search_factor;
        float _template_factor;
        float _template_update_interval; // ms
        image::Size _input_size;
        image::Size _input_size_template;
        image::Format _input_img_fmt;
        std::map<string, string> _extra_info;
        bool _dual_buff;
        tensor::Tensor *_template_input;
        tensor::Tensor *_template_online_input;
        tensor::Tensor *_search_input;
        int _last_template_crop_size;
        nn::Object _last_target;

    private:
        inline int _get_crop_size(int w, int h, float scale_factor)
        {
            return ceil(std::sqrt(w * h) * scale_factor);
        }

        std::vector<float> _get_input(image::Image &img, int c_x, int c_y, int crop_size, int out_w, int out_h, tensor::Tensor **out_tensor)
        {
            int crop_x_x1, crop_x_y1, crop_x_x2, crop_x_y2;
            int x1, y1, x2, y2;
            image::Image *img_target = _padding_crop(img, c_x, c_y, crop_size, crop_x_x1, crop_x_y1, crop_x_x2, crop_x_y2, x1, y1, x2, y2);
            float resize_scale_x = 1.0;
            float resize_scale_y = 1.0;
            if(out_w != img_target->width() || out_h != img_target->height())
            {
                resize_scale_x = 1.0 * out_w / img_target->width();
                resize_scale_y = 1.0 * out_h / img_target->height();
                image::Image *img_target_resized = img_target->resize(out_w, out_h, image::Fit::FIT_FILL);
                delete img_target;
                img_target = img_target_resized;
            }
            bool chw = true;
            img.draw_image(0, 0, *img_target);
            img_target->to_tensor_float32(out_tensor, chw, this->mean, this->scale);
            if((*out_tensor)->shape().size() != 4)
                (*out_tensor)->expand_dims(0);
            delete img_target;
            return {
                static_cast<float>(x1),
                static_cast<float>(y1),
                static_cast<float>(x2),
                static_cast<float>(y2),
                resize_scale_x,
                resize_scale_y
            };
        }

        inline float _sz(float w, float h)
        {
            float pad = (w + h) * 0.5;
            return std::sqrt((w + pad) * (h + pad));
        }

        inline float _change(float r)
        {
            float t = 1.0 / r;
            return r > t ? r : t;
        }

        inline void _bbox_clip(int &cx, int &cy, int &w, int &h, int max_w, int max_h)
        {
            cx = max(0, min(cx, max_w - 1));
            cy = max(0, min(cy, max_h - 1));
            w = max(10, min(w, max_w));
            h = max(10, min(h, max_h));
        }

        void _free_models()
        {
            if (_model)
            {
                delete _model;
                _model = nullptr;
            }
        }

        image::Image *_padding_crop(image::Image &img, int c_x, int c_y, int size_target, int &copy_x1, int &copy_y1, int &copy_x2, int &copy_y2,
            int &x1, int &y1, int &x2, int &y2
        )
        {
            x1 = c_x - size_target / 2;
            y1 = c_y - size_target / 2;
            x2 = c_x + size_target / 2;
            y2 = c_y + size_target / 2;
            // not pad
            if (x1 >= 0 && y1 >= 0 && x2 < img.width() && y2 < img.height())
            {
                copy_x1 = x1;
                copy_y1 = y1;
                copy_x2 = x2;
                copy_y2 = y2;
                return img.crop(x1, y1, size_target, size_target);
            }
            image::Image *res = new image::Image(size_target, size_target, img.format());
            if (!res)
                throw err::Exception(err::ERR_NO_MEM);
            copy_x1 = max(0, x1);
            copy_y1 = max(0, y1);
            copy_x2 = min(img.width(), x2);
            copy_y2 = min(img.height(), y2);
            uint8_t *data = (uint8_t *)res->data();
            uint8_t *src = (uint8_t *)img.data();
            // pad
            for (int y = 0; y < copy_y1 - y1; ++y)
            {
                memset(data + y * res->width() * 3, 0, (x2 - x1) * 3);
            }
            for (int y = copy_y2; y < y2; ++y)
            {
                memset(data + (y - y1) * res->width() * 3, 0, (x2 - x1) * 3);
            }
            for (int y = copy_y1; y < copy_y2; ++y)
            {
                memset(data + (y - y1) * res->width() * 3, 0, (copy_x1 - x1) * 3);
            }
            for (int y = copy_y1; y < copy_y2; ++y)
            {
                memset(data + ((y - y1) * res->width() + copy_x2) * 3, 0, (x2 - copy_x2) * 3);
            }
            for (int y = copy_y1; y < copy_y2; ++y)
            {
                uint8_t *p = data + ((y - y1) * res->width() + (copy_x1 - x1)) * 3;
                uint8_t *s = src + (y * img.width() + copy_x1) * 3;
                int length = (copy_x2 - copy_x1) * 3;
                memcpy(p, s, length);
            }
            // res->save("/root/test.png");
            return res;
        }

        std::vector<std::vector<float>> _generate_points(int stride = 16, int size = 15)
        {
            // 计算原点位置
            float ori = -(size / 2) * stride;

            // 创建points向量
            std::vector<std::vector<float>> points(size * size, std::vector<float>(2, 0.0f));

            int index = 0;
            for (int dy = 0; dy < size; ++dy)
            {
                for (int dx = 0; dx < size; ++dx)
                {
                    float x = ori + stride * dx;
                    float y = ori + stride * dy;
                    points[index][0] = x;
                    points[index][1] = y;
                    ++index;
                }
            }

            return points;
        }

        std::vector<float> _generate_hanning_window(int size)
        {
            std::vector<float> hanning(size);
            for (int i = 0; i < size; ++i)
            {
                hanning[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
            }
            return hanning;
        }

        void _generate_window(int size, std::vector<float> &window)
        {
            std::vector<float> hanning = _generate_hanning_window(size);

            window.resize(size * size);
            for (int i = 0; i < size; ++i)
            {
                for (int j = 0; j < size; ++j)
                {
                    window[i * size + j] = hanning[i] * hanning[j];
                }
            }
        }

        static void split0(std::vector<std::string> &items, const std::string &s, const std::string &delimiter)
        {
            items.clear();
            size_t pos_start = 0, pos_end, delim_len = delimiter.length();
            std::string token;

            while ((pos_end = s.find(delimiter, pos_start)) != std::string::npos)
            {
                token = s.substr(pos_start, pos_end - pos_start);
                pos_start = pos_end + delim_len;
                items.push_back(token);
            }

            items.push_back(s.substr(pos_start));
        }

        static std::vector<std::string> split(const std::string &s, const std::string &delimiter)
        {
            std::vector<std::string> tokens;
            split0(tokens, s, delimiter);
            return tokens;
        }

        static void _softmax_2xn(float *data, int size)
        {
            float *p1 = data;
            float *p2 = data + size;
            float large;
            float sum;
            for(int i=0; i< size; ++i)
            {
                large = *p1 > *p2 ? *p1 : *p2;
                *p1 = expf(*p1 - large);
                *p2 = expf(*p2 - large);
                sum = *p1 + *p2;
                *p1 /= sum;
                *p2 /= sum;
                ++p1;
                ++p2;
            }
        }
    };

} // namespace maix::nn
