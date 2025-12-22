/**
 * @author lxowalle@sipeed
 * @copyright Sipeed Ltd 2023-
 * @license Apache 2.0
 * @update 2023.9.8: Add framework, create this file.
 */

#include <stdint.h>
#include "maix_basic.hpp"
#include "maix_err.hpp"
#include "maix_audio.hpp"
#include <sys/wait.h>
#include "ax_middleware.hpp"
#include <condition_variable>
#include <list>
#include <queue>
#include <mutex>
#include <thread>

using namespace maix;
using namespace maix::middleware;

namespace maix::audio
{
    typedef struct {
        int file_size;  // pcm + 44
        int channel;
        int sample_rate;
        int sample_bit;
        int bitrate;
        int data_size;  // size of pcm
    } wav_header_t;

    static int _create_wav_header(wav_header_t *header, uint8_t *data, size_t size)
    {
        if (size < 44) return -1;

        int cnt = 0;
        data[cnt ++] = 'R';
        data[cnt ++] = 'I';
        data[cnt ++] = 'F';
        data[cnt ++] = 'F';

        data[cnt ++] = (uint8_t)((header->file_size - 8) & 0xff);
        data[cnt ++] = (uint8_t)(((header->file_size - 8) >> 8) & 0xff);
        data[cnt ++] = (uint8_t)(((header->file_size - 8) >> 16) & 0xff);
        data[cnt ++] = (uint8_t)(((header->file_size - 8) >> 24) & 0xff);

        data[cnt ++] = 'W';
        data[cnt ++] = 'A';
        data[cnt ++] = 'V';
        data[cnt ++] = 'E';

        data[cnt ++] = 'f';
        data[cnt ++] = 'm';
        data[cnt ++] = 't';
        data[cnt ++] = ' ';

        data[cnt ++] = 16;
        data[cnt ++] = 0;
        data[cnt ++] = 0;
        data[cnt ++] = 0;

        data[cnt ++] = 1;
        data[cnt ++] = 0;

        data[cnt ++] = (uint8_t)header->channel;
        data[cnt ++] = 0;

        data[cnt ++] = (uint8_t)((header->sample_rate) & 0xff);
        data[cnt ++] = (uint8_t)(((header->sample_rate) >> 8) & 0xff);
        data[cnt ++] = (uint8_t)(((header->sample_rate) >> 16) & 0xff);
        data[cnt ++] = (uint8_t)(((header->sample_rate) >> 24) & 0xff);

        data[cnt ++] = (uint8_t)((header->bitrate) & 0xff);
        data[cnt ++] = (uint8_t)(((header->bitrate) >> 8) & 0xff);
        data[cnt ++] = (uint8_t)(((header->bitrate) >> 16) & 0xff);
        data[cnt ++] = (uint8_t)(((header->bitrate) >> 24) & 0xff);

        data[cnt ++] = (uint8_t)(header->channel * header->sample_bit / 8);
        data[cnt ++] = 0;

        data[cnt ++] = (uint8_t)header->sample_bit;
        data[cnt ++] = 0;

        data[cnt ++] = 'd';
        data[cnt ++] = 'a';
        data[cnt ++] = 't';
        data[cnt ++] = 'a';

        data[cnt ++] = (uint8_t)((header->data_size) & 0xff);
        data[cnt ++] = (uint8_t)(((header->data_size) >> 8) & 0xff);
        data[cnt ++] = (uint8_t)(((header->data_size) >> 16) & 0xff);
        data[cnt ++] = (uint8_t)(((header->data_size) >> 24) & 0xff);

        return 0;
    }

    static int pcm_frame_to_bytes(int channels, int bits, int count) {
        return count * channels * bits / 8;
    }
    class RingBuffer {
    public:
        explicit RingBuffer(size_t capacity)
            : buffer(capacity), capacity(capacity), head(0), tail(0), size(0), exitFlag(false) {}

