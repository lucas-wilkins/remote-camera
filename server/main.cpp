#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/framebuffer.h>

#include <iostream>
#include <thread>
#include <fstream>
#include <csignal>
#include <numeric>
#include <utility>
#include <sys/mman.h>

#include "lib/cxxopts.hpp"

#include "constants.h"
#include "servers.h"
#include "rc_utils.h"


class DataServer : public TCPServer
{
private:

    libcamera::Request* currentRequest_;
    std::mutex mtxSend_;
    std::condition_variable cvSend_;
    std::function<void()> sendDoneCallback = []()
    {
        std::cout << "sendDoneCallback not set yet" << std::endl;
    };

public:
    DataServer(int port) : TCPServer(port) {};
    ~DataServer() = default;

    void sendData(libcamera::Request* request)
    {
        currentRequest_ = request;
        cvSend_.notify_one();
    }

    void mainLoop() override
    {

        while (running_)
        {
            std::unique_lock<std::mutex> lock(mtxSend_);
            cvSend_.wait(lock);

            write_all(getClientFd(), "[This is dummy send data]");

            delete currentRequest_;

            sendDoneCallback();
        }
    }

    void setSendDoneCallback(std::function<void()> callback)
    {
        sendDoneCallback = std::move(callback);
    }

};

/** Callback: Things to do when a request completes */
void requestComplete(libcamera::Request *request)
{
    if (request->status() == libcamera::Request::RequestCancelled)
        return;

    const libcamera::ControlList &metadata = request->metadata();

    if (metadata.contains(libcamera::controls::FrameDuration.id())) {
        int64_t frameDuration =
            metadata.get(libcamera::controls::FrameDuration).value();

        std::cout << "Frame duration: "
                  << frameDuration << " us\n";
    }

    if (metadata.contains(libcamera::controls::ExposureTime.id())) {
        int64_t exposure =
            metadata.get(libcamera::controls::ExposureTime).value();

        std::cout << "Exposure: "
                  << exposure << " us\n";
    }
}


/** Stuff for controlling main program loop */
std::atomic<bool> mainLoopRunning{true};
void sigintListener(int) {
    mainLoopRunning = false;
}



class Main
{
public:


    int exposureTime = 10000;
    float analogueGain = 2.0f;

    libcamera::CameraManager cameraManager;
    std::shared_ptr<libcamera::Camera> camera;
    ControlServer* controlServer;
    DataServer* dataServer;
    std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>> *buffers = nullptr;
    libcamera::Stream* stream;

    std::mutex bufferIndexMutex;
    int bufferIndex = 0;
    int bufferCount = 0;
    int buffersUsed = 0;

    Main(int control_port, int data_port)
    {
        cameraManager.start();

        {
            /* Get first camera */
            auto cameras = cameraManager.cameras();

            if (cameras.empty())
            {
                throw std::runtime_error("No cameras found");
            }

            camera = cameras[0];
            camera->acquire();

            /* Configure camera */

            auto config =
                camera->generateConfiguration({libcamera::StreamRole::Raw});

            libcamera::CameraConfiguration::Status status = config->validate();

            if (status == libcamera::CameraConfiguration::Invalid)
            {
                throw std::runtime_error("Configuration is invalid");
            }

            if (status == libcamera::CameraConfiguration::Adjusted)
            {
                std::cout << "Configuration was adjusted\n";
            }

            int ret = camera->configure(config.get());
            if (ret)
            {
                throw std::runtime_error(std::format("configure() failed: {}", ret));
            }

            // Info about format
            const auto& cfg = config->at(0);

            std::cout << "Size: "
                << cfg.size.width << " x "
                << cfg.size.height << '\n';

            std::cout << "Pixel format: "
                << cfg.pixelFormat.toString() << '\n';

            std::cout << "Stride: "
                << cfg.stride << '\n';

            /*
             * Set up data stream and buffers for camera
             */

            stream = config->at(0).stream();

            allocator =
                std::make_unique<libcamera::FrameBufferAllocator>(camera);

            allocator->allocate(stream);

            buffers = &allocator->buffers(stream);


            std::cout << "Buffer Count: "
                << cfg.bufferCount << '\n';

            camera->start();

            /*
             * Create servers and wire them up
             */

            controlServer = new ControlServer(control_port);
            dataServer = new DataServer(data_port);

            // callbacks
            controlServer->setCaptureCallback([this]()
            {
                return captureCallback();
            });

            controlServer->setExposureCallback([this](int exposure)
            {
                return exposureCallback(exposure);
            });

            controlServer->setGainCallback([this](float gain)
            {
                return analogueGainCallback(gain);
            });

            controlServer->setStatusCallback([this]()
            {
                return statusCallback();
            });

            // Make data errors get sent to control server
            dataServer->setErrorMessageCallback(controlServer->getErrorMessageCallback());
            dataServer->setSendDoneCallback([this]()
            {
                sendDoneCallback();
            });

        }

    }

    ~Main()
    {

        cameraManager.stop();
        controlServer->stop();
        dataServer->stop();

        camera->stop();
        allocator.reset();    // destroy buffer allocator
        camera->release();
        camera.reset();       // release shared_ptr

        delete controlServer;
        delete dataServer;
    }

    void requestCallback(libcamera::Request* request)
    {
        dataServer->sendData(request);

        delete request;
    }

    std::string exposureCallback(int exposure)
    {
        exposureTime = exposure;
        return std::format("Set exposure time to {} microseconds", exposureTime);
    }

    std::string analogueGainCallback(float gain)
    {
        analogueGain = gain;
        return std::format("Set analogue gain to {}", analogueGain);
    }

    std::string statusCallback()
    {
        return "Status info goes here";
    }

    std::string captureCallback()
    {
        int index;

        // Add a request to the queue
        {
            std::lock_guard lock(bufferIndexMutex);

            if (buffersUsed >= bufferCount)
            {
                return"Failed to capture, buffers all in use\n";
            }

            index = bufferIndex;

            bufferIndex++;
            bufferIndex %= bufferCount;
            buffersUsed++;

        }

        /*
        std::unique_ptr<libcamera::Request> request =
            camera->createRequest();

        request->addBuffer(stream, (*buffers)[index].get());

        // Manual exposure settings
        request->controls().set(libcamera::controls::AeEnable, false);
        request->controls().set(libcamera::controls::ExposureTime, exposureTime);
        request->controls().set(libcamera::controls::AnalogueGain, analogueGain);

        camera->queueRequest(request.get());
        */

        return "Dummy Capture Done\n";
    };

    void sendDoneCallback()
    {
        {
            std::lock_guard lock(bufferIndexMutex);

            bufferCount--;
        }
    }

};


int main(int argc, char* argv[])
{
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

    try
    {
        Main main = Main(control_port, data_port);


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

    } catch (const std::runtime_error e)
    {
        std::cerr << e.what() << '\n';
    }

}