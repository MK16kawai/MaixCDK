#pragma once

#include "rtc/rtc.hpp"
#include "nlohmann/json.hpp"
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
#include <map>
#include <functional>

using json = nlohmann::json;

class MaixWebRTCSignalingServer {
private:
    std::string _host;
    uint16_t _port;
    std::shared_ptr<rtc::WebSocketServer> _ws_server;
    std::map<std::string, std::shared_ptr<rtc::WebSocket>> _clients;
    std::mutex _clients_mutex;
    bool _is_running;
    std::function<void(const std::string &, const json &)> _on_request;
    std::function<void(const std::string &, const json &)> _on_message;
    std::function<void(const std::string &)> _on_client_disconnected;

public:
    MaixWebRTCSignalingServer(const std::string &host = "0.0.0.0", uint16_t port = 8001)
        : _host(host), _port(port), _is_running(false) {
        start();
    }

    ~MaixWebRTCSignalingServer() {
        stop();
    }

    bool start() {
        if (_is_running) return true;

        try {
            rtc::WebSocketServerConfiguration config;
            config.port = _port;
            config.bindAddress = _host;

            _ws_server = std::make_shared<rtc::WebSocketServer>(config);

            _ws_server->onClient([this](std::shared_ptr<rtc::WebSocket> ws) {
                handle_client(ws);
            });

            _is_running = true;
            MAIX_WEBRTC_LOG("[SignalingServer] Started on ws://%s:%d\n", _host.c_str(), _port);
            return true;
        } catch (const std::exception &e) {
            MAIX_WEBRTC_LOG("[SignalingServer] Failed to start: %s\n", e.what());
            _is_running = false;
            return false;
        }
    }

    void stop() {
        _is_running = false;
        if (_ws_server) {
            _ws_server->stop();
            _ws_server = nullptr;
        }
        {
            std::lock_guard<std::mutex> lock(_clients_mutex);
            _clients.clear();
        }
    }

    bool is_running() const {
        return _is_running;
    }

    uint16_t get_port() const {
        if (_ws_server) {
            return _ws_server->port();
        }
        return _port;
    }

    void set_on_request(std::function<void(const std::string &, const json &)> callback) {
        _on_request = callback;
    }

    void set_on_message(std::function<void(const std::string &, const json &)> callback) {
        _on_message = callback;
    }

    void set_on_client_disconnected(std::function<void(const std::string &)> callback) {
        _on_client_disconnected = callback;
    }

    void send_to_client(const std::string &client_id, const json &message) {
        std::lock_guard<std::mutex> lock(_clients_mutex);
        auto it = _clients.find(client_id);
        if (it != _clients.end() && it->second) {
            try {
                it->second->send(message.dump());
                MAIX_WEBRTC_LOG("[SignalingServer] Sent message to %s\n", client_id.c_str());
            } catch (const std::exception &e) {
                MAIX_WEBRTC_LOG("[SignalingServer] Failed to send to %s: %s\n", client_id.c_str(), e.what());
            }
        }
    }

private:
    void handle_client(std::shared_ptr<rtc::WebSocket> ws) {
        auto client_id_ptr = std::make_shared<std::string>();

        ws->onOpen([this, ws, client_id_ptr]() {
            MAIX_WEBRTC_LOG("[SignalingServer] New client connected\n");
        });

        ws->onMessage([this, ws, client_id_ptr](std::variant<rtc::binary, std::string> data) {
            if (!std::holds_alternative<std::string>(data)) {
                return;
            }

            try {
                std::string msg_str = std::get<std::string>(data);
                json message = json::parse(msg_str);

                if (client_id_ptr->empty()) {
                    if (message.contains("id")) {
                        *client_id_ptr = message["id"];
                    } else {
                        *client_id_ptr = "client_" + std::to_string(reinterpret_cast<uintptr_t>(ws.get()));
                    }

                    {
                        std::lock_guard<std::mutex> lock(_clients_mutex);
                        _clients[*client_id_ptr] = ws;
                    }
                    MAIX_WEBRTC_LOG("[SignalingServer] Client registered: %s\n", client_id_ptr->c_str());
                }

                std::string client_id = *client_id_ptr;
                std::string msg_type = message.value("type", "");
                MAIX_WEBRTC_LOG("[SignalingServer] Message from %s (type: %s)\n", client_id.c_str(), msg_type.c_str());

                if (msg_type == "request") {
                    if (_on_request) {
                        _on_request(client_id, message);
                    }
                    return;
                }

                if (msg_type == "answer") {
                    if (_on_message) {
                        _on_message(client_id, message);
                    }
                    return;
                }

                std::string destination_id = message.value("id", "");

                if (!destination_id.empty() && destination_id != client_id) {
                    std::lock_guard<std::mutex> lock(_clients_mutex);
                    auto it = _clients.find(destination_id);
                    if (it != _clients.end() && it->second) {
                        message["id"] = client_id;
                        std::string response = message.dump();

                        try {
                            it->second->send(response);
                            MAIX_WEBRTC_LOG("[SignalingServer] Message forwarded to %s\n", destination_id.c_str());
                        } catch (const std::exception &e) {
                            MAIX_WEBRTC_LOG("[SignalingServer] Failed to send to %s: %s\n", destination_id.c_str(), e.what());
                        }
                    } else {
                        MAIX_WEBRTC_LOG("[SignalingServer] Client not found: %s\n", destination_id.c_str());
                    }
                }
            } catch (const std::exception &e) {
                MAIX_WEBRTC_LOG("[SignalingServer] Error parsing message: %s\n", e.what());
            }
        });

        ws->onClosed([this, client_id_ptr]() {
            if (!client_id_ptr->empty()) {
                std::string client_id = *client_id_ptr;
                {
                    std::lock_guard<std::mutex> lock(_clients_mutex);
                    _clients.erase(client_id);
                }
                MAIX_WEBRTC_LOG("[SignalingServer] Client disconnected: %s\n", client_id.c_str());

                if (_on_client_disconnected) {
                    _on_client_disconnected(client_id);
                }
            }
        });

        ws->onError([](const std::string &error) {
            MAIX_WEBRTC_LOG("[SignalingServer] WebSocket error: %s\n", error.c_str());
        });
    }
};
