#!/bin/bash
# Builds RadioNeon.cia from RadioNeon.elf (run `make` first)
# Tools: bannertool + makerom in $TOOLS (default: devkitPro tools/extra, or on the PATH)
set -e
cd "$(dirname "$0")"
VER_MAJOR=1; VER_MINOR=1; VER_MICRO=0
T=${TOOLS:-/opt/devkitpro/tools/extra}
BT=$T/bannertool; MR=$T/makerom
[ -x "$BT" ] || BT=bannertool
[ -x "$MR" ] || MR=makerom
mkdir -p build
$BT makebanner -i meta/banner.png -a meta/banner.wav -o build/banner.bnr
$BT makesmdh -s "Radio Neon" -l "Japanese internet radio / 日本のラジオ" -p "VENOMSHiNE" -i meta/icon.png -o build/icon.smdh
$MR -f cia -o RadioNeon.cia -elf RadioNeon.elf -rsf meta/app.rsf -icon build/icon.smdh -banner build/banner.bnr \
    -exefslogo -target t -major $VER_MAJOR -minor $VER_MINOR -micro $VER_MICRO
md5sum RadioNeon.cia RadioNeon.3dsx
