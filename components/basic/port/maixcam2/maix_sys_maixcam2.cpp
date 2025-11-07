#include "maix_sys.hpp"
#include "maix_app.hpp"
#include "maix_util.hpp"
#include "signal.h"

namespace maix::sys
{
    static void signal_handle(int sig)
    {
        maix::app::set_exit_flag(true);
#if CONFIG_BUILD_WITH_MAIXPY
        raise(SIGINT);
#endif
        util::do_exit_function();
        signal(SIGINT, nullptr);
        signal(SIGILL, nullptr);
        signal(SIGTRAP, nullptr);
        signal(SIGABRT, nullptr);
        signal(SIGBUS, nullptr);
        signal(SIGFPE, nullptr);
        signal(SIGKILL, nullptr);
        signal(SIGSEGV, nullptr);
    }

    void poweroff()
    {
        util::do_exit_function();
        system("devmem 0x4840000 32 0x0");  // disable wdt0
        // system("devmem 0x2230000 32 0x0");  // disable wdt1
        system("devmem 0x6040000 32 0x0");  // disable wdt2
        int ret = system("poweroff");
        if (ret != 0)
        {
            throw err::Exception(err::Err::ERR_RUNTIME, "power off failed");
        }
    }

    void register_default_signal_handle() {
        signal(SIGILL, signal_handle);
        signal(SIGTRAP, signal_handle);
        signal(SIGABRT, signal_handle);
        signal(SIGBUS, signal_handle);
        signal(SIGFPE, signal_handle);
        signal(SIGKILL, signal_handle);
        signal(SIGSEGV, signal_handle);
        signal(SIGINT, signal_handle);
    }

    bool is_support(sys::Feature feature)
    {
        switch(feature)
        {
            default:
                return true;
        }
    }
}