#!/bin/bash
# Builds radio3ds.cia from radio3ds.elf (run `make` first)
set -e
cd "$(dirname "$0")"
T=/opt/devkitpro/tools/extra
mkdir -p build
$T/bannertool makebanner -i meta/banner.png -a meta/banner.wav -o build/banner.bnr
$T/bannertool makesmdh -s "Radio Neon ラジオネオン" -l "Japanese internet radio / 日本のラジオ" -p "VENOMSHiNE" -i meta/icon.png -o build/icon.smdh
$T/makerom -f cia -o radio3ds.cia -elf radio3ds.elf -rsf meta/app.rsf -icon build/icon.smdh -banner build/banner.bnr -exefslogo -target t -major 1 -minor 0 -micro 0
ls -la radio3ds.cia
