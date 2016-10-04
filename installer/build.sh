#!/bin/bash

mkdir -p ROOT/tmp/maxdomeII_X2/
cp "../maxdomeII.ui" ROOT/tmp/maxdomeII_X2/
cp "../maxdomeII.png" ROOT/tmp/maxdomeII_X2/
cp "../domelist MaxDomeII.txt" ROOT/tmp/maxdomeII_X2/
# cp "../build/Release/libmaxdomeII.dylib" ROOT/tmp/maxdomeII_X2/

if [ ! -z "$installer_signature" ]; then
# signed package using env variable installer_signature
pkgbuild --root ROOT --identifier org.rti-zone.maxdomeII_X2 --sign "$installer_signature" --scripts Scritps --version 1.0 maxdomeII_X2.pkg
pkgutil --check-signature ./maxdomeII_X2.pkg
else
pkgbuild --root ROOT --identifier org.rti-zone.maxdomeII_X2 --scripts Scritps --version 1.0 maxdomeII_X2.pkg
fi

rm -rf ROOT
