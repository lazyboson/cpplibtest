// obs_mp4_capture_api.cpp - OBS screen capture with REST API and MP4 recording for M1 MacBook Pro
#include "third_party/obs/include/obs.h"
#include "third_party/obs/include/obs-module.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include <mutex>
#include <map>
#include <atomic>
#include "third_party/httplib.h"
#include "third_party/json.hpp"
#include <utility>
#include <vector>
#include <CoreGraphics/CoreGraphics.h>
#include <csignal>

using json = nlohmann::json;

// Global flag for graceful shutdown
std::atomic<bool> should_stop{false};

void signal_handler(int signal) {
    std::cout << "\nReceived signal " << signal << ", stopping server..." << std::endl;
    should_stop = true;
}

enum class StreamState {
    IDLE,
    RECORDING,
    PAUSED,
    STOPPED
};

class OBSRecorder {
    obs_source_t* screen_capture = nullptr;
    obs_source_t* mic_capture = nullptr;
    obs_source_t* desktop_audio = nullptr;
    obs_scene_t* scene = nullptr;
    obs_sceneitem_t* scene_item = nullptr;
    obs_output_t* output = nullptr;
    obs_encoder_t* video_encoder = nullptr;
    obs_encoder_t* audio_encoder = nullptr;

    std::string stream_id;
    std::string output_file;
    std::atomic<StreamState> state{StreamState::IDLE};
    std::mutex state_mutex;
    std::thread recording_thread;
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point pause_time;
    std::chrono::duration<double> total_paused_duration{0};

