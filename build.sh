#!/bin/sh -e

dependencies="glew sdl2 openvr x11 xcomposite xfixes libxdo wayland-client"
includes=$(pkg-config --cflags $dependencies)
libs="$(pkg-config --libs $dependencies) -lm -pthread"
gcc -c src/window_texture.c -O2 -DNDEBUG $includes
gcc -c src/wayland_protocols_stub.c -O2 -DNDEBUG $includes
g++ -c src/wlr_screencopy.cpp -O2 -DNDEBUG $includes
g++ -c src/main.cpp -O2 -DNDEBUG $includes
g++ -o vr-video-player -O2 window_texture.o wayland_protocols_stub.o wlr_screencopy.o main.o -s $libs