        // 写入数据
        void write(const uint8_t* data, size_t len) {
            size_t written = 0;
            while (written < len) {
                std::unique_lock<std::mutex> lock(mtx);
                not_full.wait(lock, [this] { return size < capacity || exitFlag;});
                if (exitFlag) break;
                size_t space = capacity - size;
                size_t chunk = std::min(len - written, space);
                for (size_t i = 0; i < chunk; i++) {
                    buffer[tail] = data[written + i];
                    tail = (tail + 1) % capacity;
                }
                size += chunk;
                written += chunk;

                lock.unlock();
                not_empty.notify_one();
            }
        }

        // 读取数据
        size_t read(uint8_t* out, size_t len) {
            size_t readCount = 0;
            while (readCount < len) {
                std::unique_lock<std::mutex> lock(mtx);
                // 阻塞直到有数据或退出
                not_empty.wait(lock, [this] { return size > 0 || exitFlag;});

                if (exitFlag && size == 0) break;

                size_t available = size;
                size_t chunk = std::min(len - readCount, available);

                for (size_t i = 0; i < chunk; i++) {
                    out[readCount + i] = buffer[head];
                    head = (head + 1) % capacity;
                }
                size -= chunk;
                readCount += chunk;

                lock.unlock();
                not_full.notify_one();
            }
            return readCount;
        }

        // 清空缓冲区
        void clear() {
            std::lock_guard<std::mutex> lock(mtx);
            head = tail = size = 0;
            // 通知所有阻塞线程缓冲区已清空
            not_full.notify_all();
            not_empty.notify_all();
        }

        // 通知退出
        void setExit() {
            std::lock_guard<std::mutex> lock(mtx);
            exitFlag = true;
            not_empty.notify_all();
            not_full.notify_all();
        }

        bool isExit() {
            std::lock_guard<std::mutex> lock(mtx);
            return exitFlag;
        }

        bool empty() {
            std::lock_guard<std::mutex> lock(mtx);
            return size == 0;
        }

        void reset(size_t newCapacity = 0) {
            std::lock_guard<std::mutex> lock(mtx);
            head = tail = size = 0;
            exitFlag = false;

            if (newCapacity > 0 && newCapacity != capacity) {
                buffer.resize(newCapacity);
                capacity = newCapacity;
            }

            not_empty.notify_all();
            not_full.notify_all();
        }

        size_t available() {
            std::lock_guard<std::mutex> lock(mtx);
            return size;
        }

        size_t idle_size() {
            std::lock_guard<std::mutex> lock(mtx);
            return capacity - size;
        }
    private:
        std::vector<uint8_t> buffer;
        size_t capacity;
        size_t head, tail, size;
        bool exitFlag;

        std::mutex mtx;
        std::condition_variable not_empty, not_full;
    };

    typedef struct {
        uint32_t card;
        uint32_t device;
        std::string path = "";
        struct pcm *pcm = nullptr;
        FILE *file = nullptr;
        wav_header_t wav_header;
        int channels;
        int ax_channels;
        int rate;
        int bits;
        bool block;
        audio::Format format;
        maixcam2::SYS *ax_sys;
        union {
            maixcam2::AudioIn *ax_ai;
            maixcam2::AudioOut *ax_ao;
        };
        std::list<Bytes *> remaining_pcm_list;
        std::unique_ptr<audio::File> wav_reader;
        AX_AUDIO_BIT_WIDTH_E ax_bit_width;
        AX_AUDIO_SOUND_MODE_E ax_sound_mode;

        bool thread_exit_flag;
        std::unique_ptr<std::thread> thread_handle;
        std::unique_ptr<RingBuffer> ring_buffer;
        size_t thread_chunk_size;

        int user_period_size;
        int user_period_count;

        double volume_scale;
    } audio_param_t;

