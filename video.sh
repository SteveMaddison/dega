#!/bin/sh

MODE=`zenity --list --title "Dega display options" \
	--column "" --column "Display Mode" \
	0 "Normal" \
	1 "Full screen, maintain aspect ratio" \
	2 "Full screen, stretch to fill"`
[ $? = 0 ] || exit 1

echo "STRETCH=$MODE" > videorc
