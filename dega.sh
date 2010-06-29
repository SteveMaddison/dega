#!/bin/bash

SAVEDIR=./dir.saved
export SDL_VIDEODRIVER=fbcon
export SDL_FBDEV=/dev/fb1

# Did we save a ROM directory last time?
ROMDIR=`cat $SAVEDIR 2> /dev/null`
PNDDIR=`pwd`
if [ -d "$ROMDIR" ] ; then
	cd "$ROMDIR"
else
	cd /media 2> /dev/null || cd /
fi

# Get a ROM file name.
ROM=`zenity --file-selection --title="Select a Mster System/Game Gear ROM"`
[ $? -eq 0 ] || exit 1;

cd $PNDDIR

# Identify the ROM and set frame buffer stuff to scale accordingly.
if [ `./dega -i "$ROM"` = "GG" ]; then
	echo ofbset -fb $SDL_FBDEV -pos 160 24 -size 480 432 -mem 829440 -en 1
	echo fbset -fb $SDL_FBDEV -g 480 432 480 432 16
else
	echo ofbset -fb $SDL_FBDEV -pos 144 48 -size 512 384 -mem 786432 -en 1
	echo fbset -fb $SDL_FBDEV -g 512 384 512 384 16
fi

# Save ROM's directory.
dirname $ROM > $SAVEDIR

# Run the emulator
echo ./dega "$ROM"

# Upon failure, display the contents of the pndrun file.
[ $? -eq 0 ] || zenity --text-info --title="Oops..." --filename=/tmp/pndrundega.out

# Reset frame buffer stuff.
echo ofbset -fb /dev/fb1 -pos 0 0 -size 0 0 -mem 0 -en 0
