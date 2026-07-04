//
// Created by lucas on 01/07/2026.
//

#ifndef SERVER_RC_UTILS_H
#define SERVER_RC_UTILS_H

#include <string>

void write_all(int sock_fd, const std::string& msg);

#endif //SERVER_RC_UTILS_H