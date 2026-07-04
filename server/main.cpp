#include <iostream>
#include <thread>

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
        return "Dummy Capture Done";
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

    while (true)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}
