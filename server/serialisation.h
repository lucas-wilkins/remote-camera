
#ifndef SERVER_SERIALISATION_H
#define SERVER_SERIALISATION_H

#pragma pack(push, 1)
#include <cstdint>

struct ImageDataHeader {
    std::uint32_t image_id;
    std::uint64_t timestamp;
    std::uint32_t bytesused;
    std::int64_t frameDuration;
    std::int64_t exposure;
};

#pragma pack(pop)



#endif //SERVER_SERIALISATION_H