    static AX_AUDIO_BIT_WIDTH_E __maix_to_ax_fmt(audio::Format format) {
        switch (format) {
        case FMT_S8:
            return AX_AUDIO_BIT_WIDTH_8;
        case FMT_S16_LE:
            return AX_AUDIO_BIT_WIDTH_16;
        case FMT_S32_LE:
            return AX_AUDIO_BIT_WIDTH_32;
        case FMT_S16_BE:
            return AX_AUDIO_BIT_WIDTH_16;
        case FMT_S32_BE:
            return AX_AUDIO_BIT_WIDTH_32;
        default:
            std::string error_msg = "not support audio format(" + std::to_string(format) + ")";
            err::check_raise(err::ERR_ARGS, error_msg);
        }

        return AX_AUDIO_BIT_WIDTH_BUTT;
    }

    static audio::Format __ax_to_maix_fmt(AX_AUDIO_BIT_WIDTH_E format) {
        switch (format) {
        case AX_AUDIO_BIT_WIDTH_8:
            return FMT_S8;
        case AX_AUDIO_BIT_WIDTH_16:
            return FMT_S16_LE;
        case AX_AUDIO_BIT_WIDTH_32:
            return FMT_S32_LE;
        default:
            std::string error_msg = "not support audio format(" + std::to_string(format) + ")";
            err::check_raise(err::ERR_ARGS, error_msg);
        }

        return FMT_NONE;
    }

    static uint64_t __frames_to_bytes(audio_param_t *param, int frame_count) {
        return frame_count * param->bits / 8 * param->channels;
    }

    Recorder::Recorder(std::string path, int sample_rate, audio::Format format, int channel, bool block) {
        if (path.size() > 0) {
            if (fs::splitext(path)[1] != ".wav"
                && fs::splitext(path)[1] != ".pcm") {
                err::check_raise(err::ERR_RUNTIME, "Only files with the `.pcm` and `.wav` extensions are supported.");
            }
        }
        err::Err ret = err::ERR_NONE;
        audio_param_t *param = new audio_param_t();
        err::check_null_raise(param, "malloc failed");
        auto ax_bit_width = __maix_to_ax_fmt(format);
        param->format = format;
        param->channels = channel;
        param->ax_channels = 2;
        param->rate = sample_rate;
        param->bits = (ax_bit_width + 1) * 8;
        param->block = block;
        param->path = path;
        param->file = nullptr;
        err::check_bool_raise(channel == 1 || channel == 2, "Only support channel setting 1 or 2");

        maixcam2::ax_audio_in_param_t audio_in_param = {
            .channels = param->ax_channels,
            .rate = sample_rate,
            .bits = ax_bit_width,
            .layout = AX_AI_MIC_MIC,
            .period_size = 160,
            .period_count = 8,
        };

        param->ax_sys = new maixcam2::SYS();
        if (!param->ax_sys) {
            delete param;
            err::check_raise(err::ERR_RUNTIME, "constructing SYS failed");
        }

        ret = param->ax_sys->init();
        if (ret != err::ERR_NONE) {
            delete param->ax_sys;
            param->ax_sys = nullptr;
            delete param;
            err::check_raise(ret, "initializing SYS failed");
        }

        param->ax_ai = new maixcam2::AudioIn(&audio_in_param);
        if (!param->ax_ai) {
            param->ax_sys->deinit();
            delete param->ax_sys;
            param->ax_sys = nullptr;
            delete param;
            err::check_raise(err::ERR_RUNTIME, "constructing AudioIn failed");
        }

        ret = param->ax_ai->init();
        if (ret != err::ERR_NONE) {
            delete param->ax_ai;
            param->ax_ai = nullptr;
            param->ax_sys->deinit();
            delete param->ax_sys;
            param->ax_sys = nullptr;
            delete param;
            err::check_raise(ret, "initializing AudioIn failed");
        }

        param->volume_scale = atof(app::get_sys_config_kv("audio", "input_volume_scale", "1.0").c_str());
        param->volume_scale = param->volume_scale > 1.0 ? 1.0 : param->volume_scale;
        param->volume_scale = param->volume_scale < 0.01 ? 0.01 : param->volume_scale;
        _param = param;

        int new_volume = atoi(app::get_sys_config_kv("audio", "input_volume", "-1").c_str());
        if (new_volume >= 0) {
            this->volume(new_volume);
        }
    }