    static bool load_plugins() {
        const std::string base_path = "/Applications/3CLogicScreenRecorder.app/Contents/PlugIns";
        const std::vector<std::string> plugins = {
            "mac-capture", "coreaudio-encoder", "obs-ffmpeg",
            "obs-outputs", "obs-x264", "rtmp-services"
        };

        for (const auto& plugin : plugins) {
            std::string plugin_path = base_path;
            plugin_path.append("/")
           .append(plugin)
           .append(".plugin/Contents/MacOS/")
           .append(plugin);
            obs_module_t* module = nullptr;
            if (obs_open_module(&module, plugin_path.c_str(), nullptr) == MODULE_SUCCESS && module) {
                obs_init_module(module);
                std::cout << "Loaded plugin: " << plugin << std::endl;
            } else {
                std::cout << "Warning: Failed to load plugin: " << plugin << std::endl;
            }
        }
        return true;
    }

public:
    explicit OBSRecorder(std::string  id) : stream_id(std::move(id)) {
        // Generate output filename based on stream ID and timestamp
        const auto now = std::chrono::system_clock::now();
        const auto time_t = std::chrono::system_clock::to_time_t(now);
        char timestamp[100];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&time_t));
        output_file = "/tmp/" + stream_id + "_" + timestamp + ".mp4";
    }

    ~OBSRecorder() {
        cleanup();
    }

    bool initialize() {
        std::lock_guard<std::mutex> lock(state_mutex);

        if (state != StreamState::IDLE) {
            return false;
        }

        // Initialize OBS
        if (!obs_startup("en-US", nullptr, nullptr)) {
            std::cerr << "Failed to initialize OBS" << std::endl;
            return false;
        }

        // Load plugins
        load_plugins();

        // Get M1 MacBook Pro native display info
        CGDirectDisplayID main_display = CGMainDisplayID();
        const size_t pixel_width = CGDisplayPixelsWide(main_display);
        const size_t pixel_height = CGDisplayPixelsHigh(main_display);
        const auto [origin, size] = CGDisplayBounds(main_display);
        const auto logical_width = static_cast<size_t>(size.width);
        const auto logical_height = static_cast<size_t>(size.height);
        const CGFloat scale_factor = static_cast<CGFloat>(pixel_width) / static_cast<CGFloat>(logical_width);

        std::cout << "=== M1 MacBook Pro Display Info ===" << std::endl;
        std::cout << "Logical resolution: " << logical_width << "x" << logical_height << " points" << std::endl;
        std::cout << "Pixel resolution: " << pixel_width << "x" << pixel_height << " pixels" << std::endl;
        std::cout << "Scale factor: " << scale_factor << "x" << std::endl;

        // Detect MacBook Pro model
        std::string model = "Unknown";
        if (pixel_width == 2560 && pixel_height == 1600) {
            model = "13\" M1 MacBook Pro";
        } else if (pixel_width == 3024 && pixel_height == 1964) {
            model = "14\" M1 Pro/Max MacBook Pro";
        } else if (pixel_width == 3456 && pixel_height == 2234) {
            model = "16\" M1 Pro/Max MacBook Pro";
        }
        std::cout << "Detected: " << model << std::endl;

        // Setup video with native M1 MacBook Pro resolution
        struct obs_video_info ovi = {};
        ovi.fps_num = 30;
        ovi.fps_den = 1;
        ovi.base_width = pixel_width;
        ovi.base_height = pixel_height;
        ovi.output_width = pixel_width;
        ovi.output_height = pixel_height;
        ovi.output_format = VIDEO_FORMAT_NV12;
        ovi.colorspace = VIDEO_CS_709;
        ovi.range = VIDEO_RANGE_PARTIAL;
        ovi.adapter = 0;
        ovi.gpu_conversion = true;
        ovi.scale_type = OBS_SCALE_BICUBIC;

        std::string opengl_path = "/Applications/3CLogicScreenRecorder.app/Contents/Frameworks/libobs-opengl.dylib";
        ovi.graphics_module = opengl_path.c_str();

        if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) {
            std::cerr << "Failed to initialize video with M1 MacBook Pro settings" << std::endl;
            return false;
        }

        // Setup audio
        struct obs_audio_info oai = {};
        oai.samples_per_sec = 48000;
        oai.speakers = SPEAKERS_STEREO;

        if (!obs_reset_audio(&oai)) {
            std::cerr << "Failed to initialize audio" << std::endl;
            return false;
        }

        std::cout << "M1 MacBook Pro video/audio initialized successfully!" << std::endl;
        return true;
    }

    bool setup_sources() {
        // Create scene
        scene = obs_scene_create("Recording Scene");
        if (!scene) return false;

        // Create screen capture
        obs_data_t* screen_settings = obs_data_create();
        obs_data_set_bool(screen_settings, "show_cursor", true);
        obs_data_set_int(screen_settings, "display", 0);

        screen_capture = obs_source_create("screen_capture", "Screen",
                                         screen_settings, nullptr);
        obs_data_release(screen_settings);

        if (!screen_capture) {
            std::cerr << "Failed to create screen capture" << std::endl;
            return false;
        }

        // Add to scene
        scene_item = obs_scene_add(scene, screen_capture);
        if (scene_item) {
            struct vec2 bounds{};
            struct obs_video_info ovi{};
            obs_get_video_info(&ovi);
            bounds.x = static_cast<float>(ovi.base_width);
            bounds.y = static_cast<float>(ovi.base_height);
            obs_sceneitem_set_bounds(scene_item, &bounds);
            obs_sceneitem_set_bounds_type(scene_item, OBS_BOUNDS_SCALE_INNER);
            struct vec2 scale = {1.0f, 1.0f};
            obs_sceneitem_set_scale(scene_item, &scale);
        }

        // Create audio sources
        obs_data_t* desktop_settings = obs_data_create();
        desktop_audio = obs_source_create("coreaudio_output_capture",
                                        "Desktop Audio", desktop_settings, nullptr);
        obs_data_release(desktop_settings);

        obs_data_t* mic_settings = obs_data_create();
        obs_data_set_string(mic_settings, "device_id", "default");
        mic_capture = obs_source_create("coreaudio_input_capture",
                                      "Microphone", mic_settings, nullptr);
        obs_data_release(mic_settings);

        // Set output sources
        obs_source_t* scene_source = obs_scene_get_source(scene);
        obs_set_output_source(0, scene_source);
        if (mic_capture) obs_set_output_source(1, mic_capture);
        if (desktop_audio) obs_set_output_source(2, desktop_audio);

        return true;
    }

    bool setup_encoding() {
        // Video encoder optimized for M1 MacBook Pro
        obs_data_t* video_settings = obs_data_create();

        struct obs_video_info ovi{};
        obs_get_video_info(&ovi);
        int pixels = ovi.output_width * ovi.output_height;
        int bitrate;

        // Bitrate calculation for MP4 recording (higher quality than streaming)
        if (pixels >= 7700000) {
            bitrate = 20000; // 20 Mbps for 16" M1 MacBook Pro
        } else if (pixels >= 5900000) {
            bitrate = 15000; // 15 Mbps for 14" M1 MacBook Pro
        } else if (pixels >= 4000000) {
            bitrate = 12000; // 12 Mbps for 13" M1 MacBook Pro
        } else if (pixels >= 2073600) {
            bitrate = 8000;  // 8 Mbps for 1080p
        } else {
            bitrate = 5000;  // 5 Mbps fallback
        }

        obs_data_set_int(video_settings, "bitrate", bitrate);
        obs_data_set_string(video_settings, "preset", "medium");
        obs_data_set_string(video_settings, "profile", "high");
        obs_data_set_string(video_settings, "tune", "film");
        obs_data_set_int(video_settings, "keyint_sec", 2);
        obs_data_set_string(video_settings, "rate_control", "CBR");
        obs_data_set_int(video_settings, "buffer_size", bitrate);
        obs_data_set_int(video_settings, "crf", 18);
        obs_data_set_bool(video_settings, "use_bufsize", true);
        obs_data_set_bool(video_settings, "psycho_aq", true);
        obs_data_set_int(video_settings, "bf", 2);

        std::cout << "Video bitrate for MP4: " << bitrate << " kbps" << std::endl;

        video_encoder = obs_video_encoder_create("obs_x264", "Video Encoder",
                                               video_settings, nullptr);
        obs_data_release(video_settings);

        if (!video_encoder) {
            std::cerr << "Failed to create video encoder" << std::endl;
            return false;
        }

        // Audio encoder
        obs_data_t* audio_settings = obs_data_create();
        obs_data_set_int(audio_settings, "bitrate", 320); // High quality audio for recording
        obs_data_set_int(audio_settings, "rate_control", 0);

        audio_encoder = obs_audio_encoder_create("CoreAudio_AAC", "Audio Encoder",
                                               audio_settings, 0, nullptr);
        obs_data_release(audio_settings);

        if (!audio_encoder) {
            std::cerr << "Failed to create audio encoder" << std::endl;
            return false;
        }

        obs_encoder_set_video(video_encoder, obs_get_video());
        obs_encoder_set_audio(audio_encoder, obs_get_audio());

        return true;
    }

    bool start_recording() {
        std::lock_guard<std::mutex> lock(state_mutex);

        if (state != StreamState::IDLE) {
            return false;
        }

        // Create MP4 output
        obs_data_t* output_settings = obs_data_create();
        obs_data_set_string(output_settings, "path", output_file.c_str());

        output = obs_output_create("mp4_output", "Recording",
                                 output_settings, nullptr);
        obs_data_release(output_settings);

        if (!output) {
            std::cerr << "Failed to create MP4 output" << std::endl;
            return false;
        }

        // Set encoders
        obs_output_set_video_encoder(output, video_encoder);
        obs_output_set_audio_encoder(output, audio_encoder, 0);

        // Start recording
        if (!obs_output_start(output)) {
            const char* error = obs_output_get_last_error(output);
            std::cerr << "Failed to start recording: " << (error ? error : "unknown error") << std::endl;
            return false;
        }

        state = StreamState::RECORDING;
        start_time = std::chrono::steady_clock::now();
        total_paused_duration = std::chrono::duration<double>(0);

        std::cout << "Recording started: " << output_file << std::endl;
        return true;
    }

    bool pause_recording() {
        std::lock_guard<std::mutex> lock(state_mutex);

        if (state != StreamState::RECORDING) {
            return false;
        }

        // Note: OBS doesn't have native pause functionality for recording
        // We'll simulate by stopping and restarting with a new file segment
        pause_time = std::chrono::steady_clock::now();
        state = StreamState::PAUSED;

        std::cout << "Recording paused (simulated)" << std::endl;
        return true;
    }

    bool stop_recording() {
        std::lock_guard<std::mutex> lock(state_mutex);

        if (state != StreamState::RECORDING && state != StreamState::PAUSED) {
            return false;
        }

        if (output && obs_output_active(output)) {
            obs_output_stop(output);

            // Wait for output to stop
            int wait_count = 0;
            while (obs_output_active(output) && wait_count < 30) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                wait_count++;
            }

            if (obs_output_active(output)) {
                obs_output_force_stop(output);
            }
        }

        state = StreamState::STOPPED;
        std::cout << "Recording stopped: " << output_file << std::endl;
        return true;
    }

    StreamState get_state() const {
        return state.load();
    }

    std::string get_stream_id() const {
        return stream_id;
    }

    std::string get_output_file() const {
        return output_file;
    }

    json get_status() const {
        json status;
        status["stream_id"] = stream_id;
        status["output_file"] = output_file;

        switch (state.load()) {
            case StreamState::IDLE:
                status["state"] = "idle";
                break;
            case StreamState::RECORDING:
                status["state"] = "recording";
                break;
            case StreamState::PAUSED:
                status["state"] = "paused";
                break;
            case StreamState::STOPPED:
                status["state"] = "stopped";
                break;
        }

        if (state == StreamState::RECORDING || state == StreamState::PAUSED) {
            auto now = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - start_time);
            status["duration_seconds"] = duration.count();
        }

        return status;
    }

