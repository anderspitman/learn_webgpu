#!/bin/bash

set -e

TARGET="${TARGET:-main}"

gcc \
    -std=c99 \
    -Wall \
    -g \
    -O0 \
    -I ./wgpu/include/ \
    -I ./glfw/include/ \
    -o ${TARGET} \
    ${TARGET}.c \
    -L ./wgpu/lib/ \
    -L ./glfw/lib/ \
    -lwgpu_native \
    -lglfw \
    -Wl,-rpath,'$ORIGIN/wgpu/lib' \
    -Wl,-rpath,'$ORIGIN/glfw/lib'

echo Build done