    Recorder::~Recorder() {
        audio_param_t *param = (audio_param_t *)_param;
        if (param) {
            if (param->file) {
                this->finish();
            }

            if (param->ax_ai) {
                delete param->ax_ai;
                param->ax_ai = nullptr;
            }

            if (param->ax_sys) {
                delete param->ax_sys;
                param->ax_sys = nullptr;
            }

            delete param;
            _param = nullptr;
        }
    }

    bool Recorder::mute(int data) {
        err::check_raise(err::ERR_NOT_IMPL, "maixcam2 not supports this function");
        return false;
    }

    // value [0, 100], return [0, 10]
    static float __volume_int_to_float(int value) {
        value = value > 100 ? 100 : value;
        float f_value = 0;
        if (value < 50) {
            f_value = (float)value / 50;
        } else {
            f_value = ((float)(value - 50) / 50 * 9) + 1;
        }

        return f_value;
    }

    // value [0, 10], return [0, 100]
    static int __volume_float_to_int(float value) {
        int new_value = 0;
        if (value < 1) {
            new_value = value * 50;
        } else {
            new_value = ((value - 1) / 9 * 50) + 50;
        }

        return new_value;
    }

    int Recorder::volume(int value) {
        audio_param_t *param = (audio_param_t *)_param;
        value = value * param->volume_scale;

        if (param->ax_ai) {
            auto f_value = __volume_int_to_float(value);
            auto new_volume = param->ax_ai->volume(f_value);
            auto new_volume_int = __volume_float_to_int(new_volume);
            new_volume_int = (int)((double)new_volume_int / param->volume_scale);
            if (value >= 0) {
                app::set_sys_config_kv("audio", "input_volume", std::to_string(new_volume_int));
            }
            return new_volume_int;
        }
        return 0;
    }

    void Recorder::reset(bool start) {
        (void)start;
        audio_param_t *param = (audio_param_t *)_param;
        if (param->ax_ai) {
            param->ax_ai->reset();
        }

        for (auto it = param->remaining_pcm_list.begin(); it != param->remaining_pcm_list.end();) {
            auto pcm = *it;
            delete pcm;
            it = param->remaining_pcm_list.erase(it);
        }

        int new_volume = atoi(app::get_sys_config_kv("audio", "input_volume", "-1").c_str());
        if (new_volume >= 0) {
            this->volume(new_volume);
        }
    }

    maix::Bytes *Recorder::record(int record_ms) {
        audio_param_t *param = (audio_param_t *)_param;
        if (record_ms < 0) {
            return record_bytes(record_ms);
        } else {
            int frame_count = record_ms * param->rate / 1000;
            int bytes = frame_size(frame_count);
            return record_bytes(bytes);
        }
    }

    int Recorder::frame_size(int frame_count) {
        audio_param_t *param = (audio_param_t *)_param;
        frame_count = frame_count <= 0 ? 1 : frame_count;
        return __frames_to_bytes(param, frame_count);
    }

    int Recorder::get_remaining_frames() {
        return 0;
    }

    int Recorder::period_size(int period_size) {
        audio_param_t *param = (audio_param_t *)_param;
        return param->ax_ai->period_size(period_size);
    }

    int Recorder::period_count(int period_count) {
        audio_param_t *param = (audio_param_t *)_param;
        return param->ax_ai->period_count(period_count);
    }