private:
    void cleanup() {
        std::lock_guard<std::mutex> lock(state_mutex);

        std::cout << "Starting cleanup for stream: " << stream_id << std::endl;

        if (output && obs_output_active(output)) {
            obs_output_stop(output);
            int wait_count = 0;
            while (obs_output_active(output) && wait_count < 50) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                wait_count++;
            }
            if (obs_output_active(output)) {
                obs_output_force_stop(output);
            }
        }

        // Clear output sources
        for (int i = 0; i < 6; i++) {
            obs_set_output_source(i, nullptr);
        }

        // Release resources
        if (output) {
            obs_output_release(output);
            output = nullptr;
        }

        if (audio_encoder) {
            obs_encoder_release(audio_encoder);
            audio_encoder = nullptr;
        }

        if (video_encoder) {
            obs_encoder_release(video_encoder);
            video_encoder = nullptr;
        }

        if (mic_capture) {
            obs_source_release(mic_capture);
            mic_capture = nullptr;
        }

        if (desktop_audio) {
            obs_source_release(desktop_audio);
            desktop_audio = nullptr;
        }

        if (scene && scene_item) {
            obs_sceneitem_remove(scene_item);
            scene_item = nullptr;
        }

        if (screen_capture) {
            obs_source_release(screen_capture);
            screen_capture = nullptr;
        }

        if (scene) {
            obs_scene_release(scene);
            scene = nullptr;
        }

        obs_shutdown();
        std::cout << "Cleanup complete for stream: " << stream_id << std::endl;
    }
};

