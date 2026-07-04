//
// Created by lucas on 26/06/2026.
//

#ifndef SERVER_SERVERS_H
#define SERVER_SERVERS_H

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <stdexcept>
#include <iostream>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <format>

#include "rc_utils.h"


/*
 * Buffer system
 */

template <typename T, std::size_t N>
class BufferSystem
{
public:
    BufferSystem(T initial_data[N]);
    ~BufferSystem() = default;

    T popStart();
    void popFinish();

    std::optional<T> pushStart();
    void pushFinish();

    std::size_t size();

private:
    T data[N];
    std::size_t start = 0;
    std::size_t length = 0;

    std::mutex mtxSizeChange;
    std::condition_variable cvHasData;

};

template <typename T, std::size_t N>
BufferSystem<T, N>::BufferSystem(T initial_data[N])
{
    // Copy the data over
    std::copy(initial_data, initial_data + N, data);
}


template <typename T, std::size_t N>
T BufferSystem<T, N>::popStart()
{
    // Wait for the size to be > 0
    {
        std::unique_lock<std::mutex> lock(mtxSizeChange);

        cvHasData.wait(lock, [this]
        {
            return length > 0;
        });

        return data[start];
    }
}

template <typename T, std::size_t N>
void BufferSystem<T, N>::popFinish()
{
    {
        // Lock the buffer size
        std::unique_lock<std::mutex> lock(mtxSizeChange);

        if (length == 0)
        {
            throw std::runtime_error("No data in array, cannot finish (popFinish called on empty array)");
        }

        // Move the start variable up, and length down
        start = (start + 1) % N;
        length--;

    }

}


template <typename T, std::size_t N>
std::optional<T> BufferSystem<T, N>::pushStart()
{
    {
        // Lock whilst doing the checking, and working out of position
        std::unique_lock<std::mutex> lock(mtxSizeChange);

        // return optional in case buffer is full
        if (length < N)
        {
            std::size_t pos = (start + length) % N;
            return data[pos];
        }

        return std::nullopt;

    }
}

template <typename T, std::size_t N>
void BufferSystem<T, N>::pushFinish()
{
    {
        // Lock the buffer size
        std::unique_lock<std::mutex> lock(mtxSizeChange);
        length++;
    }

    // Trigger any waiting threads
    cvHasData.notify_one();
}

template <typename T, std::size_t N>
std::size_t BufferSystem<T, N>::size()
{
    {
        std::unique_lock<std::mutex> lock(mtxSizeChange);
        return length;
    }
}

/*
 * TCP server base class
 *
 */


class TCPServer
{
public:
    explicit TCPServer(int port);
    ~TCPServer();

    virtual void mainLoop(int client_fd) = 0;
    bool connected();

private:
    int port_;

    void start();
    void stop();
    void run();

    std::thread worker_;

    bool connected_ = false;


protected:
    std::atomic<bool> running_{false};

};

inline TCPServer::TCPServer(int port)
{
    port_ = port;
    start();
}

inline TCPServer::~TCPServer()
{
    stop();
}

inline void TCPServer::start() {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return; // already running
    }

    worker_ = std::thread(&TCPServer::run, this);
}

inline void TCPServer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    if (worker_.joinable()) {
        worker_.join();
    }
}

inline void TCPServer::run() {
    std::cout << "Control server starting\n";

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(server_fd, (sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        return;
    }

    if (listen(server_fd, 5) < 0) {
        perror("listen");
        return;
    }

    sockaddr_in client{};
    socklen_t client_len = sizeof(client);

    while (running_)
    {
        std::cout << "TCP server listening on port " << port_ << std::endl;

        int client_fd = accept(server_fd, (sockaddr*)&client, &client_len);
        if (client_fd < 0) {
            perror("accept");
            return;
        }
        connected_ = true;

        std::cout << "Client Connected" << std::endl;

        mainLoop(client_fd);

        std::cout << "Client Disconnected" << std::endl;


        close(client_fd);
    }

    std::cout << "Control server stopped\n";

}

/*
 * Control Server
 */


class ControlServer : public TCPServer
{
public:

    using TCPServer::TCPServer;

    void mainLoop(int client_fd) override;
    void setExposureCallback(std::function<std::string(int64_t)> callback);
    void setGainCallback(std::function<std::string(float)> callback);
    void setCaptureCallback(std::function<std::string()> callback);
    void setStatusCallback(std::function<std::string()> callback);


private:

    char buffer[1024];

    std::function<std::string(long)> exposureCallback_ = [](int64_t exposure)
    {
        return std::format("No exposure callback set (attempted to set to {})", exposure);
    };

    std::function<std::string(float)> gainCallback_ = [](float gain)
    {
        return std::format("No gain callback set (attempted to set to {})", gain);
    };

    std::function<std::string()> captureCallback_ = []
    {
        return "No capture callback set";
    };

    std::function<std::string()> statusCallback_ = []
    {
        return "No status callback set";
    };
};


inline void ControlServer::setExposureCallback(std::function<std::string(int64_t)> callback)
{
    exposureCallback_ = callback;
}

inline void ControlServer::setGainCallback(std::function<std::string(float)> callback)
{
    gainCallback_ = callback;
}


inline void ControlServer::setCaptureCallback(std::function<std::string()> callback)
{
    captureCallback_ = callback;
}


inline void ControlServer::setStatusCallback(std::function<std::string()> callback)
{
    statusCallback_ = callback;
}


inline void ControlServer::mainLoop(int client_fd)
{


    ssize_t n;
    while ((n = read(client_fd, buffer, sizeof(buffer))) > 0) {
        int message_type = buffer[0];

        std::string msg;
        std::string response;

        if (false)
        {
            response = "ERROR: Data server not set up";
        }
        else
        {
            switch (message_type)
            {
            case ControlMessageType::STATUS:
                response = statusCallback_();
                break;

            case ControlMessageType::CAPTURE:
                response = captureCallback_();
                break;

            case ControlMessageType::SET_EXPOSURE:
                if (n == 9)
                {
                    int64_t value = 0;
                    std::memcpy(&value, &buffer[1], sizeof(value));
                    response = exposureCallback_(value);
                } else
                {
                    response = "Malformed exposure time, expected 8 bytes";
                }
                break;

            case ControlMessageType::SET_GAIN:
                if (n == 5)
                {
                    float value = 0;
                    std::memcpy(&value, &buffer[1], sizeof(value));
                    response = gainCallback_(value);
                } else
                {
                    response = "Malformed gain, expected 5 bytes";
                }


                break;

            default:
                response = "Unknown Command";
            }
        }

        write_all(client_fd, response);
        std::cout << response << std::endl;
    }


}


/*
 * Control Server
 */

template <typename T, std::size_t N>
class DataServer : public TCPServer
{
public:
    DataServer(int port, BufferSystem<T, N>* buffer_system);


    void mainLoop(int client_fd) override;
    virtual void sendData(int client_fd, T data);

private:
    BufferSystem<T, N>* buffer_system_;
};

template <typename T, std::size_t N>
DataServer<T, N>::DataServer(int port, BufferSystem<T, N>* buffer_system) : TCPServer(port)
{
    buffer_system_ = buffer_system;
}

template <typename T, std::size_t N>
void DataServer<T, N>::mainLoop(int client_fd)
{
    while (running_)
    {
        T target = buffer_system_->popStart();
        sendData(client_fd, target);
        buffer_system_->popFinish();
    }
}





#endif //SERVER_SERVERS_H