    maix::Bytes *Recorder::record_bytes(int record_size) {
        audio_param_t *param = (audio_param_t *)_param;

        if (param->file == nullptr && param->path.size() > 0) {
            param->file = fopen(param->path.c_str(), "w+");
            err::check_null_raise(param->file, "Open file failed!");

            if (fs::splitext(param->path)[1] == ".wav") {
                wav_header_t header = {
                    .file_size = 44,
                    .channel = (int)param->channels,
                    .sample_rate = (int)param->rate,
                    .sample_bit = (int)param->bits,
                    .bitrate = (int)(param->channels * param->rate * param->bits / 8),
                    .data_size = 0,
                };

                uint8_t buffer[44];
                if (0 != _create_wav_header(&header, buffer, sizeof(buffer))) {
                    err::check_raise(err::ERR_RUNTIME, "create wav failed!");
                }

                if (sizeof(buffer) != fwrite(buffer, 1, sizeof(buffer), param->file)) {
                    err::check_raise(err::ERR_RUNTIME, "write wav header failed!");
                }
            }
        }

        Bytes *out_bytes = nullptr;
        if (record_size < 0) {
            std::list<Bytes*> pcm_list;
            int total_pcm_size = 0;
            for (auto it = param->remaining_pcm_list.begin(); it != param->remaining_pcm_list.end();) {
                auto pcm = *it;
                pcm_list.push_back(pcm);
                total_pcm_size = pcm->data_len;
                it = param->remaining_pcm_list.erase(it);
            }

            while (!app::need_exit()) {
                auto audio_frame = param->ax_ai->read(0);
                if (audio_frame) {
                    Bytes *pcm = nullptr;
                    if (param->channels == 1) {
                        if (param->bits == 16) {
                            pcm = new Bytes(nullptr, audio_frame->len / 2);
                            int16_t *src = (int16_t *)audio_frame->data;
                            int16_t *dst = (int16_t *)pcm->data;
                            int j = 0;
                            for (int i = 1; i < audio_frame->len / this->frame_size(); i += param->ax_channels) {
                                dst[j ++] = src[i];
                            }
                        } else {
                            err::check_raise(err::ERR_NOT_IMPL, "Can't invert channel to 1");
                        }
                    } else {
                        pcm = new Bytes((uint8_t *)audio_frame->data, audio_frame->len);
                    }

                    pcm_list.push_back(pcm);
                    total_pcm_size += pcm->data_len;
                    delete audio_frame;
                } else {
                    break;
                }
            }

            out_bytes = new Bytes(nullptr, total_pcm_size);
            if (!out_bytes) {
                return new Bytes();
            }

            int offset = 0;
            for (auto it = pcm_list.begin(); it != pcm_list.end(); ++it) {
                auto pcm = *it;
                memcpy(out_bytes->data + offset, pcm->data, pcm->data_len);
                offset += pcm->data_len;
                delete pcm;
            }
        } else {
            std::list<Bytes*> pcm_list;
            int total_pcm_size = 0;
            for (auto it = param->remaining_pcm_list.begin(); it != param->remaining_pcm_list.end();) {
                auto pcm = *it;
                pcm_list.push_back(pcm);
                total_pcm_size = pcm->data_len;
                it = param->remaining_pcm_list.erase(it);
            }

            while (!app::need_exit()) {
                if (total_pcm_size >= record_size) {
                    break;
                }

                auto audio_frame = param->ax_ai->read(param->block ? -1 : 0);
                if (audio_frame) {
                    Bytes *pcm = nullptr;
                    if (param->channels == 1) {
                        if (param->bits == 16) {
                            pcm = new Bytes(nullptr, audio_frame->len / 2);
                            int16_t *src = (int16_t *)audio_frame->data;
                            int16_t *dst = (int16_t *)pcm->data;
                            int j = 0;
                            for (int i = 1; i < audio_frame->len / this->frame_size(); i += param->ax_channels) {
                                dst[j ++] = src[i];
                            }
                        } else {
                            err::check_raise(err::ERR_NOT_IMPL, "Can't invert channel to 1");
                        }
                    } else {
                        pcm = new Bytes((uint8_t *)audio_frame->data, audio_frame->len);
                    }

                    pcm_list.push_back(pcm);
                    total_pcm_size += pcm->data_len;
                    delete audio_frame;
                } else {
                    break;
                }
            }

            if (total_pcm_size >= record_size) {
                out_bytes = new Bytes(nullptr, record_size);
                if (!out_bytes) {
                    return new Bytes();
                }
                int offset = 0;
                Bytes *remaining_pcm = nullptr;
                for (auto it = pcm_list.begin(); it != pcm_list.end();) {
                    auto pcm = *it;
                    if ((int)(pcm->data_len + offset) > record_size) {
                        int copy_size = record_size - offset;
                        memcpy(out_bytes->data + offset, pcm->data, copy_size);
                        remaining_pcm = new Bytes((uint8_t *)pcm->data + copy_size, pcm->data_len - copy_size);
                        offset += (pcm->data_len - copy_size);
                    } else {
                        memcpy(out_bytes->data + offset, pcm->data, pcm->data_len);
                        offset += pcm->data_len;
                    }

                    delete pcm;
                    it = pcm_list.erase(it);
                    if (offset >= record_size) {
                        break;
                    }
                }

                if (remaining_pcm) {
                    param->remaining_pcm_list.push_back(remaining_pcm);
                }

                for (auto pcm: pcm_list) {
                    param->remaining_pcm_list.push_back(pcm);
                }
            } else {
                out_bytes = new Bytes();
                for (auto pcm: pcm_list) {
                    param->remaining_pcm_list.push_back(pcm);
                }
            }
        }

        if (param->file)
            fwrite(out_bytes->data, 1, out_bytes->data_len, param->file);

        return out_bytes;
    }

