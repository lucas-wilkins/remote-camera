#!/bin/bash

git pull

# Create a build directory
mkdir server/build

# Configure the project
cmake server

# Build the project
cmake --build server
