

#include "tokenizer_service_util.hpp"

namespace maix::nn
{
    // 执行命令并返回退出状态
    static int executeCommand(const std::string& command) {
        int status = std::system(command.c_str());
        return WEXITSTATUS(status);  // 提取退出码
    }

    // 检查服务是否处于active状态
    bool isLLMServiceActive(const std::string& serviceName) {
        std::string cmd = "systemctl is-active --quiet " + serviceName;
        int status = executeCommand(cmd);
        return status == 0;
    }

    // 尝试启动服务
    static bool startService(const std::string& serviceName) {
        std::string cmd = "systemctl start " + serviceName;
        int status = executeCommand(cmd);
        return status == 0;
    }

    static bool stopService(const std::string& serviceName) {
        std::string cmd = "systemctl stop " + serviceName;
        int status = executeCommand(cmd);
        return status == 0;
    }

    err::Err check_start_llm_service(const std::string &service_name)
    {
        if(!isLLMServiceActive(service_name))
        {
            log::info("%s is not active, starting it...", service_name.c_str());
            if(!startService(service_name))
            {
                log::error("%s is not active and start failed, please check by command `systemctl status %s", service_name.c_str(), service_name.c_str());
                return err::ERR_RUNTIME;
            }

            size_t timeout = 5000;
            size_t start_ms = time::ticks_ms();
            while (!app::need_exit() && time::ticks_ms() - start_ms < timeout) {
                if(isLLMServiceActive(service_name))
                    break;
                time::sleep_ms(100);
            }

            if (time::ticks_ms() - start_ms >= timeout) {
                log::error("%s is not active and stop failed, please check by command `systemctl status %s", service_name.c_str(), service_name.c_str());
                return err::ERR_RUNTIME;
            }
        }
        return err::ERR_NONE;
    }

    err::Err check_stop_llm_service(const std::string &service_name)
    {
        if(isLLMServiceActive(service_name))
        {
            stopService(service_name);

            size_t timeout = 5000;
            size_t start_ms = time::ticks_ms();
            while (!app::need_exit() && time::ticks_ms() - start_ms < timeout) {
                if(!isLLMServiceActive(service_name))
                    break;
                time::sleep_ms(100);
            }

            if (time::ticks_ms() - start_ms >= timeout) {
                log::error("%s is not active and stop failed, please check by command `systemctl status %s", service_name.c_str(), service_name.c_str());
                return err::ERR_RUNTIME;
            }
        }
        return err::ERR_NONE;
    }
};
