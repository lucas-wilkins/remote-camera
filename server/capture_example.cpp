#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/framebuffer.h>

#include <iostream>
#include <memory>
#include <condition_variable>
#include <mutex>

using namespace libcamera;

std::mutex mutex_;
std::condition_variable cv_;
bool captureDone = false;

void requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled)
        return;

    std::cout << "Capture completed!" << std::endl;

    {
        std::lock_guard<std::mutex> lock(mutex_);
        captureDone = true;
    }

    cv_.notify_one();
}

int main()
{
    CameraManager cm;
    cm.start();

    auto cameras = cm.cameras();

    if (cameras.empty()) {
        std::cerr << "No cameras found\n";
        return -1;
    }

    auto camera = cameras[0];

    camera->acquire();

    auto config =
        camera->generateConfiguration({ StreamRole::StillCapture });

    camera->configure(config.get());
    Stream *stream = config->at(0).stream();

    // Buffers
    std::unique_ptr<FrameBufferAllocator> allocator =
    std::make_unique<FrameBufferAllocator>(camera);

    allocator->allocate(stream);

    const auto &buffers = allocator->buffers(stream);

    // Connect completion callback
    camera->requestCompleted.connect(requestComplete);

    std::unique_ptr<Request> request =
        camera->createRequest();

    request->addBuffer(stream, buffers[0].get());

    // Manual exposure settings
    request->controls().set(controls::AeEnable, false);
    request->controls().set(controls::ExposureTime, 10000);
    request->controls().set(controls::AnalogueGain, 2.0f);

    camera->start();

    camera->queueRequest(request.get());

    // Wait for completion
    {
        std::unique_lock<std::mutex> lock(mutex_);

        cv_.wait(lock, [] {
            return captureDone;
        });
    }

    std::cout << "Request fully processed\n";

    camera->stop();
    camera->release();

    cm.stop();

    return 0;
}