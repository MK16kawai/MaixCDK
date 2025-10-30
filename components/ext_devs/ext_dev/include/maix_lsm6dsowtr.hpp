#pragma once

#include "maix_imu.hpp"
#include "maix_basic.hpp"
#include <vector>
#include <future>
#include <cstdint>

namespace maix::ext_dev::lsm6dsowtr {

class LSM6DSOWTR {
public:
    LSM6DSOWTR(int i2c_bus = -1, uint8_t addr = 0x6B,
               imu::Mode mode = imu::Mode::DUAL,
               imu::AccScale acc_scale = imu::AccScale::ACC_SCALE_2G,
               imu::AccOdr acc_odr = imu::AccOdr::ACC_ODR_104,
               imu::GyroScale gyro_scale = imu::GyroScale::GYRO_SCALE_250DPS,
               imu::GyroOdr gyro_odr = imu::GyroOdr::GYRO_ODR_104,
               bool block = false);

    ~LSM6DSOWTR();

    std::vector<float> read();

private:
    void* _data = nullptr;  // points to priv::Lsm6dsowI2C
    imu::Mode _mode;
    std::future<std::pair<int, std::string>> open_future;
    bool open_fut_need_get = false;
};

} // namespace maix::ext_dev::lsm6dsowtr