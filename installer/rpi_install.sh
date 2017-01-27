#!/bin/bash

TheSkyX_Install=~/Library/Application\ Support/Software\ Bisque/TheSkyX\ Professional\ Edition/TheSkyXInstallPath.txt
echo "TheSkyX_Install = $TheSkyX_Install"

if [ ! -f "$TheSkyX_Install" ]; then
    echo TheSkyXInstallPath.txt not found
    exit 1
fi


TheSkyX_Path=$(<"$TheSkyX_Install")
echo "Installing to $TheSkyX_Path"

if [ ! -d "$TheSkyX_Path" ]; then
    echo TheSkyX Install dir not exist
    exit 1
fi

cp "./domelist MaxDomeII.txt" "$TheSkyX_Path/Resources/Common/Miscellaneous Files/"
cp "./maxdomeII.ui" "$TheSkyX_Path/Resources/Common/PlugInsARM32/DomePlugIns/"
cp "./libmaxdomeII.so" "$TheSkyX_Path/Resources/Common/PlugInsARM32/DomePlugIns/"

app_owner=`/usr/bin/stat -c "%u" "$TheSkyX_Path" | xargs id -n -u`
if [ ! -z "$app_owner" ]; then
	chown $app_owner "$TheSkyX_Path/Resources/Common/Miscellaneous Files/domelist MaxDomeII.txt"
	chown $app_owner "$TheSkyX_Path/Resources/Common/PlugInsARM32/DomePlugIns/maxdomeII.ui"
	chown $app_owner "$TheSkyX_Path/Resources/Common/PlugInsARM32/DomePlugIns/libmaxdomeII.so"
fi
chmod  755 "$TheSkyX_Path/Resources/Common/PlugInsARM32/DomePlugIns/libmaxdomeII.so"
