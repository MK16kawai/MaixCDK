/**
 * @author Your Name
 * @copyright Sipeed Ltd 2025
 * @license Apache 2.0
 * @brief LSM6DSOW driver using Maix I2C abstraction (no ioctl)
 */

#include "maix_lsm6dsowtr.hpp"
#include "maix_i2c.hpp"
#include "maix_log.hpp"
#include "maix_time.hpp"
#include <memory>
#include <mutex>
#include <map>
#include <cmath>
#include <cstring>

namespace maix::ext_dev::lsm6dsowtr::priv {

// --- I2C bus manager (shared across devices) ---
static const int DEFAULT_I2C_BUS = 1;
static const char* TAG = "MAIX LSM6DSOW";

struct I2cInfo {
    ::maix::peripheral::i2c::I2C* i2c_bus{nullptr};
    int dev_num{0};
};

static std::recursive_mutex mtx;
static std::map<int, I2cInfo> i2c_manager;

::maix::peripheral::i2c::I2C* get_i2c_bus(int bus, uint32_t freq_khz, bool& is_new) {
    int _bus = (bus < 0) ? DEFAULT_I2C_BUS : bus;
    std::lock_guard<std::recursive_mutex> lock(mtx);
    auto it = i2c_manager.find(_bus);
    if (it != i2c_manager.end()) {
        is_new = false;
        it->second.dev_num++;
        return it->second.i2c_bus;
    }
    is_new = true;
    auto i2c = new ::maix::peripheral::i2c::I2C(_bus, ::maix::peripheral::i2c::Mode::MASTER, (int)freq_khz * 1000);
    i2c->scan(); // optional
    I2cInfo info{.i2c_bus = i2c, .dev_num = 1};
    i2c_manager[_bus] = info;
    return i2c;
}

void release_i2c_bus(int bus) {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    auto it = i2c_manager.find(bus);
    if (it == i2c_manager.end()) return;
    it->second.dev_num--;
    if (it->second.dev_num <= 0) {
        delete it->second.i2c_bus;
        i2c_manager.erase(it->first);
    }
}

// --- LSM6DSOW register map ---
static constexpr uint8_t WHO_AM_I       = 0x0F;
static constexpr uint8_t STATUS_REG     = 0x1E;
static constexpr uint8_t OUTX_L_A       = 0x28;
static constexpr uint8_t OUTX_L_G       = 0x22;
static constexpr uint8_t CTRL1_XL       = 0x10;
static constexpr uint8_t CTRL2_G        = 0x11;

static constexpr float ACC_SENS_TABLE[4] = {0.061f, 0.122f, 0.244f, 0.488f};  // mg/LSB
static constexpr float GYRO_SENS_TABLE[4] = {8.75f, 17.5f, 35.0f, 70.0f};     // mdps/LSB
static constexpr float GRAVITY = 9.80665f;
static constexpr float ODR_TABLE[11] = {
    0, 12.5f, 26, 52, 104, 208, 416, 833, 1660, 3330, 6660
};

struct lsm6dsow_data_t {
    struct { float x, y, z; } acc_xyz;
    struct { float x, y, z; } gyro_xyz;
    float temperature = 0.0f;
};

class Lsm6dsowI2C {
public:
    ::maix::peripheral::i2c::I2C* i2c = nullptr;
    uint8_t addr = 0x6B;
    bool is_open = false;
    uint8_t device_id = 0;

    Lsm6dsowI2C(int bus, uint8_t _addr, uint32_t freq_khz)
        : addr(_addr) {
        bool is_new;
        i2c = get_i2c_bus(bus, freq_khz, is_new);
    }

    ~Lsm6dsowI2C() {
        // Note: i2c bus is managed globally, not deleted here
    }

    uint8_t read_reg(uint8_t reg) {
        return this->read_reg_block(reg, 1)[0];
    }

    std::vector<uint8_t> read_reg_block(uint8_t reg, size_t len) {
        // 发送寄存器地址
        i2c->writeto(addr, std::vector<uint8_t>{reg});

        // 读取 len 字节
        maix::Bytes* bytes = i2c->readfrom(addr, (int)len);
        std::vector<uint8_t> result;
        if (bytes && bytes->size() >= len) {
            result.assign(bytes->begin(), bytes->begin() + len);
        }
        delete bytes;
        return result;
    }

    void write_reg(uint8_t reg, uint8_t val) {
        std::vector<uint8_t> data = {reg, val};  // [寄存器地址, 值]
        i2c->writeto(addr, data);
    }

    bool check_device() {
        device_id = read_reg(WHO_AM_I);
        return (device_id == 0x6C); // LSM6DSOW WHO_AM_I = 0x6C
    }

