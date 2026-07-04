#!/bin/bash

git pull

# Create a build directory
mkdir server/build

# Configure the project
cd server/build
cmake ..

# Build the project
cmake --build .

cd ~/remote-camera
