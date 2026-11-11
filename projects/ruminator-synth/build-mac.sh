#!/bin/bash
# Ruminator를 내 Mac에서 직접 빌드하고 설치해요.
# 필요: Xcode Command Line Tools (xcode-select --install), CMake (brew install cmake)
set -e
cd "$(dirname "$0")"
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
cmake --build build --config Release -j 4 --target Ruminator_VST3 Ruminator_AU
VST3=$(find build -type d -name "Ruminator.vst3" | head -n 1)
AU=$(find build -type d -name "Ruminator.component" | head -n 1)
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
rm -rf ~/Library/Audio/Plug-Ins/VST3/Ruminator.vst3 ~/Library/Audio/Plug-Ins/Components/Ruminator.component
cp -R "$VST3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "$AU" ~/Library/Audio/Plug-Ins/Components/
echo "설치 완료. FL Studio → Options → Manage plugins → Find more plugins"
