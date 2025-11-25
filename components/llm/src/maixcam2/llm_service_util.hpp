#include "maix_basic.hpp"

namespace maix::nn
{
    bool isLLMServiceActive(const std::string& serviceName);
    err::Err check_start_llm_service(const std::string &url);
    err::Err check_stop_llm_service(const std::string &service_name);
};
