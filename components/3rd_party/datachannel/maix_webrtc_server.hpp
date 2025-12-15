#pragma once

#include "rtc/rtc.hpp"
#include "nlohmann/json.hpp"
#include "maix_webrtc_signaling_server.hpp"
#include "maix_webrtc_http_server.hpp"
#include <string>
#include <memory>
#include <iostream>

#ifdef NDEBUG
#define MAIX_WEBRTC_LOG(...)
#else
#define MAIX_WEBRTC_LOG(...) printf(__VA_ARGS__)
#endif

#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <sstream>
#include <vector>

using json = nlohmann::json;

enum class VideoCodec {
    H264,
    H265
};

class MaixWebRTCServerBuilder;

class MaixWebRTCServer
{
    friend class MaixWebRTCServerBuilder;

private:
    int *clients;
    std::shared_ptr<rtc::PeerConnection> _peer_connection;
    rtc::Configuration _peer_config;
    std::shared_ptr<rtc::DataChannel> _data_channel;
    std::shared_ptr<rtc::Track> _video_track;
    std::shared_ptr<rtc::Track> _audio_track;
    std::shared_ptr<rtc::RtpPacketizationConfig> _video_rtp_config;
    bool _has_audio;
    VideoCodec _video_codec;

    std::mutex _frame_mutex;
    std::condition_variable _frame_cv;

    bool _is_running;
    std::thread *_send_thread;

    // Signaling WebSocket
    std::shared_ptr<rtc::WebSocket> _signaling_ws;
    std::string _client_id;
    bool _offer_sent;
    std::function<void()> _on_connected;
    std::function<void()> _on_disconnected;

    // Built-in signaling server
    std::shared_ptr<MaixWebRTCSignalingServer> _builtin_signaling_server;
    bool _use_builtin_signaling;