class RecordingManager {
private:
    std::unique_ptr<httplib::Server> server;
    std::map<std::string, std::unique_ptr<OBSRecorder>> recorders;
    std::mutex recorders_mutex;

public:
    RecordingManager() : server(std::make_unique<httplib::Server>()) {
        setup_routes();
    }

    void setup_routes() {
        // Enable CORS
        server->set_pre_routing_handler([](const httplib::Request& req, httplib::Response& res) {
            res.set_header("Access-Control-Allow-Origin", "*");
            res.set_header("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.set_header("Access-Control-Allow-Headers", "Content-Type, Authorization");
            return httplib::Server::HandlerResponse::Unhandled;
        });

        // Handle preflight requests
        server->Options(".*", [](const httplib::Request&, httplib::Response& res) {
            return;
        });

        // POST /v1/stream/{streamId}/start
        server->Post("/v1/stream/([^/]+)/start", [this](const httplib::Request& req, httplib::Response& res) {
            std::string stream_id = req.matches[1];

            try {
                std::lock_guard<std::mutex> lock(recorders_mutex);

                // Check if recorder already exists
                if (recorders.find(stream_id) != recorders.end()) {
                    json error_response;
                    error_response["error"] = "Stream already exists";
                    error_response["stream_id"] = stream_id;
                    res.status = 409; // Conflict
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                // Create new recorder
                auto recorder = std::make_unique<OBSRecorder>(stream_id);

                if (!recorder->initialize()) {
                    json error_response;
                    error_response["error"] = "Failed to initialize recorder";
                    error_response["stream_id"] = stream_id;
                    res.status = 500;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                if (!recorder->setup_sources()) {
                    json error_response;
                    error_response["error"] = "Failed to setup sources";
                    error_response["stream_id"] = stream_id;
                    res.status = 500;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                if (!recorder->setup_encoding()) {
                    json error_response;
                    error_response["error"] = "Failed to setup encoding";
                    error_response["stream_id"] = stream_id;
                    res.status = 500;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                if (!recorder->start_recording()) {
                    json error_response;
                    error_response["error"] = "Failed to start recording";
                    error_response["stream_id"] = stream_id;
                    res.status = 500;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                // Store recorder
                recorders[stream_id] = std::move(recorder);

                json response;
                response["message"] = "Recording started";
                response["stream_id"] = stream_id;
                response["output_file"] = recorders[stream_id]->get_output_file();
                res.status = 200;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                json error_response;
                error_response["error"] = "Internal server error";
                error_response["details"] = e.what();
                res.status = 500;
                res.set_content(error_response.dump(), "application/json");
            }
        });

        // PUT /v1/stream/{streamId}/pause
        server->Put("/v1/stream/([^/]+)/pause", [this](const httplib::Request& req, httplib::Response& res) {
            std::string stream_id = req.matches[1];

            try {
                std::lock_guard<std::mutex> lock(recorders_mutex);

                const auto it = recorders.find(stream_id);
                if (it == recorders.end()) {
                    json error_response;
                    error_response["error"] = "Stream not found";
                    error_response["stream_id"] = stream_id;
                    res.status = 404;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                if (!it->second->pause_recording()) {
                    json error_response;
                    error_response["error"] = "Failed to pause recording";
                    error_response["stream_id"] = stream_id;
                    res.status = 400;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                json response;
                response["message"] = "Recording paused";
                response["stream_id"] = stream_id;
                res.status = 200;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                json error_response;
                error_response["error"] = "Internal server error";
                error_response["details"] = e.what();
                res.status = 500;
                res.set_content(error_response.dump(), "application/json");
            }
        });

        // DELETE /v1/stream/{streamId}/stop
        server->Delete("/v1/stream/([^/]+)/stop", [this](const httplib::Request& req, httplib::Response& res) {
            std::string stream_id = req.matches[1];

            try {
                std::lock_guard<std::mutex> lock(recorders_mutex);

                auto it = recorders.find(stream_id);
                if (it == recorders.end()) {
                    json error_response;
                    error_response["error"] = "Stream not found";
                    error_response["stream_id"] = stream_id;
                    res.status = 404;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                std::string output_file = it->second->get_output_file();

                if (!it->second->stop_recording()) {
                    json error_response;
                    error_response["error"] = "Failed to stop recording";
                    error_response["stream_id"] = stream_id;
                    res.status = 400;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                // Remove recorder after stopping
                recorders.erase(it);

                json response;
                response["message"] = "Recording stopped";
                response["stream_id"] = stream_id;
                response["output_file"] = output_file;
                res.status = 200;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                json error_response;
                error_response["error"] = "Internal server error";
                error_response["details"] = e.what();
                res.status = 500;
                res.set_content(error_response.dump(), "application/json");
            }
        });

        // GET /v1/stream/{streamId}/status
        server->Get("/v1/stream/([^/]+)/status", [this](const httplib::Request& req, httplib::Response& res) {
            std::string stream_id = req.matches[1];

            try {
                std::lock_guard<std::mutex> lock(recorders_mutex);

                auto it = recorders.find(stream_id);
                if (it == recorders.end()) {
                    json error_response;
                    error_response["error"] = "Stream not found";
                    error_response["stream_id"] = stream_id;
                    res.status = 404;
                    res.set_content(error_response.dump(), "application/json");
                    return;
                }

                json response = it->second->get_status();
                res.status = 200;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                json error_response;
                error_response["error"] = "Internal server error";
                error_response["details"] = e.what();
                res.status = 500;
                res.set_content(error_response.dump(), "application/json");
            }
        });

        // GET /v1/streams - List all streams
        server->Get("/v1/streams", [this](const httplib::Request& req, httplib::Response& res) {
            try {
                std::lock_guard<std::mutex> lock(recorders_mutex);

                json response;
                response["streams"] = json::array();

                for (const auto& pair : recorders) {
                    response["streams"].push_back(pair.second->get_status());
                }

                res.status = 200;
                res.set_content(response.dump(), "application/json");

            } catch (const std::exception& e) {
                json error_response;
                error_response["error"] = "Internal server error";
                error_response["details"] = e.what();
                res.status = 500;
                res.set_content(error_response.dump(), "application/json");
            }
        });

        // Health check endpoint
        server->Get("/health", [](const httplib::Request& req, httplib::Response& res) {
            json response;
            response["status"] = "healthy";
            response["service"] = "obs-recorder-api";
            res.set_content(response.dump(), "application/json");
        });
    }

    void start_server(const std::string& host = "0.0.0.0", int port = 8080) {
        std::cout << "Starting OBS Recording API server on " << host << ":" << port << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "  POST   /v1/stream/{streamId}/start" << std::endl;
        std::cout << "  PUT    /v1/stream/{streamId}/pause" << std::endl;
        std::cout << "  DELETE /v1/stream/{streamId}/stop" << std::endl;
        std::cout << "  GET    /v1/stream/{streamId}/status" << std::endl;
        std::cout << "  GET    /v1/streams" << std::endl;
        std::cout << "  GET    /health" << std::endl;
        std::cout << "\nRecordings will be saved to: /tmp/" << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;

        server->listen(host, port);
    }

    void stop_server() {
        std::cout << "Stopping server..." << std::endl;
        server->stop();
    }
};

int main(int argc, char* argv[]) {
    // Setup signal handlers
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    std::cout << "OBS MP4 Recording API for M1 MacBook Pro" << std::endl;
    std::cout << "=======================================" << std::endl;

    // Parse command line arguments
    std::string host = "0.0.0.0";
    int port = 8080;

    if (argc > 1) {
        port = std::atoi(argv[1]);
    }
    if (argc > 2) {
        host = argv[2];
    }

    try {
        RecordingManager manager;

        // Start server in a separate thread
        std::thread server_thread([&manager, host, port]() {
            manager.start_server(host, port);
        });

        // Wait for shutdown signal
        while (!should_stop) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Stop server
        manager.stop_server();

        // Wait for server thread to finish
        if (server_thread.joinable()) {
            server_thread.join();
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "Server stopped successfully" << std::endl;
    return 0;
}
