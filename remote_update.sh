#!/bin/bash

git pull

# Create a build directory
mkdir server/build

# Configure the project
cd server/build
cmake ..

# Build the project
if ! cmake --build .; then
    echo "Build failed. Exiting."
    exit 1
fi

cd ~/remote-camera

./server/build/test_capture