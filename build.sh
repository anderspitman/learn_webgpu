#!/bin/bash

gcc \
    -std=c99 \
    -lwgpu_native \
    -lglfw \
    -lOpenGL \
    -L ./wgpu/lib/ \
    -I ./wgpu/include/ \
    -Wl,-rpath,'$ORIGIN/wgpu/lib' \
    -o main \
    main.c
