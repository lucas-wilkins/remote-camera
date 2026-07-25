cmake_minimum_required(VERSION 3.1)
project(server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Only needed for windows development
set(LIBCAMERA_SOURCE_DIR
        "/mnt/d/git_stuff/libcamera")


# Find libcamera headers, or no tool, set found to false
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(LIBCAMERA libcamera)
else()
    set(LIBCAMERA_FOUND, FALSE)
endif()

if(NOT LIBCAMERA_FOUND)

    if(EXISTS "/proc/device-tree/model")
        file(READ "/proc/device-tree/model" RPI_MODEL)

        if(RPI_MODEL MATCHES "Raspberry Pi")
            message(FATAL_ERROR "This is trying to build on Raspberry Pi, but has fallen back the windows version")
        endif()
    endif()

    set(LIBCAMERA_INCLUDE_DIRS
            "${LIBCAMERA_SOURCE_DIR}/build/include"
            "${LIBCAMERA_SOURCE_DIR}/include"
            "${LIBCAMERA_SOURCE_DIR}/src"
    )

    # If you're not actually linking on Windows, this can be empty
    set(LIBCAMERA_LIBRARIES "")
endif()

add_executable(server
        main.cpp
        rc_utils.cpp
        rc_utils.h
        constants.h
        servers.h
)


add_executable(test_capture
        capture_example.cpp
)

# Extra stuff for server

target_include_directories(server PRIVATE
        ${LIBCAMERA_INCLUDE_DIRS}
)

target_link_directories(server PRIVATE
        ${LIBCAMERA_LIBRARY_DIRS}
)

target_link_libraries(server PRIVATE
        ${LIBCAMERA_LIBRARIES}
)

target_compile_options(server PRIVATE
        ${LIBCAMERA_CFLAGS_OTHER}
)


# Extra stuff for test_capture

target_include_directories(test_capture PRIVATE
        ${LIBCAMERA_INCLUDE_DIRS}
)

target_link_directories(test_capture PRIVATE
        ${LIBCAMERA_LIBRARY_DIRS}
)

target_link_libraries(test_capture PRIVATE
        ${LIBCAMERA_LIBRARIES}
)

target_compile_options(test_capture PRIVATE
        ${LIBCAMERA_CFLAGS_OTHER}
)