// obs_mp4_capture_api_singleton.cpp - OBS screen capture with REST API and MP4 recording using singleton pattern
#include "third_party/obs/include/obs.h"
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

// Singleton OBS Core Manager
class OBSCore {
private:
    static std::unique_ptr<OBSCore> instance;
    static std::mutex instance_mutex;

    bool initialized = false;
    std::mutex core_mutex;

    // Display info
    size_t pixel_width = 0;
    size_t pixel_height = 0;
    size_t logical_width = 0;
    size_t logical_height = 0;
    CGFloat scale_factor = 0;

    OBSCore() = default;

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
    ~OBSCore() {
        shutdown();
    }

    static OBSCore* getInstance() {
        std::lock_guard<std::mutex> lock(instance_mutex);
        if (!instance) {
            instance = std::unique_ptr<OBSCore>(new OBSCore());
        }
        return instance.get();
    }

    bool initialize() {
        std::lock_guard<std::mutex> lock(core_mutex);

        if (initialized) {
            return true;
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
        pixel_width = CGDisplayPixelsWide(main_display);
        pixel_height = CGDisplayPixelsHigh(main_display);
        const auto [origin, size] = CGDisplayBounds(main_display);
        logical_width = static_cast<size_t>(size.width);
        logical_height = static_cast<size_t>(size.height);
        scale_factor = static_cast<CGFloat>(pixel_width) / static_cast<CGFloat>(logical_width);

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
            obs_shutdown();
            return false;
        }

        // Setup audio
        struct obs_audio_info oai = {};
        oai.samples_per_sec = 48000;
        oai.speakers = SPEAKERS_STEREO;

        if (!obs_reset_audio(&oai)) {
            std::cerr << "Failed to initialize audio" << std::endl;
            obs_shutdown();
            return false;
        }

        initialized = true;
        std::cout << "OBS Core initialized successfully!" << std::endl;
        return true;
    }

    void shutdown() {
        std::lock_guard<std::mutex> lock(core_mutex);
        if (initialized) {
            obs_shutdown();
            initialized = false;
            std::cout << "OBS Core shutdown complete" << std::endl;
        }
    }

    bool isInitialized() const {
        return initialized;
    }

    void getVideoInfo(size_t& width, size_t& height) const {
        width = pixel_width;
        height = pixel_height;
    }

    int calculateBitrate() const {
        int pixels = pixel_width * pixel_height;
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

        return bitrate;
    }
};

// Initialize static members
std::unique_ptr<OBSCore> OBSCore::instance = nullptr;
std::mutex OBSCore::instance_mutex;

// Stream Recorder class that uses the singleton OBS instance
class StreamRecorder {
private:
    // Each recorder has its own scene and output
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
    std::chrono::steady_clock::time_point start_time;
    std::chrono::steady_clock::time_point pause_time;
    std::chrono::duration<double> total_paused_duration{0};

    // Track which output channels we're using
    int video_channel = -1;
    int audio_channel = -1;
    int desktop_channel = -1;

    // Static channel allocation (simple round-robin)
    static std::mutex channel_mutex;
    static std::vector<bool> used_channels;

public:
    explicit StreamRecorder(std::string id) : stream_id(std::move(id)) {
        // Generate output filename based on stream ID and timestamp
        const auto now = std::chrono::system_clock::now();
        const auto time_t = std::chrono::system_clock::to_time_t(now);
        char timestamp[100];
        std::strftime(timestamp, sizeof(timestamp), "%Y%m%d_%H%M%S", std::localtime(&time_t));
        output_file = "/tmp/" + stream_id + "_" + timestamp + ".mp4";
    }

    ~StreamRecorder() {
        cleanup();
    }