    // Built-in HTTP server
    std::shared_ptr<MaixWebRTCHttpServer> _http_server;
    bool _use_http_server;

public:
    MaixWebRTCServer(int *clients, std::shared_ptr<rtc::PeerConnection> peer_connection, bool has_audio, VideoCodec video_codec = VideoCodec::H264)
        : clients(clients), _peer_connection(peer_connection), _has_audio(has_audio), _video_codec(video_codec), _is_running(false), _send_thread(nullptr),
          _client_id("server"), _offer_sent(false), _use_builtin_signaling(false), _use_http_server(false)
    {
        if (clients != nullptr) {
            *clients = 0;
        }

        _peer_connection->onStateChange([this](rtc::PeerConnection::State state) {
            MAIX_WEBRTC_LOG("PeerConnection state: %d\n", (int)state);
        });

        _peer_connection->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
            MAIX_WEBRTC_LOG("PeerConnection gathering state: %d\n", (int)state);
        });

        _peer_connection->onLocalCandidate([this](rtc::Candidate candidate) {
            std::string cand_str = candidate.candidate();
            if (cand_str.find("::") != std::string::npos ||
                (cand_str.find(':') != std::string::npos &&
                 std::count(cand_str.begin(), cand_str.end(), ':') > 2)) {
                MAIX_WEBRTC_LOG("Filtered IPv6 candidate: %s\n", cand_str.c_str());
                return;
            }
            MAIX_WEBRTC_LOG("New local candidate: %s\n", cand_str.c_str());
        });

        _peer_connection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
            MAIX_WEBRTC_LOG("New data channel: %s\n", dc->label().c_str());
            _data_channel = dc;

            dc->onOpen([this]() {
                MAIX_WEBRTC_LOG("Data channel opened\n");
            });

            dc->onClosed([this]() {
                MAIX_WEBRTC_LOG("Data channel closed\n");
            });

            dc->onMessage([this](rtc::message_variant data) {
                if (std::holds_alternative<rtc::binary>(data)) {
                    MAIX_WEBRTC_LOG("Received binary message\n");
                } else {
                    MAIX_WEBRTC_LOG("Received string message: %s\n", std::get<std::string>(data).c_str());
                }
            });
        });
    }

    ~MaixWebRTCServer() {
        stop();

        if (_http_server) {
            _http_server->stop();
        }

        if (_builtin_signaling_server) {
            _builtin_signaling_server->stop();
        }

        if (_signaling_ws && _signaling_ws->isOpen()) {
            _signaling_ws->close();
        }

        if (_peer_connection) {
            _peer_connection->close();
        }

        if (clients != nullptr) {
            free(clients);
            clients = nullptr;
        }
    }

    int get_clients() {
        int value = 0;
        if (clients != nullptr) {
            value = *clients;
        }
        return value;
    }

    void reset_peer_connection() {
        MAIX_WEBRTC_LOG("[WebRTC] Resetting PeerConnection...\n");

        if (_peer_connection) {
            try {
                _peer_connection->close();
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            } catch (const std::exception &e) {
                MAIX_WEBRTC_LOG("[WebRTC] Error closing old PeerConnection: %s\n", e.what());
            }
        }

        _video_track.reset();
        _audio_track.reset();
        _data_channel.reset();
        _video_rtp_config.reset();

        try {
            _peer_connection = std::make_shared<rtc::PeerConnection>(_peer_config);

            _peer_connection->onStateChange([this](rtc::PeerConnection::State state) {
                MAIX_WEBRTC_LOG("PeerConnection state: %d\n", (int)state);
            });

            _peer_connection->onGatheringStateChange([this](rtc::PeerConnection::GatheringState state) {
                MAIX_WEBRTC_LOG("PeerConnection gathering state: %d\n", (int)state);
            });

            _peer_connection->onLocalCandidate([this](rtc::Candidate candidate) {
                std::string cand_str = candidate.candidate();
                if (cand_str.find("::") != std::string::npos ||
                    (cand_str.find(':') != std::string::npos &&
                     std::count(cand_str.begin(), cand_str.end(), ':') > 2)) {
                    MAIX_WEBRTC_LOG("Filtered IPv6 candidate: %s\n", cand_str.c_str());
                    return;
                }
                MAIX_WEBRTC_LOG("New local candidate: %s\n", cand_str.c_str());
            });

            _peer_connection->onDataChannel([this](std::shared_ptr<rtc::DataChannel> dc) {
                MAIX_WEBRTC_LOG("New data channel: %s\n", dc->label().c_str());
                _data_channel = dc;

                dc->onOpen([this]() {
                    MAIX_WEBRTC_LOG("Data channel opened\n");
                });

                dc->onClosed([this]() {
                    MAIX_WEBRTC_LOG("Data channel closed\n");
                });

                dc->onMessage([this](rtc::message_variant data) {
                    if (std::holds_alternative<rtc::binary>(data)) {
                        auto bin = std::get<rtc::binary>(data);
                        MAIX_WEBRTC_LOG("Data channel received binary data: %zu bytes\n", bin.size());
                    } else {
                        auto str = std::get<std::string>(data);
                        MAIX_WEBRTC_LOG("Data channel received: %s\n", str.c_str());
                    }
                });
            });

            add_video_track();
            if (_has_audio) {
                add_audio_track();
            }

            try {
                _peer_connection->setLocalDescription();
            } catch (const std::exception &e) {
                MAIX_WEBRTC_LOG("[WebRTC] Error setting local description after reset: %s\n", e.what());
            }

            MAIX_WEBRTC_LOG("[WebRTC] PeerConnection reset successfully\n");
        } catch (const std::exception &e) {
            MAIX_WEBRTC_LOG("[WebRTC] Failed to reset PeerConnection: %s\n", e.what());
        }
    }

    void add_video_track() {
        if (!_peer_connection) return;

        const rtc::SSRC ssrc = 1;
        auto video = rtc::Description::Video("video");

        if (_video_codec == VideoCodec::H265) {
            video.addH265Codec(96, "profile-id=1;level-id=153");
            video.addAttribute("rtcp-fb:96 nack");
            video.addAttribute("rtcp-fb:96 nack pli");
            video.addAttribute("rtcp-fb:96 goog-remb");
            video.addAttribute("extmap:1 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay");
            video.addSSRC(ssrc, "video-stream", "stream1", "video-stream");
            _video_track = _peer_connection->addTrack(video);

            if (_video_track) {
                _video_rtp_config = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, "video-stream", 96, rtc::H265RtpPacketizer::ClockRate);
                auto packetizer = std::make_shared<rtc::H265RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, _video_rtp_config);
                auto srReporter = std::make_shared<rtc::RtcpSrReporter>(_video_rtp_config);
                packetizer->addToChain(srReporter);

                auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
                packetizer->addToChain(nackResponder);

                _video_track->setMediaHandler(packetizer);

                MAIX_WEBRTC_LOG("Video track added: H.265/HEVC Main Profile, NACK/PLI/REMB enabled\n");
            }
        } else {
            video.addH264Codec(96, "level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=4D0034");
            video.addAttribute("rtcp-fb:96 nack");
            video.addAttribute("rtcp-fb:96 nack pli");
            video.addAttribute("rtcp-fb:96 goog-remb");
            video.addAttribute("extmap:1 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay");
            video.addSSRC(ssrc, "video-stream", "stream1", "video-stream");
            _video_track = _peer_connection->addTrack(video);

            if (_video_track) {
                _video_rtp_config = std::make_shared<rtc::RtpPacketizationConfig>(ssrc, "video-stream", 96, rtc::H264RtpPacketizer::ClockRate);
                auto packetizer = std::make_shared<rtc::H264RtpPacketizer>(rtc::NalUnit::Separator::StartSequence, _video_rtp_config);
                auto srReporter = std::make_shared<rtc::RtcpSrReporter>(_video_rtp_config);
                packetizer->addToChain(srReporter);

                auto nackResponder = std::make_shared<rtc::RtcpNackResponder>();
                packetizer->addToChain(nackResponder);

                _video_track->setMediaHandler(packetizer);

                MAIX_WEBRTC_LOG("Video track added: H.264 Main Profile (4D0034), packetization-mode=1, NACK/PLI/REMB enabled\n");
            }
        }
    }

    void add_audio_track() {
        if (!_peer_connection || !_has_audio) return;

        auto audio = rtc::Description::Audio("audio");
        audio.addOpusCodec(97);
        _audio_track = _peer_connection->addTrack(audio);
        if (_audio_track) {
            MAIX_WEBRTC_LOG("Audio track added\n");
        }
    }

    int video_frame_push(uint8_t *frame, size_t frame_size, uint64_t timestamp) {
        if (!_video_track || !_video_track->isOpen() || !frame || frame_size == 0) {
            return -1;
        }

        try {
            if (_video_rtp_config) {
                _video_rtp_config->timestamp = static_cast<uint32_t>(timestamp);
            }

            rtc::binary video_frame;
            video_frame.reserve(frame_size);
            for (size_t i = 0; i < frame_size; ++i) {
                video_frame.push_back(static_cast<std::byte>(frame[i]));
            }
            _video_track->send(video_frame);
        } catch (const std::exception &e) {
            MAIX_WEBRTC_LOG("Error sending video frame: %s\n", e.what());
            return -1;
        }

        return 0;
    }

    int audio_frame_push(uint8_t *frame, size_t frame_size, uint64_t timestamp) {
        if (!_audio_track || !_audio_track->isOpen() || !_has_audio || !frame || frame_size == 0) {
            return -1;
        }

        try {
            rtc::binary audio_frame;
            audio_frame.reserve(frame_size);
            for (size_t i = 0; i < frame_size; ++i) {
                audio_frame.push_back(static_cast<std::byte>(frame[i]));
            }
            _audio_track->send(audio_frame);
        } catch (const std::exception &e) {
            MAIX_WEBRTC_LOG("Error sending audio frame: %s\n", e.what());
            return -1;
        }

        return 0;
    }

    void start() {
        if (_is_running) return;

        _is_running = true;

        try {
            _peer_connection->setLocalDescription();
        } catch (const std::exception &e) {
            MAIX_WEBRTC_LOG("Error setting local description: %s\n", e.what());
        }
    }    void stop() {
        _is_running = false;
    }

    bool is_video_track_open() {
        return _video_track && _video_track->isOpen();
    }

    bool is_audio_track_open() {
        return _audio_track && _audio_track->isOpen();
    }

    std::string get_local_description() {
        if (!_peer_connection) return "";
        auto description = _peer_connection->localDescription();
        if (description) {
            std::string sdp = std::string(*description);
            std::istringstream iss(sdp);
            std::ostringstream oss;
            std::string line;

            while (std::getline(iss, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                if (line.find("a=candidate:") == 0) {
                    if (line.find("::") != std::string::npos ||
                        std::count(line.begin(), line.end(), ':') > 5) {
                        MAIX_WEBRTC_LOG("Filtered IPv6 from SDP: %s\n", line.c_str());
                        continue;
                    }
                }
                oss << line << "\r\n";
            }

            return oss.str();
        }
        return "";
    }

    bool connect_signaling(const std::string &ip, int port, const std::string &client_id = "server") {
        _client_id = client_id;

        if (ip.empty()) {
            MAIX_WEBRTC_LOG("[WebRTC] Starting built-in signaling server on 0.0.0.0:%d\n", port);
            _builtin_signaling_server = std::make_shared<MaixWebRTCSignalingServer>("0.0.0.0", port);

            _builtin_signaling_server->set_on_request([this](const std::string &client_id, const json &message) {
                MAIX_WEBRTC_LOG("[WebRTC] Received request from %s, sending offer\n", client_id.c_str());

                std::string local_desc;
                for (int i = 0; i < 10; i++) {
                    local_desc = get_local_description();
                    if (!local_desc.empty()) {
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(200));
                }

                if (!local_desc.empty()) {
                    json offer_message = {
                        {"id", client_id},
                        {"type", "offer"},
                        {"sdp", local_desc}
                    };
                    _builtin_signaling_server->send_to_client(client_id, offer_message);
                    MAIX_WEBRTC_LOG("[WebRTC] Sent offer to %s\n", client_id.c_str());
                } else {
                    MAIX_WEBRTC_LOG("[WebRTC] Local description is empty after waiting\n");
                }
            });

            _builtin_signaling_server->set_on_message([this](const std::string &client_id, const json &message) {
                std::string msg_type = message.value("type", "");
                MAIX_WEBRTC_LOG("[WebRTC] Received %s from %s\n", msg_type.c_str(), client_id.c_str());

                if (msg_type == "answer") {
                    std::string sdp = message.value("sdp", "");
                    if (!sdp.empty()) {
                        set_remote_description(sdp);
                        MAIX_WEBRTC_LOG("[WebRTC] Set remote description from answer\n");
                    }
                }
            });

            _builtin_signaling_server->set_on_client_disconnected([this](const std::string &client_id) {
                MAIX_WEBRTC_LOG("[WebRTC] Client %s disconnected, resetting connection...\n", client_id.c_str());
                reset_peer_connection();
            });

            if (!_builtin_signaling_server->start()) {
                MAIX_WEBRTC_LOG("[WebRTC] Failed to start built-in signaling server\n");
                return false;
            }
            _use_builtin_signaling = true;
            MAIX_WEBRTC_LOG("[WebRTC] Built-in signaling server started successfully\n");
            return true;
        }

        _signaling_ws = std::make_shared<rtc::WebSocket>();

        _signaling_ws->onOpen([this]() {
            MAIX_WEBRTC_LOG("WebSocket connected to signaling server\n");
            if (_on_connected) {
                _on_connected();
            }
        });

        _signaling_ws->onClosed([this]() {
            MAIX_WEBRTC_LOG("WebSocket closed\n");
            if (_on_disconnected) {
                _on_disconnected();
            }
        });

        _signaling_ws->onError([](const std::string &error) {
            std::cerr << "WebSocket error: " << error << std::endl;
        });

        _signaling_ws->onMessage([this](std::variant<rtc::binary, std::string> data) {
            if (!std::holds_alternative<std::string>(data))
                return;

            try {
                json message = json::parse(std::get<std::string>(data));
                std::string type = message["type"];

                std::cout << "Received signaling message: " << type << std::endl;

                if (type == "request") {
                    std::cout << "Client requested stream, sending offer" << std::endl;

                    std::string client_id = message["id"];

                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    std::string local_desc = get_local_description();
                    if (!local_desc.empty()) {
                        json offer_message = {
                            {"id", client_id},
                            {"type", "offer"},
                            {"sdp", local_desc}
                        };
                        _signaling_ws->send(offer_message.dump());
                        _offer_sent = true;
                        std::cout << "Sent offer to client: " << client_id << std::endl;
                    } else {
                        std::cout << "Local description is empty, waiting..." << std::endl;
                    }

                } else if (type == "answer") {
                    std::string sdp = message["sdp"];
                    std::cout << "Received answer SDP" << std::endl;
                    set_remote_description(sdp);
                }
            } catch (const std::exception &e) {
                std::cerr << "Error parsing signaling message: " << e.what() << std::endl;
            }
        });

        std::string signaling_url = "ws://" + ip + ":" + std::to_string(port) + "/" + _client_id;
        std::cout << "Connecting to signaling server: " << signaling_url << std::endl;

        try {
            _signaling_ws->open(signaling_url);
        } catch (const std::exception &e) {
            std::cerr << "Failed to connect to signaling server: " << e.what() << std::endl;
            return false;
        }

        std::cout << "Waiting for signaling connection..." << std::endl;
        int wait_count = 0;
        while (!_signaling_ws->isOpen() && !_signaling_ws->isClosed() && wait_count < 50) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            wait_count++;
        }

        if (!_signaling_ws->isOpen()) {
            std::cerr << "Failed to connect to signaling server" << std::endl;
            return false;
        }

        std::cout << "Connected to signaling server" << std::endl;
        return true;
    }

    bool is_signaling_connected() {
        return _signaling_ws && _signaling_ws->isOpen();
    }

    bool is_offer_sent() {
        return _offer_sent;
    }

    void set_on_connected(std::function<void()> callback) {
        _on_connected = callback;
    }

    void set_on_disconnected(std::function<void()> callback) {
        _on_disconnected = callback;
    }

    void set_remote_description(const std::string &description) {
        if (!_peer_connection) return;
        try {
            _peer_connection->setRemoteDescription(rtc::Description(description, "offer"));
        } catch (const std::exception &e) {
            std::cerr << "Error setting remote description: " << e.what() << std::endl;
        }
    }

    void add_remote_candidate(const std::string &candidate, const std::string &mid = "") {
        if (!_peer_connection) return;
        try {
            _peer_connection->addRemoteCandidate(rtc::Candidate(candidate, mid));
        } catch (const std::exception &e) {
            std::cerr << "Error adding remote candidate: " << e.what() << std::endl;
        }
    }

};