    void set_acc_odr(float hz) {
        int best_idx = 0;
        float min_diff = std::abs(ODR_TABLE[0] - hz);
        for (int i = 1; i < 11; ++i) {
            float diff = std::abs(ODR_TABLE[i] - hz);
            if (diff < min_diff) {
                min_diff = diff;
                best_idx = i;
            }
        }
        uint8_t val = read_reg(CTRL1_XL);
        val = (val & 0x0F) | (best_idx << 4);
        write_reg(CTRL1_XL, val);
    }

    void set_acc_scale(int g_val) {
        int fs = 0;
        if (g_val == 2) fs = 0;
        else if (g_val == 4) fs = 1;
        else if (g_val == 8) fs = 2;
        else if (g_val == 16) fs = 3;
        uint8_t val = read_reg(CTRL1_XL);
        val = (val & ~0x0C) | (fs << 2);
        write_reg(CTRL1_XL, val);
    }

    void set_gyro_odr(float hz) {
        int best_idx = 0;
        float min_diff = std::abs(ODR_TABLE[0] - hz);
        for (int i = 1; i < 11; ++i) {
            float diff = std::abs(ODR_TABLE[i] - hz);
            if (diff < min_diff) {
                min_diff = diff;
                best_idx = i;
            }
        }
        uint8_t val = read_reg(CTRL2_G);
        val = (val & 0x0F) | (best_idx << 4);
        write_reg(CTRL2_G, val);
    }

    void set_gyro_scale(int dps_val) {
        int fs = 0;
        if (dps_val == 250) fs = 0;
        else if (dps_val == 500) fs = 1;
        else if (dps_val == 1000) fs = 2;
        else if (dps_val == 2000) fs = 3;
        uint8_t val = read_reg(CTRL2_G);
        val = (val & ~0x0C) | (fs << 2);
        write_reg(CTRL2_G, val);
    }

    bool read_data(lsm6dsow_data_t& out) {
        uint8_t status = read_reg(STATUS_REG);
        if ((status & 0x01) == 0 || (status & 0x02) == 0) {
            return false; // no new data
        }

        auto acc_bytes = read_reg_block(OUTX_L_A, 6);
        auto gyro_bytes = read_reg_block(OUTX_L_G, 6);

        auto to_i16 = [](const std::vector<uint8_t>& b, size_t i) -> int16_t {
            return static_cast<int16_t>(b[i] | (b[i+1] << 8));
        };

        int16_t acc_raw[3] = {
            to_i16(acc_bytes, 0),
            to_i16(acc_bytes, 2),
            to_i16(acc_bytes, 4)
        };
        int16_t gyro_raw[3] = {
            to_i16(gyro_bytes, 0),
            to_i16(gyro_bytes, 2),
            to_i16(gyro_bytes, 4)
        };

        float acc_sens = ACC_SENS_TABLE[(read_reg(CTRL1_XL) >> 2) & 0x03];
        float gyro_sens = GYRO_SENS_TABLE[(read_reg(CTRL2_G) >> 2) & 0x03];

        for (int i = 0; i < 3; ++i) {
            out.acc_xyz.x = acc_raw[0] * acc_sens * 1e-3f * GRAVITY;
            out.acc_xyz.y = acc_raw[1] * acc_sens * 1e-3f * GRAVITY;
            out.acc_xyz.z = acc_raw[2] * acc_sens * 1e-3f * GRAVITY;
            out.gyro_xyz.x = gyro_raw[0] * gyro_sens * 1e-3f;
            out.gyro_xyz.y = gyro_raw[1] * gyro_sens * 1e-3f;
            out.gyro_xyz.z = gyro_raw[2] * gyro_sens * 1e-3f;
        }
        return true;
    }
};

} // namespace maix::ext_dev::lsm6dsowtr::priv

// --- Public API ---

