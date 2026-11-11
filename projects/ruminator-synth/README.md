# Ruminator — 웨이브테이블 신스 (VST3 / AU)

Arturia Pigments 느낌의 신스를 직접 만든 것. FL Studio에서 VST3로 써요.
코드는 JUCE (C++). Mac/Windows 빌드는 GitHub Actions가 자동으로 만들어요.

## 들어있는 것
- **OSC 1, 2**: 웨이브테이블 8종 (Basic, PWM, Sync, Harmonic, Formant, Digital, Organ, Fold)
  - Position(모핑), Warp(위상 왜곡), Coarse/Fine, Level, Pan
  - Unison 최대 7 + Detune + Stereo Spread (슈퍼쏘)
  - 앨리어싱 줄인 대역 제한 테이블
- **SUB / NOISE**: 한 옥타브 아래 사인/스퀘어, 화이트 노이즈
- **FILTER**: LP12, LP24, Band, High, Notch + Reso, Drive, Env 양, Keytrack
- **AMP ENV, MOD ENV**: ADSR 2개
- **LFO 1, 2**: Sine/Tri/Saw/Square/S&H, Hz 또는 템포 싱크
- **MOD MATRIX 6칸**: 소스(LFO, Env, Velocity, Mod Wheel, Aftertouch, Keytrack, Random, Macro 1–4) → 18개 목적지
- **MACROS 4개**: FL 자동화용
- **FX**: Distortion, Chorus, Ping-pong Delay (템포 싱크), Reverb
- 16 보이스, Mono/Legato + Glide (808 슬라이드), 서스테인 페달, 피치벤드
- 프리셋 9개: Init, Hyper Supersaw, Digicore Pluck, 808 Slide Bass, Formant Lead, Lo-fi Keys, Dream Pad, Rage Lead, Wobble Bass

## 받는 법
> Mac: GitHub에서 Mac 빌드 서버가 이 계정에 안 열려서(작업이 바로 취소됨) `build-mac.sh` 로 직접 빌드해요. INSTALL.md 참고.

1. GitHub 저장소 → **Actions** → "Ruminator synth build" → 최신 실행 클릭
2. 아래 **Artifacts** 에서 `Ruminator-macOS` 또는 `Ruminator-Windows` 다운로드
3. `INSTALL.md` 대로 설치

`Ruminator-test-output` 에는 프리셋 데모 WAV와 UI 스크린샷이 있어요.

## 직접 빌드 (선택)
```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Ruminator_VST3
```

## 다음 할 일 (계획)
- FL Studio에서 실제로 로드해 보고 소리 확인 (Jae)
- 자기 웨이브테이블(.wav) 불러오기
- 사용자 프리셋 저장 버튼
