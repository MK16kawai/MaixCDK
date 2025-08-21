
#include "maix_basic.hpp"
#include "maix_vision.hpp"
#include "maix_nn_classifier.hpp"
#include "main.h"
#include "maix_touchscreen.hpp"
#include "maix_comm.hpp"
#include "maix_key.hpp"

using namespace maix;

image::Image *get_back_img(int screen_h)
{
    image::Image *ret_img = image::load("./assets/ret.png", image::Format::FMT_RGB888);
    if (!ret_img)
    {
        log::error("load ret image failed");
        return nullptr;
    }
    int new_h = screen_h * 0.15;
    int new_w = new_h / ret_img->height() * ret_img->width();
    new_h = new_h % 2 == 0 ? new_h : new_h + 1; // make sure height is even
    new_w = new_w % 2 == 0 ? new_w : new_w + 1; // make sure width is even
    if(new_h != ret_img->height() || new_w != ret_img->width())
    {
        log::info("resize ret image from %dx%d to %dx%d", ret_img->width(), ret_img->height(), new_w, new_h);
        image::Image *tmp_img = ret_img->resize(new_w, new_h, image::FIT_CONTAIN);
        if (!tmp_img)
        {
            log::error("resize ret image failed");
            delete ret_img;
            throw err::Exception(err::ERR_NO_MEM, "resize ret image failed");
        }
        delete ret_img;
        ret_img = tmp_img;
    }

    return ret_img;
}

int _main(int argc, char *argv[])
{
    int ret = 0;
    char tmp_chars[128] = {0};
    log::info("Program start");

    const char *model_path = "/root/models/mobilenetv2.mud";

    touchscreen::TouchScreen ts;
    int ts_x = 0, ts_y = 0;
    bool ts_pressed = false;

    log::info("model path: %s", model_path);
    nn::Classifier classifier(model_path);
    log::info("load classifier model %s success", model_path);

    char msg[64];
    display::Display disp = display::Display();
    int w = disp.width();
    int h = disp.height();
    int min_len = w > h ? h : w;
    // int max_len = w > h ? w : h;
    log::info("open camera now");
    // image::Size input_size = classifier.input_size();
    camera::Camera cam = camera::Camera(w, h, classifier.input_format());
    log::info("open camera success");

    auto ret_img = get_back_img(cam.height());
    int font_scale = 2;
    int font_thickness = 2;

    uint64_t t, t2, t3, t_show, t_all = 0;
    while (!app::need_exit())
    {
        ts.read(ts_x, ts_y, ts_pressed);
        if (ts_pressed && ts_x < 40 + 60 && ts_y < 40 + 60)
        {
            break;
        }
        t = time::ticks_ms();
        image::Image *img = cam.read();
        err::check_null_raise(img, "read camera failed");
        image::Image *img_final = img->resize(classifier.input_width(), classifier.input_height(), image::Fit::FIT_COVER);
        err::check_null_raise(img_final, "resize failed");
        t2 = time::ticks_ms();
        std::vector<std::pair<int, float>> *result = classifier.classify(*img_final);
        t3 = time::ticks_ms();
        int max_idx = result->at(0).first;
        float max_score = result->at(0).second;
        img->draw_rect((w - min_len) / 2, (h - min_len) / 2, min_len, min_len, image::COLOR_WHITE, font_thickness);
        snprintf(msg, sizeof(msg), "%4.1f %%:\n%s", max_score * 100, classifier.labels[max_idx].c_str());
        img->draw_string((w - min_len) / 2, disp.height() - 80, msg, image::COLOR_RED, font_scale, font_thickness);
        img->draw_image(0, 0, *ret_img);
        snprintf(tmp_chars, sizeof(tmp_chars), "All: %ldms, cam: %ldms\ndetect: %ldms, show: %ldms", t_all, t2 - t, t3 - t2, t_show);
        img->draw_string((w - min_len) / 2, 4, tmp_chars, image::COLOR_RED, font_scale, font_thickness);
        disp.show(*img);
        t_show = time::ticks_ms() - t3;
        t_all = time::ticks_ms() - t;
        delete result;
        delete img;
        delete img_final;
    }

    log::info("Program exit");

    return ret;
}

int main(int argc, char *argv[])
{
    // Catch SIGINT signal(e.g. Ctrl + C), and set exit flag to true.
    signal(SIGINT, [](int sig)
           { app::set_exit_flag(true); });

    // support default maix communication protol commands
    comm::add_default_comm_listener();

    // default key action
    peripheral::key::add_default_listener();

    // Use CATCH_EXCEPTION_RUN_RETURN to catch exception,
    // if we don't catch exception, when program throw exception, the objects will not be destructed.
    // So we catch exception here to let resources be released(call objects' destructor) before exit.
    CATCH_EXCEPTION_RUN_RETURN(_main, -1, argc, argv);
}