class MaixWebRTCServerBuilder {
    std::string _ice_server = "";
    bool _has_audio = false;
    int _audio_sample_rate = 48000;
    int _audio_channels = 1;
    VideoCodec _video_codec = VideoCodec::H264;
    bool _enable_http_server = false;
    std::string _http_host = "0.0.0.0";
    uint16_t _http_port = 8080;
    std::string _stun_server = "stun:stun.l.google.com:19302";
    uint16_t _ws_port = 8001;

public:
    MaixWebRTCServerBuilder() {
    }

    ~MaixWebRTCServerBuilder() {
    }

    MaixWebRTCServerBuilder &set_ice_server(std::string ice_server) {
        _ice_server = ice_server;
        return *this;
    }

    MaixWebRTCServerBuilder &set_audio(bool en) {
        _has_audio = en;
        return *this;
    }

    MaixWebRTCServerBuilder &set_audio_sample_rate(int sample_rate) {
        _audio_sample_rate = sample_rate;
        return *this;
    }

    MaixWebRTCServerBuilder &set_audio_channels(int channels) {
        _audio_channels = channels;
        return *this;
    }

    MaixWebRTCServerBuilder &set_video_codec(VideoCodec codec) {
        _video_codec = codec;
        return *this;
    }

    MaixWebRTCServerBuilder &set_http_server(bool enable, const std::string &host = "0.0.0.0",
                                             uint16_t port = 8080) {
        _enable_http_server = enable;
        _http_host = host;
        _http_port = port;
        return *this;
    }

