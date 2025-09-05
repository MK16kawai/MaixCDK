/**
 * @author lxowalle
 * @copyright Sipeed Ltd 2024-
 * @license Apache 2.0
 * @update 2025-09-02: Add framework, create this file.
 */

#include "maix_lsm6dsowtr.hpp"
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace maix::ext_dev::lsm6dsowtr {
#define GYRO_DEVICE_NAME "lsm6dsox_gyro"
#define ACCEL_DEVICE_NAME "lsm6dsox_accel"

static std::string __load_file_to_string(std::string path) {
    FILE* fp = fopen(path.c_str(), "r");
    if (!fp) {
        perror("fopen");
        return "";
    }

    std::string content;
    char buffer[128];

    while (fgets(buffer, sizeof(buffer), fp)) {
        content += buffer;
    }

    fclose(fp);
    return content;
}

static bool __write_string_to_file(std::string path, std::string content) {
    FILE* fp = fopen(path.c_str(), "w");
    if (!fp) {
        perror("fopen");
        return false;
    }

    fprintf(fp, "%s", content.c_str());
    fclose(fp);
    return true;
}

static float __invert_gyro_scale_to_float(imu::GyroScale scale) {
    if (scale >= imu::GyroScale::GYRO_SCALE_2000DPS) {
        return 0.001221729f;
    } else if (scale >= imu::GyroScale::GYRO_SCALE_1000DPS) {
        return 0.000610865f;
    } else if (scale >= imu::GyroScale::GYRO_SCALE_500DPS) {
        return 0.000305432f;
    } else {
        return 0.000152716f;
    }
}

static float __invert_gyro_sampling_rate_to_float(imu::GyroOdr odr) {
    if (odr <= imu::GyroOdr::GYRO_ODR_833) {
        return 833;
    } else if (odr <= imu::GyroOdr::GYRO_ODR_416) {
        return 416;
    } else if (odr <= imu::GyroOdr::GYRO_ODR_208) {
        return 208;
    } else if (odr <= imu::GyroOdr::GYRO_ODR_104) {
        return 104;
    } else if (odr <= imu::GyroOdr::GYRO_ODR_52) {
        return 52;
    } else if (odr <= imu::GyroOdr::GYRO_ODR_26) {
        return 26;
    } else {
        return 12.5;
    }
}

static float __invert_accel_scale_to_float(imu::AccScale scale) {
    if (scale >= imu::AccScale::ACC_SCALE_16G) {
        return 0.004785645f;
    } else if (scale >= imu::AccScale::ACC_SCALE_8G) {
        return 0.002392822f;
    } else if (scale >= imu::AccScale::ACC_SCALE_4G) {
        return 0.001196411f;
    } else {
        return 0.000598205f;
    }
}

static float __invert_accel_sampling_rate_to_float(imu::AccOdr odr) {
    if (odr <= imu::AccOdr::ACC_ODR_833) {
        return 833;
    } else if (odr <= imu::AccOdr::ACC_ODR_416) {
        return 416;
    } else if (odr <= imu::AccOdr::ACC_ODR_208) {
        return 208;
    } else if (odr <= imu::AccOdr::ACC_ODR_104) {
        return 104;
    } else if (odr <= imu::AccOdr::ACC_ODR_52) {
        return 52;
    } else if (odr <= imu::AccOdr::ACC_ODR_26) {
        return 26;
    } else {
        return 12.5;
    }
}

static std::vector<std::string> __get_device_paths() {
    std::vector<std::string> device_paths;
    auto iio_device_base_path = "/sys/bus/iio/devices/iio:device";
    int idx = 0;
    while (!app::need_exit()) {
        auto path = iio_device_base_path + std::to_string(idx) + "/name";
        FILE *f = fopen(path.c_str(), "r");
        if (f) {
            char name[256];
            if (fgets(name, sizeof(name), f) != NULL) {
                name[strcspn(name, "\r\n")] = 0;
                if (!strcmp(name, GYRO_DEVICE_NAME) || !strcmp(name, ACCEL_DEVICE_NAME)) {
                    device_paths.push_back(iio_device_base_path + std::to_string(idx));
                }
            }
            fclose(f);
            idx ++;
        } else {
            break;
        }
    }

    return device_paths;
}

// static std::vector<std::string> __string_to_vector_string(std::string input) {
//     std::istringstream iss(input);
//     std::vector<std::string> values;
//     std::string token;

//     while (iss >> token) {   // 按空格分割
//         values.push_back(token);
//     }
//     return values;
// }

static std::vector<float> __read_gyro_data(std::string path) {
    auto gyro_scale = (float)std::atof(__load_file_to_string(path + "/in_anglvel_scale").c_str());
    auto gyro_x = (float)std::atof(__load_file_to_string(path + "/in_anglvel_x_raw").c_str());
    auto gyro_y = (float)std::atof(__load_file_to_string(path + "/in_anglvel_y_raw").c_str());
    auto gyro_z = (float)std::atof(__load_file_to_string(path + "/in_anglvel_z_raw").c_str());
    return {gyro_x * gyro_scale, gyro_y * gyro_scale, gyro_z * gyro_scale};
}

static std::vector<float> __read_accel_data(std::string path) {
    auto accel_scale = (float)std::atof(__load_file_to_string(path + "/in_accel_scale").c_str());
    auto accel_x = (float)std::atof(__load_file_to_string(path + "/in_accel_x_raw").c_str());
    auto accel_y = (float)std::atof(__load_file_to_string(path + "/in_accel_y_raw").c_str());
    auto accel_z = (float)std::atof(__load_file_to_string(path + "/in_accel_z_raw").c_str());
    return {accel_x * accel_scale, accel_y * accel_scale, accel_z * accel_scale};
}

LSM6DSOWTR::LSM6DSOWTR(imu::Mode mode, imu::AccScale acc_scale,
                imu::AccOdr acc_odr, imu::GyroScale gyro_scale, imu::GyroOdr gyro_odr, bool block)
{
    auto device_paths = __get_device_paths();
    bool found_gyro_device = false, found_accel_device = false;
    for (auto path : device_paths) {
        auto name = __load_file_to_string(path + "/name");
        name[strcspn(name.c_str(), "\r\n")] = 0;
        if (!strcmp(name.c_str(), GYRO_DEVICE_NAME)) {
            found_gyro_device = true;
            __gyro_device_path = path;
        } else if (!strcmp(name.c_str(), ACCEL_DEVICE_NAME)) {
            found_accel_device = true;
            __accel_device_path = path;
        }
    }

    err::check_bool_raise(found_gyro_device, "Can't find gyro device!");
    err::check_bool_raise(found_accel_device, "Can't find accel device!");

    __mode = mode;
    __block = block;

    auto curr_acc_scale = __invert_accel_scale_to_float(acc_scale);
    err::check_bool_raise(__write_string_to_file(__accel_device_path + "/in_accel_scale", std::to_string(curr_acc_scale)), "Config accel scale failed!");

    auto curr_acc_odr = __invert_accel_sampling_rate_to_float(acc_odr);
    err::check_bool_raise(__write_string_to_file(__accel_device_path + "/sampling_frequency", std::to_string(curr_acc_odr)), "Config accel odr failed!");

    auto curr_gyro_scale = __invert_gyro_scale_to_float(gyro_scale);
    err::check_bool_raise(__write_string_to_file(__gyro_device_path + "/in_anglvel_scale", std::to_string(curr_gyro_scale)), "Config gyro scale failed!");

    auto curr_gyro_odr = __invert_gyro_sampling_rate_to_float(gyro_odr);
    err::check_bool_raise(__write_string_to_file(__gyro_device_path + "/sampling_frequency", std::to_string(curr_gyro_odr)), "Config gyro odr failed!");
}

LSM6DSOWTR::~LSM6DSOWTR()
{
}

std::vector<float> LSM6DSOWTR::read()
{
    std::vector<float> ret;
    if (__mode == imu::Mode::GYRO_ONLY) {
        ret = __read_gyro_data(__gyro_device_path);
        ret.push_back(0);
    } else if (__mode == imu::Mode::ACC_ONLY) {
        ret = __read_accel_data(__accel_device_path);
        ret.push_back(0);
    } else {
        ret = __read_accel_data(__accel_device_path);
        auto gyro_data = __read_gyro_data(__gyro_device_path);
        ret.insert(ret.end(), gyro_data.begin(), gyro_data.end());
        ret.push_back(0);
    }

    return ret;
}

}