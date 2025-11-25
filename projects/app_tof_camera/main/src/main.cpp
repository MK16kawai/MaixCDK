
#include "global_config.h"
#include "global_build_info_time.h"
#include "global_build_info_version.h"


/**
 * @file main
 *
 */

/*********************
 *      INCLUDES
 *********************/
#include <iostream>
#include <sys/resource.h>

#include "maix_basic.hpp"
#include "maix_display.hpp"
#include "maix_lvgl.hpp"
#include "maix_camera.hpp"
#include "maix_image.hpp"
#include "maix_util.hpp"
#include "maix_comm.hpp"
#include "lvgl.h"
#include "app.hpp"

#include "maix_cmap.hpp"
#include "maix_tof100.hpp"
#include "mix.hpp"

using namespace maix;
using namespace maix::image;

using namespace maix::ext_dev::tof100;

void point_map_init_with_res(Resolution res)
{
    switch (res) {
    case Resolution::RES_100x100:
        println("remap -> (100, 100)");
        point_map_init(100, 100);
        break;
    case Resolution::RES_25x25:
        [[fallthrough]];
    case Resolution::RES_50x50:
        println("remap -> (50, 50)");
        point_map_init(50, 50);
        break;
    default: break;
    }
}

void show_error(display::Display& disp, touchscreen::TouchScreen& ts)
{
    ui_total_deinit();
    auto img = Image(disp.width(), disp.height());
    std::string msg = "Devices Not Found!\nSupport list:\nPMOD_TOF100\n\nClicked to exit.";
    img.draw_string(0, 100, msg);
    disp.show(img);
    while (!app::need_exit()) {
        time::sleep_ms(200);
        auto tsd = ts.read();
        if (tsd.empty()) continue;
        if (tsd[2] == 1) break;
    }
}

static void wait_enter()
{
    std::string dummy;
    std::getline(std::cin, dummy);  // consume pending input
    std::getline(std::cin, dummy);  // wait for actual Enter
}