using namespace maix::ext_dev::lsm6dsowtr::priv;
namespace maix::ext_dev::lsm6dsowtr {

LSM6DSOWTR::LSM6DSOWTR(int i2c_bus, uint8_t addr,
                       imu::Mode mode,
                       imu::AccScale acc_scale,
                       imu::AccOdr acc_odr,
                       imu::GyroScale gyro_scale,
                       imu::GyroOdr gyro_odr,
                       bool block)
    : _mode(mode)
{
    auto dev = new Lsm6dsowI2C(i2c_bus, addr, 400); // 400kHz
    _data = dev;

    // Map enums to numeric values
    int acc_g = 2;
    switch (acc_scale) {
        case imu::AccScale::ACC_SCALE_2G:  acc_g = 2; break;
        case imu::AccScale::ACC_SCALE_4G:  acc_g = 4; break;
        case imu::AccScale::ACC_SCALE_8G:  acc_g = 8; break;
        case imu::AccScale::ACC_SCALE_16G: acc_g = 16; break;
        default:  acc_g = 16; break;
    }

    int gyro_dps = 250;
    switch (gyro_scale) {
        case imu::GyroScale::GYRO_SCALE_250DPS:  gyro_dps = 250; break;
        case imu::GyroScale::GYRO_SCALE_500DPS:  gyro_dps = 500; break;
        case imu::GyroScale::GYRO_SCALE_1000DPS: gyro_dps = 1000; break;
        case imu::GyroScale::GYRO_SCALE_2000DPS: gyro_dps = 2000; break;
        default:  gyro_dps = 2000; break;
    }

    float acc_hz = 104.0f;
    switch (acc_odr) {
        case imu::AccOdr::ACC_ODR_12_5: acc_hz = 12.5f; break;
        case imu::AccOdr::ACC_ODR_26:   acc_hz = 26; break;
        case imu::AccOdr::ACC_ODR_52:   acc_hz = 52; break;
        case imu::AccOdr::ACC_ODR_104:  acc_hz = 104; break;
        case imu::AccOdr::ACC_ODR_208:  acc_hz = 208; break;
        case imu::AccOdr::ACC_ODR_416:  acc_hz = 416; break;
        case imu::AccOdr::ACC_ODR_833:  acc_hz = 833; break;
        default:  acc_hz = 833; break;
    }

    float gyro_hz = 104.0f;
    switch (gyro_odr) {
        case imu::GyroOdr::GYRO_ODR_12_5: gyro_hz = 12.5f; break;
        case imu::GyroOdr::GYRO_ODR_26:   gyro_hz = 26; break;
        case imu::GyroOdr::GYRO_ODR_52:   gyro_hz = 52; break;
        case imu::GyroOdr::GYRO_ODR_104:  gyro_hz = 104; break;
        case imu::GyroOdr::GYRO_ODR_208:  gyro_hz = 208; break;
        case imu::GyroOdr::GYRO_ODR_416:  gyro_hz = 416; break;
        case imu::GyroOdr::GYRO_ODR_833:  gyro_hz = 833; break;
        default:  gyro_hz = 833; break;
    }

    auto init_task = [dev, acc_g, acc_hz, gyro_dps, gyro_hz]() -> std::pair<int, std::string> {
        if (!dev->check_device()) {
            maix::log::error("[%s] Invalid WHO_AM_I: 0x%02x", TAG, dev->device_id);
            return {-1, "Device not found"};
        }

        dev->set_acc_scale(acc_g);
        dev->set_acc_odr(acc_hz);
        dev->set_gyro_scale(gyro_dps);
        dev->set_gyro_odr(gyro_hz);
        dev->is_open = true;

        maix::log::info("[%s] Open IMU Succ. Device ID: 0x%02x", TAG, dev->device_id);
        return {0, ""};
    };

    this->open_future = std::async(std::launch::async, init_task);
    this->open_fut_need_get = true;

    if (block) {
        maix::log::info("[%s] Waiting for IMU open...", TAG);
        auto ret = this->open_future.get();
        this->open_fut_need_get = false;
        if (ret.first < 0) {
            maix::log::error("[%s] Failed: %s", TAG, ret.second.c_str());
        }
    }
}

LSM6DSOWTR::~LSM6DSOWTR() {
    if (this->open_fut_need_get) {
        this->open_future.wait();
        this->open_fut_need_get = false;
    }
    delete static_cast<Lsm6dsowI2C*>(this->_data);
}

std::vector<float> LSM6DSOWTR::read() {
    auto dev = static_cast<Lsm6dsowI2C*>(this->_data);
    if (!dev->is_open) {
        return {};
    }

    lsm6dsow_data_t data;
    if (!dev->read_data(data)) {
        // No new data, return last or empty? Here we return zeros with temp=0
        // In real use, you may cache last valid frame
        data = {};
    }

    if (_mode == imu::Mode::DUAL) {
        return {data.acc_xyz.x, data.acc_xyz.y, data.acc_xyz.z,
                data.gyro_xyz.x, data.gyro_xyz.y, data.gyro_xyz.z,
                data.temperature};
    } else if (_mode == imu::Mode::ACC_ONLY) {
        return {data.acc_xyz.x, data.acc_xyz.y, data.acc_xyz.z, data.temperature};
    } else if (_mode == imu::Mode::GYRO_ONLY) {
        return {data.gyro_xyz.x, data.gyro_xyz.y, data.gyro_xyz.z, data.temperature};
    }
    return {};
}
}