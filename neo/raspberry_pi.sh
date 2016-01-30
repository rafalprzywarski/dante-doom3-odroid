#!/bin/bash

LINK_PATH="/usr/local/share/applications/doom3.desktop"
FFMPEG_URL="https://github.com/FFmpeg/FFmpeg.git"
GAME_DATA=$HOME"/.doom3"
CUR_DIR=$PWD

set -e

#git pull

export PATH=/usr/bin:$PATH
export ARCH=arm-linux-gnueabihf
export CXX=g++
export CC=gcc

#read -p "Do you wish to build doom3 to play video with ffmpeg decoder ?" -n 1 -r
#echo
#if [[ $REPLY =~ ^[Yy]$ ]]
#then
#    export USE_FFMPEG=1
#	if [ ! -d $CUR_DIR/FFmpeg ]; then
#		echo -e "\nOk downloading FFmpeg"
#		git clone --depth 1 $FFMPEG_URL
#		cd $CUR_DIR/FFmpeg
#		echo -e "\nBuild FFmpeg, this can take a while"
#		./configure --disable-programs --enable-neon --enable-pthreads
#		make -j5 V=0
#		echo -e "\nBuild successfully finished please identify yourself to install FFmpeg"
#		sudo "make install"
#		cd ..
#	fi
#else
#
	export USE_FFMPEG=0
#fi

#	BUILD='release' \

scons -j5 \
	ARCH='arm' \
	BUILD='release' \
	\
	CC=$CC \
	CXX=$CXX \
	\
	NOCURL=1 \
	TARGET_ANDROID=0 \
	TARGET_D3XP=1 \
	TARGET_DEMO=0 \
	USE_FFMPEG=$USE_FFMPEG \
	\
	BASEFLAGS='-I/usr/include/arm-linux-gnueabihf -I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -DRASPBERRY_PI -L/opt/vc/lib ' \
	CORECPPFLAGS='-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -DRASPBERRY_PI ' \
	GAMECPPFLAGS='-I/opt/vc/include -I/opt/vc/include/interface/vcos/pthreads -I/opt/vc/include/interface/vmcs_host/linux -DRASPBERRY_PI ' \
	BASELINKFLAGS='-L/opt/vc/lib ' \
	CORELINKFLAGS='-L/opt/vc/lib -lbcm_host -lvchiq_arm ' \
	$*

# Don't care if the directories still exist or not
mkdir -p $GAME_DATA/{base,demo,d3xp}

strip doom.arm gamearm-base.so gamearm-d3xp.so

cp gamearm-base.so $GAME_DATA/base/gamearm.so
cp gamearm-d3xp.so $GAME_DATA/d3xp/gamearm.so
if [ TARGET_DEMO == 1 ]; then
	cp gamearm-demo.so $GAME_DATA/demo/gamearm.so
fi

echo "Install shaders in ~/.doom3/base/gl2progs"
mkdir -p $GAME_DATA/base/gl2progs
cp -r gl2progs/* $GAME_DATA/base/gl2progs/

cp config/DoomConfig.cfg $GAME_DATA/base/
cp config/DoomConfig.cfg $GAME_DATA/d3xp/
cp config/DoomConfig.cfg $GAME_DATA/demo/
echo "Installed DoomConfig.cfg in ~/.doom3/base, ~/.doom3/d3xp and ~/.doom3/demo"

echo -e "\nExecutable (doom.arm) is now available in "$CUR_DIR"\nYou should now copy your *.pk4 files to "$GAME_DATA"/base, "$GAME_DATA"/d3xp or "$GAME_DATA"/demo\n"
exit

cd $GAME_DATA/base

read -p "Do you wish to install and create desktop menu ? " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    exit 1
fi

cd /usr/local/share/pixmaps
su -c "wget --quiet -N https://github.com/AreaScout/dante-doom3-odroid/raw/gh-pages/images/doom_3.png | cp $CUR_DIR/doom.arm /usr/local/bin/. | cat <<EOM >$LINK_PATH
[Desktop Entry]
Name=Doom3
Version=1.3
Exec=doom.arm
Comment=Doom3
Icon=doom_3
Type=Application
Terminal=false
StartupNotify=true
Encoding=UTF-8
Categories=Game;ActionGame;
EOM"

echo -e "\nFinish, have fun !"

exit
