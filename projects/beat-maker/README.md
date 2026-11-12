# beat-maker — 코드로 비트 만들기

`make_beat.py` 하나로 오리지널 비트를 만들어요 (numpy만 필요).
- `beat_preview.wav`: 바로 들을 수 있는 믹스 (소리 전부 코드로 합성)
- `chords.mid`, `melody.mid`, `808.mid`, `drums.mid`: FL Studio에 끌어다 놓고 내 플러그인으로 연주

```
python3 make_beat.py output --seed 82 --bpm 145
```
`--seed` 숫자를 바꾸면 멜로디가 새로 나와요.

## 스타일
- `--style chaos` (v2, 160 BPM, E 메이저): 글리치/디스토션 디지코어. 브레이크비트, 클립된 808 슬라이드,
  비트크러시 슈퍼쏘 월, 피치 올린 보컬 찹(합성), 스터터·리버스·테이프스탑, 13~14마디 크러시 스위치업
- `--style trap` (v1, 기본값): 아래 설명

```
python3 make_beat.py output --style chaos --seed 7
```

## trap 스타일 (v1)
멜로딕 트랩 / 디지코어 느낌, 145 BPM, F# 마이너, 16마디
- 1~4마디: 패드 + 멜로디, 필터 열리는 인트로
- 5~16마디: 하프타임 드럼(클랩 3박), 하이햇 롤, 808 슬라이드, 사이드체인

## FL에서 쓰는 법
1. MIDI 4개를 채널랙/플레이리스트로 드래그
2. chords → Halo "Glass Cathedral" 또는 Ruminator "Hyper Supersaw"
3. melody → 82 Keys "Music Box Dream" 또는 Ruminator "Digicore Pluck"
4. 808 → Ruminator "808 Slide Bass" (Mono + Glide)
5. drums → 내 드럼 샘플 (킥 36, 클랩 39, 클로즈드햇 42, 오픈햇 46)
