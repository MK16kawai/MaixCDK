#pragma once

#include <string>
#include <memory>
#include <iostream>

#ifdef NDEBUG
#define MAIX_WEBRTC_LOG(...)
#else
#define MAIX_WEBRTC_LOG(...) printf(__VA_ARGS__)
#endif

#include <thread>
#include <atomic>
#include "httplib.h"

class MaixWebRTCHttpServer {
private:
    std::string _host;
    uint16_t _port;
    std::string _stun_server;
    uint16_t _ws_port;
    std::unique_ptr<httplib::Server> _server;
    std::unique_ptr<std::thread> _server_thread;
    std::atomic<bool> _is_running;

    static constexpr const char WEBRTC_INDEX_HTML[] = R"HTML(
    <html>
    <head>
        <meta charset="UTF-8">
        <meta name="viewport" content="width=device-width, initial-scale=1.0">
        <title>WebRTC Streaming</title>

        <style>
            html, body {
                margin: 0;
                padding: 0;
                height: 100%;
                background: #3a3a3a;
                font-family: Arial, Helvetica, sans-serif;

                display: flex;
                justify-content: center;
                align-items: center;
            }

            #container {
                width: 100%;
                max-width: 900px;
                text-align: center;
                padding: 20px;
            }

            #title {
                font-size: 28px;
                font-weight: bold;
                margin-bottom: 20px;
                color: white;
                background: rgba(255, 255, 255, 0.08);
                padding: 10px 20px;
                border-radius: 12px;
                backdrop-filter: blur(6px);
            }

            #video-wrap {
                position: relative;
                width: 100%;
                max-height: 65vh;
            }

            #video {
                width: 100%;
                max-height: 65vh;
                background: black;
                border-radius: 18px;
                box-shadow: 0 0 20px rgba(0,0,0,0.5);
                transition: box-shadow 0.3s ease;
                display: block;
            }

            #video:hover {
                box-shadow: 0 0 30px rgba(0,0,0,0.7);
            }

            #resolution-badge {
                position: absolute;
                left: 12px;
                bottom: 12px;
                background: rgba(0,0,0,0.55);
                color: #fff;
                padding: 6px 10px;
                border-radius: 10px;
                font-size: 13px;
                backdrop-filter: blur(6px);
                box-shadow: 0 4px 10px rgba(0,0,0,0.4);
            }

            #fullscreen-btn {
                display: none;
                position: fixed;
                bottom: 40px;
                right: 40px;
                width: 55px;
                height: 55px;

                background: rgba(255, 255, 255, 0.15);
                border: none;
                border-radius: 50%;
                cursor: pointer;

                color: white;
                font-size: 14px;

                backdrop-filter: blur(10px);
                box-shadow: 0 0 12px rgba(0,0,0,0.4);

                transition: all 0.25s ease;
            }

            #fullscreen-btn:hover {
                background: rgba(255, 255, 255, 0.25);
                transform: scale(1.08);
            }

            @media (max-width: 600px) {
                #title {
                    font-size: 22px;
                }
                #fullscreen-btn {
                    width: 48px;
                    height: 48px;
                    bottom: 20px;
                    right: 20px;
                }
            }
        </style>
    </head>

    <body>
        <div id="container">
            <div id="title">WebRTC Streaming</div>
            <div id="video-wrap">
                <video id="video" autoplay playsinline muted></video>
                <div id="resolution-badge">
                    <span id="res-text">-- × --</span>
                    <span id="codec-text" style="margin-left:8px; opacity:0.85;">(unknown)</span>
                </div>
            </div>
        </div>

        <button id="fullscreen-btn">⛶</button>

        <script src="https://webrtc.github.io/adapter/adapter-latest.js"></script>
        <script src="client.js"></script>
    </body>
    </html>
    )HTML";

    static constexpr const char WEBRTC_CLIENT_JS[] = R"JS(
    const clientId = randomId(10);
    const wsUrl = 'ws://' + window.location.hostname + ':8001/' + clientId;
    console.log('Connecting to:', wsUrl);
    const websocket = new WebSocket(wsUrl);

    websocket.onopen = () => {
        console.log('WebSocket connected, sending request...');
        sendRequest();
    }

    websocket.onmessage = async (evt) => {
        if (typeof evt.data !== 'string') return;
        const message = JSON.parse(evt.data);
        if (message.type == "offer") {
            await handleOffer(message);
        }
    }

    let pc = null;
    let dc = null;

    function createPeerConnection() {
        const config = {
            bundlePolicy: "max-bundle",
            iceServers: [{ urls: ['stun:stun.l.google.com:19302'] }],
        };

        let pc = new RTCPeerConnection(config);

        pc.ontrack = (evt) => {
            const video = document.getElementById('video');
            video.srcObject = evt.streams[0];
            video.play();

            document.getElementById("fullscreen-btn").style.display = "block";

            function updateResolution() {
                const video = document.getElementById('video');
                const res = document.getElementById('res-text');
                const codec = document.getElementById('codec-text');
                const badge = document.getElementById('resolution-badge');

                const w = video.videoWidth || 0;
                const h = video.videoHeight || 0;

                if (w === 0 || h === 0) {
                    badge.style.display = "none";
                } else {
                    badge.style.display = "block";
                    res.textContent = `${w} × ${h}`;
                }

                try {
                    const params = pc.getReceivers()?.[0]?.getParameters();
                    if (params && params.codecs && params.codecs.length > 0) {
                        const mime = params.codecs[0].mimeType || "";
                        codec.textContent = mime.replace("video/", "").toUpperCase();
                    }
                } catch (e) { /* Not Supported getParameters */ }
            }

            video.addEventListener('loadedmetadata', updateResolution);
            if (window.ResizeObserver) {
                const ro = new ResizeObserver(() => {
                    updateResolution();
                });
                ro.observe(video);
            }

            let tries = 0;
            const poll = setInterval(() => {
                tries++;
                updateResolution();
                if ((video.videoWidth && video.videoHeight) || tries > 20) {
                    clearInterval(poll);
                }
            }, 200);
        };

        pc.ondatachannel = (evt) => {
            dc = evt.channel;
            dc.onopen = () => {};
            dc.onmessage = () => {};
            dc.onclose = () => {};
        };

        return pc;
    }

    async function waitGatheringComplete() {
        return new Promise((resolve) => {
            if (pc.iceGatheringState === 'complete') resolve();
            else pc.addEventListener('icegatheringstatechange', () => {
                if (pc.iceGatheringState === 'complete') resolve();
            });
        });
    }

    async function sendAnswer(pc) {
        await pc.setLocalDescription(await pc.createAnswer());
        await waitGatheringComplete();

        const answer = pc.localDescription;
        websocket.send(JSON.stringify({
            id: "server",
            type: answer.type,
            sdp: answer.sdp,
        }));
    }

    async function handleOffer(offer) {
        pc = createPeerConnection();
        await pc.setRemoteDescription(offer);
        await sendAnswer(pc);
    }

    function sendRequest() {
        websocket.send(JSON.stringify({ id: "server", type: "request" }));
    }

    function randomId(length) {
    const chars = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz';
    return [...Array(length)].map(() => chars.charAt(Math.floor(Math.random() * Math.random()*chars.length))).join('');
    }

    let startTime = null;
    function currentTimestamp() {
        if (startTime === null) return (startTime = Date.now(), 0);
        return Date.now() - startTime;
    }

    document.getElementById("fullscreen-btn").onclick = () => {
        const video = document.getElementById("video");

        if (!document.fullscreenElement) {
            video.requestFullscreen?.() ||
            video.webkitRequestFullscreen?.() ||
            video.mozRequestFullScreen?.() ||
            video.msRequestFullscreen?.();
        } else {
            document.exitFullscreen?.() ||
            document.webkitExitFullscreen?.() ||
            document.mozCancelFullScreen?.() ||
            document.msExitFullscreen?.();
        }
    };
    )JS";

