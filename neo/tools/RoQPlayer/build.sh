#!/bin/bash

CFLAGS="-L/usr/local/lib -L/usr/lib -I../../FFmpeg /usr/local/lib/libavformat.a /usr/local/lib/libavcodec.a /usr/local/lib/libavutil.a /usr/local/lib/libswscale.a /usr/local/lib/libswresample.a -lz -lva -lbz2 -lGLESv1_CM -lX11 -lpthread -lm"
gcc RoQPlayer.c -o RoQPlayer $CFLAGS