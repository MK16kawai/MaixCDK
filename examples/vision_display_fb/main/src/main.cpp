

#include "maix_basic.hpp"
#include "maix_display.hpp"
#include "main.h"


using namespace maix;

int _main(int argc, char* argv[])
{
    std::string fb_dev = "/dev/fb0";
    if(argc > 1)
    {
        fb_dev = argv[1];
    }
    log::info("fb dev: %s", fb_dev.c_str());
    auto disp = display::Display();
    auto disp_fb = display::Display(-1, -1, image::Format::FMT_BGRA8888, fb_dev);
    int y = 0;
    while(!app::need_exit())
    {
        image::Image img = image::Image(disp_fb.width(), disp_fb.height(), image::Format::FMT_BGRA8888, image::COLOR_BLACK);
        img.draw_string(0, y, "hello", image::COLOR_WHITE, 2, 2);
        disp_fb.show(img);
        y = (y + 1) % img.height();
    }
}

int main(int argc, char* argv[])
{
    // Catch signal and process
    sys::register_default_signal_handle();

    // Use CATCH_EXCEPTION_RUN_RETURN to catch exception,
    // if we don't catch exception, when program throw exception, the objects will not be destructed.
    // So we catch exception here to let resources be released(call objects' destructor) before exit.
    CATCH_EXCEPTION_RUN_RETURN(_main, -1, argc, argv);
}

