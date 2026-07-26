#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/framebuffer.h>

#include <iostream>
#include <thread>
#include <fstream>
#include <csignal>
#include <sys/mman.h>

#include "lib/cxxopts.hpp"

#include "constants.h"
#include "servers.h"
#include "rc_utils.h"

#define BUFFER_SIZE 4

class IntDataServer : DataServer<int, BUFFER_SIZE>
{
public:
    IntDataServer(int port, BufferSystem<int, BUFFER_SIZE>* data) : DataServer<int, BUFFER_SIZE>(port, data) {};
    ~IntDataServer() = default;

    void sendData(int client_fd, int data) override
    {
        std::string s = std::format("Sending from buffer {}", data);
        write_all(client_fd, s);
    }
};

class FrameBufferDataServer : DataServer<libcamera::FrameBuffer*, BUFFER_SIZE>
{
    FrameBufferDataServer(int port, BufferSystem<libcamera::FrameBuffer*, BUFFER_SIZE>* data) :
        DataServer<libcamera::FrameBuffer*, BUFFER_SIZE>(port, data) {};
    ~FrameBufferDataServer() = default;

    void sendData(int client_fd, libcamera::FrameBuffer* data) override
    {
        std::string s = "Test data send";
        write_all(client_fd, s);
    }
};

/* Stuff for controlling main program loop */
std::atomic<bool> mainLoopRunning{true};
void sigintListener(int) {
    mainLoopRunning = false;
}

int main(int argc, char* argv[]) {
    cxxopts::Options options("raw-camera", "Serves raw camera data");

    options.add_options()
        ("h,help", "Print usage")
        ("c,control", "TCP port for control signals",
            cxxopts::value<int>()->default_value(std::to_string(DEFAULT_CONTROL_PORT)))
        ("d,data", "TCP port for data",
            cxxopts::value<int>()->default_value(std::to_string(DEFAULT_DATA_PORT)));

    auto result = options.parse(argc, argv);

    options.parse_positional({"control", "data"});

    // Help
    if (result.count("help")) {
        std::cout << options.help() << "\n";
        return 0;
    }

    // Get the positional integer (uses default if not provided)
    int control_port = result["control"].as<int>();
    int data_port = result["data"].as<int>();

    /* Camera Stuff */

    libcamera::CameraManager camera_manager;
    camera_manager.start();

    {

        /* Get first camera */
        auto cameras = camera_manager.cameras();

        if (cameras.empty()) {
            std::cerr << "No cameras found\n";
            return -1;
        }

        auto camera = cameras[0];

        camera->acquire();

        /* Configure camera */

        auto config =
            camera->generateConfiguration({ libcamera::StreamRole::Raw });

        libcamera::CameraConfiguration::Status status = config->validate();

        if (status == libcamera::CameraConfiguration::Invalid) {
            std::cerr << "Configuration is invalid\n";
            return -1;
        }

        if (status == libcamera::CameraConfiguration::Adjusted)
            std::cout << "Configuration was adjusted\n";

        int ret = camera->configure(config.get());
        if (ret) {
            std::cerr << "configure() failed: " << ret << '\n';
            return ret;
        }

        // Info about format
        const auto &cfg = config->at(0);

        std::cout << "Size: "
                  << cfg.size.width << " x "
                  << cfg.size.height << '\n';

        std::cout << "Pixel format: "
                  << cfg.pixelFormat.toString() << '\n';

        std::cout << "Stride: "
                  << cfg.stride << '\n';

        /* Set up data stream and buffers for camera */
        libcamera::Stream *stream = config->at(0).stream();

        std::unique_ptr<libcamera::FrameBufferAllocator> allocator =
            std::make_unique<libcamera::FrameBufferAllocator>(camera);

        allocator->allocate(stream);

        const auto &buffers = allocator->buffers(stream);


        /* Initialise buffers */
        int buffer_data[BUFFER_SIZE];
        for (int i=0; i<BUFFER_SIZE; i++)
        {
            buffer_data[i] = i+1;
        }

        /* Set up systems */
        BufferSystem<int, BUFFER_SIZE>* buffer_system = new BufferSystem<int, 4>(buffer_data);

        ControlServer control_server = ControlServer(control_port);
        IntDataServer data_server = IntDataServer(data_port, buffer_system);

        std::function<std::string(void)> capture_callback = [buffer_system]()
        {
            buffer_system->pushStart();
            buffer_system->pushFinish();
            return "Dummy Capture Done\n";
        };

        control_server.setCaptureCallback(capture_callback);


        // data_server->bind_control_server(control_server);
        // control_server->bind_data_server(data_server);
        //
        // control_server->start();
        // data_server->start();
        //
        // std::this_thread::sleep_for(std::chrono::seconds(5));
        //
        // control_server->stop();
        // data_server->stop();

        //bufferSystemTest();

        /* Wait until ctrl-C pressed */
        sigset_t set;
        sigemptyset(&set);
        sigaddset(&set, SIGINT);

        // Block SIGINT in this thread.
        pthread_sigmask(SIG_BLOCK, &set, nullptr);

        // Wait until Ctrl+C is pressed.
        int sig;
        sigwait(&set, &sig);

        std::cout << "Ctrl+C pressed, exiting ...\n";

        camera->stop();
        // request.reset();      // destroy requests
        allocator.reset();    // destroy buffer allocator
        camera->release();
        camera.reset();       // release shared_ptr
    }

    camera_manager.stop();

    return 0;
}