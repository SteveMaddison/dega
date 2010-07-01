#!/bin/bash

SAVEDIR=./dir.saved
FBDEV=/dev/fb1

# Did we save a ROM directory last time?
ROMDIR=`cat $SAVEDIR 2> /dev/null`
PNDDIR=`pwd`
if [ -d "$ROMDIR" ] ; then
	cd "$ROMDIR"
else
	cd /media 2> /dev/null || cd /
fi

# Get a ROM file name.
ROM=`zenity --file-selection --title="Select a Master System/Game Gear ROM"`
[ $? -eq 0 ] || exit 1;

# Save ROM's directory.
cd $PNDDIR
dirname "$ROM" > $SAVEDIR

# Identify the ROM and set frame buffer stuff to scale accordingly.
if [ `./dega -i "$ROM"` = "GG" ]; then
	ofbset -fb $FBDEV -pos 160 24 -size 480 432 -mem 414720 -en 1
	fbset -fb $FBDEV -g 160 144 160 144 16
else
	ofbset -fb $FBDEV -pos 144 48 -size 512 384 -mem 786432 -en 1
	fbset -fb $FBDEV -g 256 192 256 192 16
fi

# Run the emulator
op_runfbapp ./dega "\"$ROM\""

# Upon failure, display the contents of the pndrun file.
[ $? -eq 0 ] || zenity --text-info --title="Oops..." --filename=/tmp/pndrundega.out

# Reset frame buffer stuff.
ofbset -fb $FBDEV -pos 0 0 -size 0 0 -mem 0 -en 0
