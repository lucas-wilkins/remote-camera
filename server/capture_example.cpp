#include <libcamera/libcamera.h>
#include <libcamera/control_ids.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/framebuffer.h>

#include <iostream>
#include <memory>
#include <condition_variable>
#include <mutex>
#include <fstream>
#include <sys/mman.h>

using namespace libcamera;

std::mutex mutex_;
std::condition_variable cv_;
bool captureDone = false;

void requestComplete(Request *request)
{
    if (request->status() == Request::RequestCancelled)
        return;

    for (const auto &[stream, buffer] : request->buffers()) {
        for (unsigned int i = 0; i < buffer->planes().size(); i++) {
            const FrameBuffer::Plane &plane = buffer->planes()[i];

            void *memory = mmap(nullptr,
                                plane.length,
                                PROT_READ,
                                MAP_SHARED,
                                plane.fd.get(),
                                0);

            if (memory == MAP_FAILED) {
                perror("mmap");
                continue;
            }

            std::ofstream out("image.bin", std::ios::binary);
            out.write(static_cast<char *>(memory), plane.bytesused);

            munmap(memory, plane.length);
        }
    }

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

    {
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
        request.reset();      // destroy requests
        allocator.reset();    // destroy buffer allocator
        camera->release();
        camera.reset();       // release shared_ptr
    }

    cm.stop();

    return 0;
}