public:
    MaixWebRTCHttpServer(const std::string &host = "0.0.0.0", uint16_t port = 8080,
                         const std::string &stun_server = "stun:stun.l.google.com:19302",
                         uint16_t ws_port = 8001)
        : _host(host), _port(port), _stun_server(stun_server), _ws_port(ws_port), _is_running(false) {
    }

    ~MaixWebRTCHttpServer() {
        stop();
    }

    bool start() {
        if (_is_running) return true;

        _server = std::make_unique<httplib::Server>();

        _server->Get("/", [](const httplib::Request& req, httplib::Response& res) {
            res.set_content(WEBRTC_INDEX_HTML, "text/html; charset=utf-8");
        });

        _server->Get("/index.html", [](const httplib::Request& req, httplib::Response& res) {
            res.set_content(WEBRTC_INDEX_HTML, "text/html; charset=utf-8");
        });

        _server->Get("/client.js", [this](const httplib::Request& req, httplib::Response& res) {
            std::string js_content = std::string(WEBRTC_CLIENT_JS);
            std::string ws_url_old = "const wsUrl = 'ws://' + window.location.hostname + ':8001/' + clientId;";
            std::string ws_url_new = "const wsUrl = 'ws://' + window.location.hostname + ':" + std::to_string(_ws_port) + "/' + clientId;";
            size_t pos = js_content.find(ws_url_old);
            if (pos != std::string::npos) {
                js_content.replace(pos, ws_url_old.length(), ws_url_new);
            }

            std::string stun_old = "iceServers: [{ urls: ['stun:stun.l.google.com:19302'] }]";
            std::string stun_new = "iceServers: [{ urls: ['" + _stun_server + "'] }]";
            pos = js_content.find(stun_old);
            if (pos != std::string::npos) {
                js_content.replace(pos, stun_old.length(), stun_new);
            }

            res.set_content(js_content, "application/javascript; charset=utf-8");
        });

        _is_running = true;
        _server_thread = std::make_unique<std::thread>([this]() {

            MAIX_WEBRTC_LOG("[HttpServer] Starting on http://%s:%d\n", _host.c_str(), _port);
            MAIX_WEBRTC_LOG("[HttpServer] Serving embedded WebRTC client interface\n");

            if (!_server->listen(_host.c_str(), _port)) {
                MAIX_WEBRTC_LOG("[HttpServer] Failed to bind to %s:%d\n", _host.c_str(), _port);
                _is_running = false;
            }
        });

        std::this_thread::sleep_for(std::chrono::milliseconds(100));

        return _is_running;
    }

    void stop() {
        if (_is_running) {
            _is_running = false;

            if (_server) {
                _server->stop();
            }

            if (_server_thread && _server_thread->joinable()) {
                _server_thread->join();
            }

            MAIX_WEBRTC_LOG("[HttpServer] Stopped\n");
        }
    }

    bool is_running() const {
        return _is_running;
    }

    uint16_t get_port() const {
        return _port;
    }

    std::string get_url() const {
        return "http://" + (_host == "0.0.0.0" ? "localhost" : _host) + ":" + std::to_string(_port);
    }
};
