//
// Created by lucas on 01/07/2026.
//

#ifndef SERVER_RC_UTILS_H
#define SERVER_RC_UTILS_H

#include <string>
#include <span>

void write_all(int sock_fd, const std::string& msg);
void write_all_bytes(int sock_fd, std::span<const std::byte> data);

#endif //SERVER_RC_UTILS_H