#!/bin/bash
CFLAGS="-g -O0 `sdl2-config --cflags`"
LDFLAGS="`sdl2-config --libs`"
if [ `uname` = "Darwin" ]; then
	LDFLAGS="$LDFLAGS -framework OpenGL"
else
	LDFLAGS="$LDFLAGS -lGL -lm"
fi
cc $CFLAGS main.c $LDFLAGS -o snake