    bool setup_sources() {
        // Create scene
        scene = obs_scene_create(("Recording Scene " + stream_id).c_str());
        if (!scene) return false;

        // Create screen capture
        obs_data_t* screen_settings = obs_data_create();
        obs_data_set_bool(screen_settings, "show_cursor", true);
        obs_data_set_int(screen_settings, "display", 0);

        screen_capture = obs_source_create("screen_capture",
                                         ("Screen " + stream_id).c_str(),
                                         screen_settings, nullptr);
        obs_data_release(screen_settings);

        if (!screen_capture) {
            std::cerr << "Failed to create screen capture for stream: " << stream_id << std::endl;
            return false;
        }

        // Add to scene
        scene_item = obs_scene_add(scene, screen_capture);
        if (scene_item) {
            struct vec2 bounds{};
            size_t width, height;
            OBSCore::getInstance()->getVideoInfo(width, height);
            bounds.x = static_cast<float>(width);
            bounds.y = static_cast<float>(height);
            obs_sceneitem_set_bounds(scene_item, &bounds);
            obs_sceneitem_set_bounds_type(scene_item, OBS_BOUNDS_SCALE_INNER);
            struct vec2 scale = {1.0f, 1.0f};
            obs_sceneitem_set_scale(scene_item, &scale);
        }

        // Create audio sources
        obs_data_t* desktop_settings = obs_data_create();
        desktop_audio = obs_source_create("coreaudio_output_capture",
                                        ("Desktop Audio " + stream_id).c_str(),
                                        desktop_settings, nullptr);
        obs_data_release(desktop_settings);

        obs_data_t* mic_settings = obs_data_create();
        obs_data_set_string(mic_settings, "device_id", "default");
        mic_capture = obs_source_create("coreaudio_input_capture",
                                      ("Microphone " + stream_id).c_str(),
                                      mic_settings, nullptr);
        obs_data_release(mic_settings);

        // Allocate output channels
        allocate_channels();

        // Set output sources
        obs_source_t* scene_source = obs_scene_get_source(scene);
        obs_set_output_source(video_channel, scene_source);
        if (mic_capture && audio_channel >= 0) obs_set_output_source(audio_channel, mic_capture);
        if (desktop_audio && desktop_channel >= 0) obs_set_output_source(desktop_channel, desktop_audio);

        return true;
    }

