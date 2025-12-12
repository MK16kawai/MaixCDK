#include "main.h"
#include "maix_util.hpp"
#include "maix_image.hpp"
#include "maix_time.hpp"
#include "maix_display.hpp"
#include "maix_webrtc.hpp"
#include "maix_camera.hpp"
#include "maix_basic.hpp"
#include "csignal"

using namespace maix;

int _main(int argc, char* argv[])
{
    camera::Camera cam = camera::Camera(640, 480, image::Format::FMT_YVU420SP, "", 30, 3);
    webrtc::WebRTC webrtc = webrtc::WebRTC();
    webrtc.bind_camera(&cam);

    log::info("url:%s", webrtc.get_url().c_str());

    std::vector<std::string> url = webrtc.get_urls();
    for (size_t i = 0; i < url.size(); i ++) {
        log::info("url[%d]:%s", i, url[i].c_str());
    }

    err::check_raise(webrtc.start());

    while(!app::need_exit()) {
        time::sleep_ms(10);
    }

    webrtc.stop();

    return 0;
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
