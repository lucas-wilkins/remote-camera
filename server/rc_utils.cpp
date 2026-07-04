//
// Created by lucas on 01/07/2026.
//

#include "rc_utils.h"

#include <string>
#include <unistd.h>
#include <stdexcept>

/*
 * Helper function for writing strings over TCP
 */
void write_all(int sock_fd, const std::string& msg)
{
    size_t total_sent = 0;

    while (total_sent < msg.size())
    {
        ssize_t n = write(
            sock_fd,
            msg.data() + total_sent,
            msg.size() - total_sent);

        if (n < 0)
        {
            throw std::runtime_error("write failed");
        }

        total_sent += static_cast<size_t>(n);
    }
}