    err::Err Recorder::finish() {
        audio_param_t *param = (audio_param_t *)_param;
        if (param->file) {
            if (fs::splitext(param->path)[1] == ".wav") {
                int file_size = ftell(param->file);
                int pcm_size = file_size - 44;
                char buffer[4];
                buffer[0] = (uint8_t)((file_size - 8) & 0xff);
                buffer[1] = (uint8_t)(((file_size - 8) >> 8) & 0xff);
                buffer[2] = (uint8_t)(((file_size - 8) >> 16) & 0xff);
                buffer[3] = (uint8_t)(((file_size - 8) >> 24) & 0xff);

                fseek(param->file, 4, 0);
                if (sizeof(buffer) != fwrite(buffer, 1, sizeof(buffer), param->file)) {
                    err::check_raise(err::ERR_RUNTIME, "write wav file size failed!");
                }

                buffer[0] = (uint8_t)((pcm_size) & 0xff);
                buffer[1] = (uint8_t)(((pcm_size) >> 8) & 0xff);
                buffer[2] = (uint8_t)(((pcm_size) >> 16) & 0xff);
                buffer[3] = (uint8_t)(((pcm_size) >> 24) & 0xff);
                fseek(param->file, 40, 0);
                if (sizeof(buffer) != fwrite(buffer, 1, sizeof(buffer), param->file)) {
                    err::check_raise(err::ERR_RUNTIME, "write wav data size failed!");
                }
            }

            fflush(param->file);
            fclose(param->file);
            param->file = NULL;
        }

        return err::ERR_NONE;
    }

    int Recorder::sample_rate() {
        audio_param_t *param = (audio_param_t *)_param;

        return param->rate;
    }

    audio::Format Recorder::format() {
        audio_param_t *param = (audio_param_t *)_param;
        return param->format;
    }

    int Recorder::channel() {
        audio_param_t *param = (audio_param_t *)_param;
        return param->channels;
    }
#if CONFIG_BUILD_WITH_MAIXPY
    maix::Bytes *Player::NoneBytes = new maix::Bytes();
#else
    maix::Bytes *Player::NoneBytes = NULL;
#endif

    static void player_thread(audio_param_t *param) {
        auto period_size = param->user_period_size;
        auto chunk_size = period_size * param->channels * param->bits / 8;
        std::vector<uint8_t> buffer(chunk_size);
        while (!app::need_exit() && !param->thread_exit_flag) {
            auto size = param->ring_buffer->read(buffer.data(), buffer.size());
            if (size != buffer.size()) {
                time::sleep_ms(10);
                continue;
            }

            maixcam2::Frame frame = maixcam2::Frame(param->card, param->device,
                buffer.data(), buffer.size(), param->ax_bit_width, param->ax_sound_mode);
            auto ret = param->ax_ao->write(&frame);
            if (ret != err::ERR_NONE) {
                log::warn("ao write failed");
            }
        }
        param->ring_buffer->setExit();
    }

