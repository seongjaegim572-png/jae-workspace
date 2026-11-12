#!/bin/bash
# Jae 플러그인 3개(Ruminator, 82 Keys, Halo)를 내 Mac에서 빌드하고 설치해요.
# 필요: Xcode Command Line Tools (xcode-select --install), CMake (brew install cmake)
set -e
cd "$(dirname "$0")"
mkdir -p ~/Library/Audio/Plug-Ins/VST3 ~/Library/Audio/Plug-Ins/Components
for p in ruminator-synth keys82 halo-synth; do
  echo "== $p 빌드 중 =="
  cmake -S "$p" -B "$p/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_DEPLOYMENT_TARGET=10.13
  cmake --build "$p/build" --config Release -j 4
  find "$p/build" -type d -name "*.vst3" -path "*Release/VST3*" -maxdepth 5 -exec cp -R {} ~/Library/Audio/Plug-Ins/VST3/ \;
  find "$p/build" -type d -name "*.component" -path "*Release/AU*" -maxdepth 5 -exec cp -R {} ~/Library/Audio/Plug-Ins/Components/ \;
done
echo "설치 완료. FL Studio → Options → Manage plugins → Find more plugins"
