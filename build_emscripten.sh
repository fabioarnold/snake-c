#!/bin/bash
source ~/.emsdk/emsdk_env.sh
emcc -Os -s USE_SDL=2 -s NO_FILESYSTEM=1 main.c -o snake.js