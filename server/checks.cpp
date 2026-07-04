//
// Created by lucas on 28/06/2026.
//

#include "checks.h"
#include "servers.h"

void bufferSystemTest()
{
    // Build the buffer system


    int bufferData[4] = {0,1,2,3};
    BufferSystem<int, 4> bs = BufferSystem<int, 4>(bufferData);

    /* A thread for reading out from the buffer every second */
    bool running = true;
    std::thread worker([running, &bs] {
        int x;
        while (running) {
            // std::cout << "SEND: Getting access index" << std::endl;
            x = bs.popStart();

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            // std::cout << "SEND: Got index " << x << std::endl;

            bs.popFinish();
            // std::cout << "SEND: Pop finished" << std::endl;

            std::cout << "SEND -> : " << x << " (" << bs.size() << ")" << std::endl;

            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    });

    // So some pushing and see how it responds

    std::cout << "Started sender thread, waiting 2 second to start filling buffer" << std::endl;
    std::this_thread::sleep_for(std::chrono::milliseconds(2000));

    while (true)
    {

        for (int t: {200, 700, 1400, 2000})
        {
            for (int i=0; i < 6; i++)
            {

                // std::cout << "Buffer size: " << bs.size() << std::endl;
                // std::cout << "CAPT: Getting write index" << std::endl;
                std::optional<int> slot = bs.pushStart();

                if (slot)
                {
                    // std::cout << "CAPT: Got index " << *slot << std::endl;

                    bs.pushFinish();
                    std::cout << "CAPT <- : " << *slot << " (" << bs.size() << ")" << std::endl;

                } else
                {
                    std::cout << "CAPT <- : FULL (" << bs.size() << ")" << std::endl;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(t));

            }
        }
    }

}