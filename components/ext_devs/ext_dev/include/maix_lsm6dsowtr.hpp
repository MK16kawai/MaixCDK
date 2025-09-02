/**
 * @author iawak9lkm
 * @copyright Sipeed Ltd 2024-
 * @license Apache 2.0
 * @update 2024.8.6: Add framework, create this file.
 */

#pragma once
#include "maix_basic.hpp"
#include "maix_imu.hpp"
#include <atomic>
#include <future>

namespace maix::ext_dev::lsm6dsowtr {

/**
 * LSM6DSOWTR driver class
 * @maixpy maix.ext_dev.lsm6dsowtr.LSM6DSOWTR
 */
class LSM6DSOWTR {
public:
    /**
     * @brief Construct a new LSM6DSOWTR object, will open LSM6DSOWTR
     *
     * @param mode LSM6DSOWTR Mode: ACC_ONLY/GYRO_ONLY/DUAL
     * @param acc_scale acc scale, see @imu::AccScale
     * @param acc_odr acc output data rate, see @imu::AccOdr
     * @param gyro_scale gyro scale, see @imu::GyroScale
     * @param gyro_odr gyro output data rate, see @imu::GyroOdr
     * @param block block or non-block, defalut is true
     *
     * @maixpy maix.ext_dev.lsm6dsowtr.LSM6DSOWTR.__init__
     */
    LSM6DSOWTR(
            maix::ext_dev::imu::Mode mode=maix::ext_dev::imu::Mode::DUAL,
            maix::ext_dev::imu::AccScale acc_scale=maix::ext_dev::imu::AccScale::ACC_SCALE_2G,
            maix::ext_dev::imu::AccOdr acc_odr=maix::ext_dev::imu::AccOdr::ACC_ODR_8000,
            maix::ext_dev::imu::GyroScale gyro_scale=maix::ext_dev::imu::GyroScale::GYRO_SCALE_16DPS,
            maix::ext_dev::imu::GyroOdr gyro_odr=maix::ext_dev::imu::GyroOdr::GYRO_ODR_8000,
            bool block=true);
    ~LSM6DSOWTR();

    LSM6DSOWTR(const LSM6DSOWTR&)             = delete;
    LSM6DSOWTR& operator=(const LSM6DSOWTR&)  = delete;
    LSM6DSOWTR(LSM6DSOWTR&&)                  = delete;
    LSM6DSOWTR& operator=(LSM6DSOWTR&&)       = delete;

    /**
     * @brief Read data from LSM6DSOWTR.
     *
     * @return list type. If only one of the outputs is initialized, only [x,y,z] of that output will be returned.
     *                    If all outputs are initialized, [acc_x, acc_y, acc_z, gyro_x, gyro_y, gyro_z] is returned.
     *
     * @maixpy maix.ext_dev.lsm6dsowtr.LSM6DSOWTR.read
     */
    std::vector<float> read();
private:
    void* _data;
    imu::Mode __mode;
    bool __block;
    // gyro
    std::string __gyro_device_path;
    std::vector<imu::GyroScale> __anglvel_scale_available;
    std::vector<imu::GyroOdr> __anglvel_sampling_freq_available;
    imu::GyroScale __curr_anglvel_scale;
    imu::GyroOdr __curr_anglvel_sampling_freq;
    // accel
    std::string __accel_device_path;
    std::vector<imu::AccScale> __accel_scale_available;
    std::vector<imu::AccOdr> __accel_sampling_freq_available;
    imu::AccScale __curr_accel_scale;
    imu::AccOdr __curr_accel_sampling_freq;
};
}