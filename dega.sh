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

# Get video settings
STRETCH=0
if [ -f videorc ] ; then
	. videorc
fi

# Identify the ROM and set frame buffer stuff to scale accordingly.
if [ `./dega -i "$ROM"` = "GG" ]; then
	case "$STRETCH" in
		1)	# Stretch, but maintain aspect ratio
			ofbset -fb $FBDEV -pos 133 0 -size 534 480 -mem 92160 -en 1 ;;
		2)	# Fill entire screen
			ofbset -fb $FBDEV -pos 0 0 -size 800 480 -mem 92160 -en 1 ;;
		*)	# Standard integer scaling
			ofbset -fb $FBDEV -pos 160 24 -size 480 432 -mem 92160 -en 1 ;;
	esac
	fbset -fb $FBDEV -g 160 144 160 288 16
else
	case "$STRETCH" in
		1)	# Stretch, but maintain aspect ratio
			ofbset -fb $FBDEV -pos 80 0 -size 640 480 -mem 196608 -en 1 ;;
		2)	# Fill entire screen
			ofbset -fb $FBDEV -pos 0 0 -size 800 480 -mem 196608 -en 1 ;;
		*)	# Standard integer scaling
			ofbset -fb $FBDEV -pos 144 48 -size 512 384 -mem 196608 -en 1 ;;
	esac
	fbset -fb $FBDEV -g 256 192 256 384 16
fi

# Run the emulator
op_runfbapp ./dega "$ROM"

# Upon failure, display the contents of the pndrun file.
[ $? -eq 0 ] || zenity --text-info --title="Oops..." --filename=/tmp/pndrundega.out

# Reset frame buffer stuff.
ofbset -fb $FBDEV -pos 0 0 -size 0 0 -mem 0 -en 0