    Player::Player(std::string path, int sample_rate, audio::Format format, int channel, bool block) {
        if (path.size() > 0) {
            if (fs::splitext(path)[1] != ".wav"
                && fs::splitext(path)[1] != ".pcm") {
                err::check_raise(err::ERR_RUNTIME, "Only files with the `.pcm` and `.wav` extensions are supported.");
            }
        }

        err::Err ret = err::ERR_NONE;
        audio_param_t *param = new audio_param_t();
        err::check_null_raise(param, "malloc failed");
        AX_AUDIO_BIT_WIDTH_E ax_bit_width = __maix_to_ax_fmt(format);
        if (path.size() > 0) {
            param->wav_reader = std::make_unique<audio::File>(sample_rate, channel, fmt_bits[format]);
            param->wav_reader->load(path);
            sample_rate = param->wav_reader->sample_rate();
            channel = param->wav_reader->channels();
            ax_bit_width = (AX_AUDIO_BIT_WIDTH_E)(param->wav_reader->sample_bits() / 8 - 1);
            format = __ax_to_maix_fmt(ax_bit_width);
        }

        param->path = path;
        param->card = 1;
        param->device = 0;
        param->channels = channel;
        param->ax_channels = channel;
        param->format = format;
        param->rate = sample_rate;
        param->ax_bit_width = ax_bit_width;
        param->block = block;
        param->bits = (ax_bit_width + 1) * 8;
        param->ax_sound_mode = param->channels == 1 ? AX_AUDIO_SOUND_MODE_MONO : AX_AUDIO_SOUND_MODE_STEREO;
        param->user_period_size = 160;
        param->user_period_count = 30;
        maixcam2::ax_audio_out_param_t audio_out_param = {
            .channels = param->ax_channels,
            .rate = sample_rate,
            .bits = ax_bit_width,
            .period_size = 160,
            .period_count = 30,
            .insert_silence = true,
        };

        param->ax_sys = new maixcam2::SYS();
        if (!param->ax_sys) {
            delete param;
            err::check_raise(err::ERR_RUNTIME, "constructing SYS failed");
        }

        ret = param->ax_sys->init();
        if (ret != err::ERR_NONE) {
            delete param->ax_sys;
            param->ax_sys = nullptr;
            delete param;
            err::check_raise(ret, "initializing SYS failed");
        }

        param->ax_ao = new maixcam2::AudioOut(&audio_out_param);
        if (!param->ax_ao) {
            param->ax_sys->deinit();
            delete param->ax_sys;
            param->ax_sys = nullptr;
            delete param;
            err::check_raise(err::ERR_RUNTIME, "constructing AudioOut failed");
        }

        ret = param->ax_ao->init();
        if (ret != err::ERR_NONE) {
            delete param->ax_ao;
            param->ax_ao = nullptr;
            param->ax_sys->deinit();
            delete param->ax_sys;
            param->ax_sys = nullptr;
            delete param;
            err::check_raise(ret, "initializing AudioOut failed");
        }

        param->thread_exit_flag = false;

        auto period_size = param->ax_ao->period_size();
        auto period_count = param->ax_ao->period_count();
        auto ringbuffer_size = pcm_frame_to_bytes(param->channels, param->bits, period_size)* period_count;
        param->ring_buffer = std::make_unique<RingBuffer>(ringbuffer_size);
        param->thread_handle = std::make_unique<std::thread>(player_thread, param);
        param->volume_scale = atof(app::get_sys_config_kv("audio", "output_volume_scale", "1.0").c_str());
        param->volume_scale = param->volume_scale > 1.0 ? 1.0 : param->volume_scale;
        param->volume_scale = param->volume_scale < 0.01 ? 0.01 : param->volume_scale;
        _param = param;

        int new_volume = atoi(app::get_sys_config_kv("audio", "output_volume", "-1").c_str());
        if (new_volume >= 0) {
            this->volume(new_volume);
        }
    }

