#!/bin/bash

gcc \
    -std=c99 \
    -lwgpu_native \
    -lglfw \
    -L ./wgpu/lib/ \
    -I ./wgpu/include/ \
    -Wl,-rpath,'$ORIGIN/wgpu/lib' \
    -g \
    -O0 \
    -o main \
    main.c
