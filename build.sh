#!/bin/bash

set -e

CC="${CC:-gcc}"
TARGET="${TARGET:-main}"

echo Start build

${CC} -o gen_shader gen_shader.c && ./gen_shader shader.wgsl

${CC} \
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
    -lm \
    -Wl,-rpath,'$ORIGIN/wgpu/lib' \
    -Wl,-rpath,'$ORIGIN/glfw/lib'

echo Build done
