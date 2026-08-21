#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/framebuffer.h>

#include <iostream>
#include <thread>
#include <fstream>
#include <csignal>
#include <sys/mman.h>
#include <queue>

#include "lib/cxxopts.hpp"

#include "constants.h"
#include "servers.h"
#include "rc_utils.h"

template <class T>
class SendServer : public TCPServer
{
private:

    std::queue<T> dataQueue;
    std::mutex mtxSend_;
    std::condition_variable cvSend_;
    std::function<void()> sendDoneCallback = []()
    {
        std::cout << "sendDoneCallback not set yet" << std::endl;
    };

public:
    SendServer(int port) : TCPServer(port) {};
    ~SendServer() = default;

    void send(T data)
    {
        dataQueue.push(data);
        cvSend_.notify_one();
    }

    void mainLoop() override
    {

        std::cout << "Data Server mainLoop" << std::endl;
        while (running_)
        {
            std::unique_lock<std::mutex> lock(mtxSend_);
            cvSend_.wait_for(lock, std::chrono::milliseconds(200));

            if (!running_)
            {
                break;
            }

            while (!dataQueue.empty())
            {
                sendFunction(dataQueue.front());
                dataQueue.pop();
            }
        }
        std::cout << "DataServer mainLoop exiting" << std::endl;
    }

    virtual void sendFunction(T data)
    {

    }

    void stop() override
    {

        std::cout << std::format("Stopping TCP Server on {}\n", getPort());

        running_ = false;
        cvSend_.notify_one();

        std::thread* worker = getWorkerThread();

        if (worker->joinable()) {
            std::cout << "Joining SendServer thread" << std::endl;
            worker->join();
        }
    }

};

class MessageServer : public SendServer<std::string>
{
public:
    MessageServer(int port) : SendServer(port) {};

    void sendFunction(std::string data) override
    {
        write_all(getClientFd(), data);
    }
};

class DataServer : public SendServer<libcamera::Request*>
{
public:
    DataServer(int port) : SendServer(port) {};

    void sendFunction(libcamera::Request* data) override
    {
        write_all(getClientFd(), "Data server test write");
        delete data;
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
    MessageServer* messageServer;
    DataServer* dataServer;

    std::unique_ptr<libcamera::FrameBufferAllocator> allocator;
    const std::vector<std::unique_ptr<libcamera::FrameBuffer>> *buffers = nullptr;
    libcamera::Stream* stream;

    std::mutex bufferIndexMutex;
    unsigned int bufferIndex = 0;
    unsigned int bufferCount = 0;
    unsigned int buffersUsed = 0;

    Main(int control_port, int message_port, int data_port)
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
            bufferCount = cfg.bufferCount;

            camera->start();

            /*
             * Create servers and wire them up
             */

            controlServer = new ControlServer(control_port);
            messageServer = new MessageServer(message_port);
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

            controlServer->setResponseCallback([this](std::string msg)
            {
               messageServer->send(msg);
            });


        }

    }

    ~Main()
    {

        delete controlServer;
        delete messageServer;
        delete dataServer;

        camera->stop();
        allocator.reset();    // destroy buffer allocator
        camera->release();
        camera.reset();       // release shared_ptr
    }

    void requestCallback(libcamera::Request* request)
    {
        dataServer->send(request);

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
        return std::format("Buffers used {}/{}", buffersUsed, bufferCount);
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

            buffersUsed--;
        }
    }

};


int main(int argc, char* argv[])
{
    cxxopts::Options options("raw-camera", "Serves raw camera data");

    options.add_options()
        ("h,help", "Print usage")
        ("c,control",
            "TCP port for control signals",
            cxxopts::value<int>()->default_value(std::to_string(DEFAULT_CONTROL_PORT)))
        ("m,message",
            "TCP port for messages",
            cxxopts::value<int>()->default_value(std::to_string(DEFAULT_MESSAGE_PORT)))
        ("d,data",
            "TCP port for data",
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
    int message_port = result["message"].as<int>();

    try
    {
        Main main = Main(control_port, message_port, data_port);


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