int _main(int argc, char **argv)
{
    display::Display disp = display::Display(-1, -1, image::FMT_RGB888);
    err::check_bool_raise(disp.is_opened(), "camera open failed");
    
    // Retain for re-testing purposes. Please ignore during reading.
    if constexpr (false) {
        constexpr int BIG_W = 66;
        constexpr int BIG_H = 50;
        auto image_rgb = image::Image(BIG_W, BIG_H, image::Format::FMT_RGB888);
        image_rgb.draw_rect(0, 0, BIG_W, BIG_H, image::Color::from_rgb(255, 0, 0), -1);

        auto image_rgb2 = image::Image(BIG_W, BIG_H, image::Format::FMT_RGB888);
        image_rgb2.draw_rect(0, 0, BIG_W, BIG_H, image::Color::from_rgb(0, 255, 0), -1);
        image_rgb2.draw_rect(8, 0, BIG_H, BIG_H, image::Color::from_rgb(0, 0, 255), -1);
        auto resize_img = image_rgb2.resize(640, 480);
        while (!app::need_exit()) {
            disp.show(image_rgb);
            time::sleep(1);
            disp.show(*resize_img);
            // disp.show(image_rgb2);
            time::sleep(1);
            delete resize_img;
        }
        return 0;
    }

    if constexpr (false) {
        constexpr int CAM_W = 50;
        constexpr int CAM_H = 50;
        auto cam = camera::Camera(CAM_W, CAM_H);

        while (!app::need_exit()) {
            auto image = cam.read();
            auto new_image = image->resize(CAM_W, CAM_H);
            println("cam: %dx%d", cam.width(), cam.height());
            println("image: %s", image->to_str().c_str());
            println("new_image: %s", new_image->to_str().c_str());
            disp.show(*new_image);
            delete new_image;
            delete image;
        }  

        return 0;
    }

    display::Display *other_disp = disp.add_channel();
    err::check_bool_raise(other_disp->is_opened(), "display open failed");

    println("disp: %dx%d, odisp: %dx%d",
        disp.width(), disp.height(), 
        other_disp->width(), other_disp->height()
    );

    // Retain for re-testing purposes. Please ignore during reading.
    if constexpr (false) {
        auto image_rgb = image::Image(640, 480, image::Format::FMT_RGB888);
        image_rgb.draw_rect(0, 0, 640, 480, image::Color::from_rgb(255, 0, 0), -1);
        auto image_rgba1 = image::Image(640, 480, image::Format::FMT_BGRA8888);
        image_rgba1.draw_rect(0, 0, 640, 480, image::Color::from_bgra(0, 255, 0, 0), -1);
        auto image_rgba2 = image::Image(640, 480, image::Format::FMT_BGRA8888);
        image_rgba2.draw_rect(0, 0, 640, 480, image::Color::from_bgra(0, 255, 0, 1), -1);

        disp.show(image_rgb);

        while (!app::need_exit()) {
            other_disp->show(image_rgba1);
            disp.show(image_rgb);
            time::sleep(1);
            other_disp->show(image_rgba2);
            disp.show(image_rgb);
            time::sleep(1);
        }
        return 0;
    }

    touchscreen::TouchScreen touchscreen = touchscreen::TouchScreen();
    err::check_bool_raise(touchscreen.is_opened(), "touchscreen open failed");
    maix::lvgl_init(other_disp, &touchscreen);
    maix::ext_dev::cmap::Cmap prev_cmap = g_cmap;
    Resolution prev_res = g_res;

    ui_total_init(disp.width(), disp.height());
    point_map_init_with_res(prev_res);
    // println("cam cam_onfo_o %dx%d", cam_onfo_o.w, cam_onfo_o.h);
    // std::unique_ptr<camera::Camera> cam = std::make_unique<camera::Camera>(cam_onfo_o.w, cam_onfo_o.h);
    std::unique_ptr<camera::Camera> cam = std::make_unique<camera::Camera>(50, 50);
    float aspect_ratio = static_cast<float>(disp.width()) / disp.height();

    std::unique_ptr<Tof100> opns;
    int OPNS_MIN_DIS_MM = 40; // 4cm
    int OPNS_MAX_DIS_MM = 750; // 100cm
    int spi_num = 4;
#if PLATFORM_MAIXCAM
    spi_num = 4;
#elif PLATFORM_MAIXCAM2
    spi_num = 2;
#endif
    try {
        /* try to init opns */
        opns.reset(new Tof100(spi_num, prev_res, prev_cmap, OPNS_MIN_DIS_MM, OPNS_MAX_DIS_MM));
    } catch (...) {
        eprintln("Device Not Found!");
        show_error(disp, touchscreen);
    }

    // opns.reset(new Tof100(4, prev_res, prev_cmap, OPNS_MIN_DIS_MM, OPNS_MAX_DIS_MM));
    Image* rimage = nullptr;
    while (!app::need_exit()) {
        if (g_cmap != prev_cmap || g_res != prev_res) {
            prev_cmap = g_cmap;
            prev_res = g_res;
            point_map_init_with_res(prev_res);
            opns.reset(new Tof100(spi_num, prev_res, prev_cmap, 40, 1000));
        }

        auto matrix = opns->matrix();
        if (matrix.empty()) {
            eprintln("matrix = (null)");
            continue;
        }

        std::unique_ptr<Image> img;
        std::vector<uint8_t> mix_data;

        if (g_fusion_mode) {
            if (g_res != Resolution::RES_50x50) {
                g_res = Resolution::RES_50x50;
                continue;
            }
            if (cam_need_reset) {
                println("cam need reset --> %dx%d", g_camera_img_info.w, g_camera_img_info.h);
                cam.reset(nullptr);
                time::sleep(1);
                println("cam g_camera_img_info %dx%d", g_camera_img_info.w, g_camera_img_info.h);
                cam.reset(new camera::Camera(g_camera_img_info.w, g_camera_img_info.h));
                time::sleep(1);
                cam_need_reset = false;
                println("cam rest finish, %dx%d", cam->width(), cam->height());
#if PLATFORM_MAIXCAM2
                point_map_init_with_res(prev_res);
                opns.reset(new Tof100(spi_num, prev_res, prev_cmap, 40, 1000));
#endif
                continue;
            }

            std::unique_ptr<Image> cam_img = [&](){
                auto oimage = std::unique_ptr<Image>(cam->read());
                if (oimage->width() == cam->width() && oimage->height() == cam->height()) {
                    return oimage;
                }
                return std::unique_ptr<Image>(oimage->resize(cam->width(), cam->height()));
            }();
            std::unique_ptr<Image> crop_image;

            // DEBUG
            if constexpr (false) {
                println("g_camera_img_info.w = %d", g_camera_img_info.w);
                println("g_camera_img_info.h = %d", g_camera_img_info.h);
                println("g_camera_img_info.x1 = %d", g_camera_img_info.x1);
                println("g_camera_img_info.y1 = %d", g_camera_img_info.y1);
                println("g_camera_img_info.ww = %d", g_camera_img_info.ww);
                println("g_camera_img_info.hh = %d", g_camera_img_info.hh);
                println("image format: %s", cam_img->to_str().c_str());
            }

            if (g_camera_img_info.w != -1 || g_camera_img_info.h != -1) {
                auto _tmp_img = std::unique_ptr<Image>(
                    cam_img->crop(
                        g_camera_img_info.x1, g_camera_img_info.y1,
                        g_camera_img_info.ww, g_camera_img_info.hh
                    )
                );
                crop_image.reset(_tmp_img->resize(50, 50));
            }

            int _dx = 3400;
            int _dn = 100;
            mix_data = std::move(
                mix2<uint32_t>(
                    matrix, OPNS_MAX_DIS_MM, OPNS_MIN_DIS_MM, 
                    _dx, _dn, 
                    crop_image ? (uint8_t*)crop_image->data() : (uint8_t*)cam_img->data(), 0.5f, //0.5f,
                    true
                )
            );
            if (mix_data.empty()) {
                eprintln("mix_data = (null)");
                continue;
            }
            img.reset(new Image(50, 50, FMT_RGB888, mix_data.data(), mix_data.size(), false));
        } else {
            img.reset(opns->image_from(matrix));
        }

        if (img.get() == nullptr) {
            eprintln("img = (null)");
            continue;
        }

        int __new_w = static_cast<int>(img->width()*aspect_ratio);
        __new_w += __new_w%2;
        auto show_img = Image(__new_w, img->height(), FMT_RGB888);
        show_img.clear();

        // DEBUG
        if constexpr (false) {
            println("iw:%d, ih:%d, sw:%d, sh:%d", 
                img->width(), img->height(), 
                show_img.width(), show_img.height()
            );
        }

        int img_draw_x_oft = (show_img.width()-show_img.height()) / 2;
        show_img.draw_image(img_draw_x_oft, 0, *img);

        // release old image
        if (rimage != nullptr) {
            delete rimage;
        }
        rimage = show_img.resize(disp.width(), disp.height());
        disp.show(*rimage);
        
        // Retain for re-testing purposes. Please ignore during reading.
        if constexpr (false) {
            const auto [centerpx, centerpy, centerpv] = opns->center_point();
            println("centerp (%d, %d, %d)", centerpx, centerpy, centerpv);
            const auto [minpx, minpy, minpv] = opns->min_dis_point();
            println("minp (%d, %d, %d)", minpx, minpy, minpv);
            const auto [maxpx, maxpy, maxpv] = opns->max_dis_point();
            println("maxp (%d, %d, %d)", maxpx, maxpy, maxpv);
        }
        
        upate_3crosshair_and_label(opns->min_dis_point(), opns->max_dis_point(), opns->center_point());
        update_user_crosshair(matrix);

        lv_timer_handler();

        // Retain for re-testing purposes. Please ignore during reading.
        if constexpr (false) {
            println("Press Enter to continue...");
            wait_enter();
        } 

        // Retain for re-testing purposes. Please ignore during reading.
        if constexpr (false) {
            auto fps = time::fps();
            println("fps: %0.2f", fps);
        }
    }

    if (rimage != nullptr) {
        delete rimage;
    }

    return 0;
}


int main(int argc, char* argv[])
{
    // Catch signal and process
    sys::register_default_signal_handle();

    // support default maix communication protol commands
    comm::add_default_comm_listener();

    // Use CATCH_EXCEPTION_RUN_RETURN to catch exception,
    // if we don't catch exception, when program throw exception, the objects will not be destructed.
    // So we catch exception here to let resources be released(call objects' destructor) before exit.
    CATCH_EXCEPTION_RUN_RETURN(_main, -1, argc, argv);
}

