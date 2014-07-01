#!/bin/bash

LINK_PATH="/usr/local/share/applications/doom3.desktop"
BASE_URL="https://github.com/AreaScout/dante-doom3-odroid/raw/gh-pages/config/DoomConfig.cfg"
SHADER_URL="https://github.com/AreaScout/gl2progs.git"
GAME_DATA=$HOME"/.doom3"

set -e

git pull

export PATH=/usr/bin:$PATH
export ARCH=arm-linux-gnueabihf
export CXX=g++
export CC=gcc

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
	\
	BASEFLAGS='-I/usr/include/arm-linux-gnueabihf -I/usr/include' \
	$*
	
if [ ! -d $GAME_DATA ]; then
	mkdir -p $GAME_DATA/{base,demo,d3xp}
fi

CUR_DIR=$PWD

strip doom.arm gamearm-base.so gamearm-d3xp.so 

cp gamearm-base.so $GAME_DATA/base/gamearm.so
cp gamearm-d3xp.so $GAME_DATA/d3xp/gamearm.so

cd $GAME_DATA/base

if [ ! -f $GAME_DATA/base/DoomConfig.cfg ]; then
	echo -e "\nDownload initial DoomConfig.cfg"
	if  ! wget --quiet $BASE_URL ; then
		echo "error: can't get DoomConfig.cfg"
	fi
	cp DoomConfig.cfg ../d3xp/.
	cp DoomConfig.cfg ../demo/.
	echo "Installed it in ~/.doom3/base, ~/.doom3/d3xp and ~/.doom3/demo"
fi

if [ ! -d $GAME_DATA/base/gl2progs ]; then
	echo "Install shaders in ~/.doom3/base/gl2progs"
	git clone --quiet --depth 1 $SHADER_URL
else
	cd $GAME_DATA/base/gl2progs
	git pull
fi

echo -e "\nExecutable (doom.arm) is now available in "$CUR_DIR"\nYou should now copy your *.pk4 files to "$GAME_DATA"/base, "$GAME_DATA"/d3xp or "$GAME_DATA"/demo\n" 

read -p "Do you wish to install and create desktop menu? " -n 1 -r
echo
if [[ ! $REPLY =~ ^[Yy]$ ]]
then
    exit 1
fi

cd /usr/local/share/pixmaps
su -c "wget --quiet -N https://github.com/AreaScout/dante-doom3-odroid/raw/gh-pages/images/doom_3.png | cp $CUR_DIR/doom.arm /usr/local/bin/. | cat <<EOM >$LINK_PATH
[Desktop Entry]
Name=Doom3
Version=1
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