    MaixWebRTCServerBuilder &set_stun_server(const std::string &stun_server) {
        _stun_server = stun_server;
        return *this;
    }

    MaixWebRTCServerBuilder &set_ws_port(uint16_t ws_port) {
        _ws_port = ws_port;
        return *this;
    }

    MaixWebRTCServer *build() {
        rtc::Configuration config;

        if (!_ice_server.empty()) {
            config.iceServers.emplace_back(_ice_server);
        }

        std::shared_ptr<rtc::PeerConnection> peer_connection;
        try {
            peer_connection = std::make_shared<rtc::PeerConnection>(config);
        } catch (const std::exception &e) {
            std::cerr << "Failed to create PeerConnection: " << e.what() << std::endl;
            throw "webrtc peer connection creation failed";
        }

        int *clients = (int *)malloc(sizeof(int));
        if (clients == nullptr) {
            throw "alloc memory failed";
        }
        *clients = 0;

        MaixWebRTCServer *new_server = new MaixWebRTCServer(clients, peer_connection, _has_audio, _video_codec);

        new_server->_peer_config = config;

        new_server->add_video_track();
        if (_has_audio) {
            new_server->add_audio_track();
        }

        // Start HTTP server if enabled
        if (_enable_http_server) {
            new_server->_http_server = std::make_shared<MaixWebRTCHttpServer>(_http_host, _http_port, _stun_server, _ws_port);
            if (new_server->_http_server->start()) {
                new_server->_use_http_server = true;
                MAIX_WEBRTC_LOG("[HttpServer] Access WebRTC client at %s\n", new_server->_http_server->get_url().c_str());
            } else {
                MAIX_WEBRTC_LOG("[HttpServer] Failed to start HTTP server\n");
            }
        }

        return new_server;
    }
};