    bool setup_encoding() {
        // Video encoder optimized for M1 MacBook Pro
        obs_data_t* video_settings = obs_data_create();
        int bitrate = OBSCore::getInstance()->calculateBitrate();

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

        std::cout << "Video bitrate for MP4 (" << stream_id << "): " << bitrate << " kbps" << std::endl;

        video_encoder = obs_video_encoder_create("obs_x264",
                                               ("Video Encoder " + stream_id).c_str(),
                                               video_settings, nullptr);
        obs_data_release(video_settings);

        if (!video_encoder) {
            std::cerr << "Failed to create video encoder for stream: " << stream_id << std::endl;
            return false;
        }

        // Audio encoder
        obs_data_t* audio_settings = obs_data_create();
        obs_data_set_int(audio_settings, "bitrate", 320); // High quality audio for recording
        obs_data_set_int(audio_settings, "rate_control", 0);

        audio_encoder = obs_audio_encoder_create("CoreAudio_AAC",
                                               ("Audio Encoder " + stream_id).c_str(),
                                               audio_settings, 0, nullptr);
        obs_data_release(audio_settings);

        if (!audio_encoder) {
            std::cerr << "Failed to create audio encoder for stream: " << stream_id << std::endl;
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

        output = obs_output_create("mp4_output", ("Recording " + stream_id).c_str(),
                                 output_settings, nullptr);
        obs_data_release(output_settings);

        if (!output) {
            std::cerr << "Failed to create MP4 output for stream: " << stream_id << std::endl;
            return false;
        }

        // Set encoders
        obs_output_set_video_encoder(output, video_encoder);
        obs_output_set_audio_encoder(output, audio_encoder, 0);

        // Start recording
        if (!obs_output_start(output)) {
            const char* error = obs_output_get_last_error(output);
            std::cerr << "Failed to start recording for stream " << stream_id
                      << ": " << (error ? error : "unknown error") << std::endl;
            return false;
        }

        state = StreamState::RECORDING;
        start_time = std::chrono::steady_clock::now();
        total_paused_duration = std::chrono::duration<double>(0);

        std::cout << "Recording started for stream " << stream_id << ": " << output_file << std::endl;
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

        std::cout << "Recording paused for stream " << stream_id << " (simulated)" << std::endl;
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
        std::cout << "Recording stopped for stream " << stream_id << ": " << output_file << std::endl;
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
    void allocate_channels() {
        std::lock_guard<std::mutex> lock(channel_mutex);

        // Initialize channel array if needed
        if (used_channels.empty()) {
            used_channels.resize(MAX_CHANNELS, false);
        }

        // Find free channels
        for (int i = 0; i < MAX_CHANNELS; ++i) {
            if (!used_channels[i]) {
                if (video_channel < 0) {
                    video_channel = i;
                    used_channels[i] = true;
                } else if (audio_channel < 0) {
                    audio_channel = i;
                    used_channels[i] = true;
                } else if (desktop_channel < 0) {
                    desktop_channel = i;
                    used_channels[i] = true;
                    break;
                }
            }
        }
    }

    void release_channels() {
        std::lock_guard<std::mutex> lock(channel_mutex);

        if (video_channel >= 0 && video_channel < MAX_CHANNELS) {
            used_channels[video_channel] = false;
            obs_set_output_source(video_channel, nullptr);
        }
        if (audio_channel >= 0 && audio_channel < MAX_CHANNELS) {
            used_channels[audio_channel] = false;
            obs_set_output_source(audio_channel, nullptr);
        }
        if (desktop_channel >= 0 && desktop_channel < MAX_CHANNELS) {
            used_channels[desktop_channel] = false;
            obs_set_output_source(desktop_channel, nullptr);
        }
    }

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

        // Release channels
        release_channels();

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

        std::cout << "Cleanup complete for stream: " << stream_id << std::endl;
    }
};

// Initialize static members
std::mutex StreamRecorder::channel_mutex;
std::vector<bool> StreamRecorder::used_channels;

class RecordingManager {
private:
    std::unique_ptr<httplib::Server> server;
    std::map<std::string, std::unique_ptr<StreamRecorder>> recorders;
    std::mutex recorders_mutex;

public:
    RecordingManager() : server(std::make_unique<httplib::Server>()) {
        // Initialize OBS core once
        if (!OBSCore::getInstance()->initialize()) {
            throw std::runtime_error("Failed to initialize OBS core");
        }
        setup_routes();
    }

    ~RecordingManager() {
        // Clean up all recorders before shutting down OBS
        {
            std::lock_guard<std::mutex> lock(recorders_mutex);
            recorders.clear();
        }
        // OBS core will be cleaned up automatically by its destructor
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
                auto recorder = std::make_unique<StreamRecorder>(stream_id);

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
                response["active_streams"] = recorders.size();
                response["obs_core_initialized"] = OBSCore::getInstance()->isInitialized();

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
            response["service"] = "obs-singleton-recorder-api";
            response["obs_core"] = OBSCore::getInstance()->isInitialized() ? "initialized" : "not initialized";
            res.set_content(response.dump(), "application/json");
        });
    }

    void start_server(const std::string& host = "0.0.0.0", int port = 8080) {
        std::cout << "Starting OBS Singleton Recording API server on " << host << ":" << port << std::endl;
        std::cout << "Available endpoints:" << std::endl;
        std::cout << "  POST   /v1/stream/{streamId}/start" << std::endl;
        std::cout << "  PUT    /v1/stream/{streamId}/pause" << std::endl;
        std::cout << "  DELETE /v1/stream/{streamId}/stop" << std::endl;
        std::cout << "  GET    /v1/stream/{streamId}/status" << std::endl;
        std::cout << "  GET    /v1/streams" << std::endl;
        std::cout << "  GET    /health" << std::endl;
        std::cout << "\nRecordings will be saved to: /tmp/" << std::endl;
        std::cout << "Using singleton OBS core for all recordings" << std::endl;
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

    std::cout << "OBS Singleton MP4 Recording API for M1 MacBook Pro" << std::endl;
    std::cout << "=================================================" << std::endl;

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