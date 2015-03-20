#!/bin/bash

CFLAGS="-L/usr/local/lib -L/usr/lib -I../../FFmpeg ../../FFmpeg/libavformat/libavformat.a ../../FFmpeg/libavcodec/libavcodec.a ../../FFmpeg/libavutil/libavutil.a ../../FFmpeg/libswscale/libswscale.a ../../FFmpeg/libswresample/libswresample.a -lz -lva -lbz2 -llzma -lGLESv1_CM -lX11 -lpthread -lm"
gcc RoQPlayer.c -o RoQPlayer $CFLAGS