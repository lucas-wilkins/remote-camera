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
#include <syncstream>
#include <poll.h>

#include "rc_utils.h"


/*
 * TCP server base class
 *
 */


class TCPServer
{
public:
    explicit TCPServer(int port);
    ~TCPServer();

    virtual void mainLoop() = 0;
    bool connected();
    virtual void stop();

    void setErrorMessageCallback(std::function<void(std::string)> callback);
    std::function<void(std::string)> getErrorMessageCallback();

    int getClientFd();
    int getPort();
    std::thread* getWorkerThread();

private:
    int port_;

    void start();
    void run();

    int client_fd_ = -1;

    std::thread worker_;

    bool connected_ = false;

    std::function<void(std::string)> errorMessageCallback_ = [this](std::string msg)
    {
        write_all(client_fd_, msg);
        std::cerr << msg << std::endl;
    };


protected:
    std::atomic<bool> running_{false};

};

inline TCPServer::TCPServer(int port)
{
    port_ = port;
    client_fd_ = 0;
    start();
}

inline TCPServer::~TCPServer()
{
    stop();
}

inline std::thread* TCPServer::getWorkerThread()
{
    return &worker_;
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

    // std::cout << "Stopping TCP Server on " << port_ << std::endl;

    running_ = false;

    if (worker_.joinable()) {
        std::cout << "Joining TCP server worker thread\n";
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
        std::osyncstream(std::cout) << "TCP server listening on port " << port_ << std::endl;

        client_fd_ = accept(server_fd, (sockaddr*)&client, &client_len);
        if (client_fd_ < 0) {
            perror("accept");
            continue;
        }
        connected_ = true;

        std::cout << "Client Connected\n";

        mainLoop();

        std::cout << "Client Disconnected\n";


        close(client_fd_);
    }

    std::cout << "Server stopped\n";

}

inline int TCPServer::getClientFd()
{
    return client_fd_;
}

inline int TCPServer::getPort()
{
    return port_;
}

inline void TCPServer::setErrorMessageCallback(std::function<void(std::string)> callback)
{
    errorMessageCallback_ = callback;
}

inline std::function<void(std::string)> TCPServer::getErrorMessageCallback()
{
    return errorMessageCallback_;
}


/*
 * Control Server
 */


class ControlServer : public TCPServer
{
public:

    using TCPServer::TCPServer;

    void mainLoop() override;
    void setExposureCallback(std::function<std::string(int64_t)> callback);
    void setGainCallback(std::function<std::string(float)> callback);
    void setCaptureCallback(std::function<std::string()> callback);
    void setStatusCallback(std::function<std::string()> callback);
    void setResponseCallback(std::function<void(std::string)> callback);


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

    std::function<void(std::string)> responseCallback_ = [](std::string msg)
    {
        return;
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

inline void ControlServer::setResponseCallback(std::function<void(std::string)> callback)
{
    responseCallback_ = callback;
}


inline void ControlServer::mainLoop()
{
    ssize_t n;

    while (running_) {
        struct pollfd pfd{};
        pfd.fd = getClientFd();
        pfd.events = POLLIN;

        int ret = poll(&pfd, 1, 100); // 100 ms timeout

        if (!running_)
            break;

        if (ret < 0) {
            if (errno == EINTR)
                continue;

            // Handle error
            break;
        }

        if (ret == 0)
            continue; // timeout; check running_ again

        if (pfd.revents & POLLIN) {
            n = read(getClientFd(), buffer, sizeof(buffer));

            if (n <= 0)
                break;

            int message_type = buffer[0];

            std::string response;

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

            std::cout << response << std::endl;
            responseCallback_(response);

        }


    }

    std::cout << "ControlServer mainLoop exited" << std::endl;
}

    /*
    while ((n = read(getClientFd(), buffer, sizeof(buffer))) > 0) {
        int message_type = buffer[0];

        std::cout << "Received message " << message_type << std::endl;

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

        std::cout << response << std::endl;
        write_all(getClientFd(), response);
    }


}*/

#endif //SERVER_SERVERS_H
