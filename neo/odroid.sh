#!/bin/bash
set -e

export PATH=/usr/bin:$PATH
export ARCH=arm-linux-gnueabihf
export CXX=g++
export CC=gcc

scons -j5\
	ARCH='arm' \
	BUILD='release' \
	\
	CC=$CC \
	CXX=$CXX \
	\
	NOCURL=1 \
	TARGET_ANDROID=0 \
	TARGET_D3XP=0 \
	\
	BASEFLAGS='-I/usr/include/arm-linux-gnueabihf -I/usr/include' \
	$*