    Player::~Player() {
        audio_param_t *param = (audio_param_t *)_param;
        if (param) {

            if (param->thread_handle) {
                param->thread_exit_flag = true;
                param->ring_buffer->setExit();
                param->thread_handle->join();
                param->thread_handle = nullptr;
                param->ring_buffer = nullptr;
            }

            if (param->ax_ao) {
                delete param->ax_ao;
                param->ax_ao = nullptr;
            }

            if (param->ax_sys) {
                delete param->ax_sys;
                param->ax_sys = nullptr;
            }

            if (param->file) {
                fclose(param->file);
                param->file = nullptr;
            }

            delete param;
            _param = nullptr;
        }
    }

    int Player::volume(int value) {
        audio_param_t *param = (audio_param_t *)_param;
        value = value * param->volume_scale;
        if (param->ax_ao) {
            auto f_value = __volume_int_to_float(value);
            auto new_volume = param->ax_ao->volume(f_value);
            auto new_volume_int = __volume_float_to_int(new_volume);
            new_volume_int = (int)((double)new_volume_int / param->volume_scale);
            if (value >= 0) {
                app::set_sys_config_kv("audio", "output_volume", std::to_string(new_volume_int));
            }
            return new_volume_int;
        }
        return 0;
    }

    err::Err Player::play(maix::Bytes *data) {
        audio_param_t *param = (audio_param_t *)_param;
        err::Err ret = err::ERR_NONE;
        if (!param->ring_buffer || param->ring_buffer->isExit()) {
            return err::ERR_NOT_READY;
        }

        if (!data || !data->data || !data->size()) {
            if (param->path.size() > 0 && fs::exists(param->path)) {
                auto pcm = param->wav_reader->get_pcm();
                param->ring_buffer->write(pcm->data, pcm->data_len);
                delete pcm;
            }
        } else {
            param->ring_buffer->write(data->data, data->data_len);
        }

        if (param->block) {
            while (!app::need_exit()) {
                if (param->ring_buffer->empty()) {
                    break;
                }
                time::sleep_ms(10);
            }
        }

        return ret;
    }

    int Player::frame_size(int frame_count) {
        audio_param_t *param = (audio_param_t *)_param;
        return pcm_frame_to_bytes(param->channels, param->bits, frame_count);
    }

    int Player::get_remaining_frames() {
        audio_param_t *param = (audio_param_t *)_param;
        auto avail = param->ring_buffer->idle_size() / frame_size();
        return avail;
    }

    int Player::period_size(int period_size) {
        audio_param_t *param = (audio_param_t *)_param;
        if (period_size >= 0 && period_size != param->user_period_size) {
            param->user_period_size = period_size;
            reset();
        }

        return param->user_period_size;
    }

    int Player::period_count(int period_count) {
        audio_param_t *param = (audio_param_t *)_param;
        if (period_count >= 0 && period_count != param->user_period_count) {
            param->user_period_count = period_count;
            reset();
        }

        return param->user_period_count;
    }

    void Player::reset(bool start) {
        (void)start;
        audio_param_t *param = (audio_param_t *)_param;
        param->ax_ao->clear();

        param->thread_exit_flag = true;
        param->ring_buffer->setExit();
        param->thread_handle->join();
        auto ringbuffer_size = frame_size(param->user_period_size) * param->user_period_count;
        param->ring_buffer->reset(ringbuffer_size);
        param->thread_exit_flag = false;
        param->thread_handle = std::make_unique<std::thread>(player_thread, param);
    }

    int Player::sample_rate() {
        audio_param_t *param = (audio_param_t *)_param;
        return param->rate;
    }

    audio::Format Player::format() {
        audio_param_t *param = (audio_param_t *)_param;
        return param->format;
    }

    int Player::channel() {
        audio_param_t *param = (audio_param_t *)_param;
        return param->channels;
    }
} // namespace maix::audio