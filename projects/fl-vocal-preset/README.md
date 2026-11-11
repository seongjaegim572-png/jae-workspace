# FL Studio 보컬 체인 — "82 Jae Hyperpop Lead"

상태: **초안** (아직 FL에서 귀로 확인 안 함)

목표 사운드: kimj, 2hollis, jane remover, brakence 느낌.
밝고, 앞에 붙고, 살짝 깨진 디지털 보컬.

모두 FL 기본 플러그인만 사용. 믹서 트랙 1개에 순서대로 넣는다.

---

## 체인 (위에서 아래 순서)

### 1. Pitcher (오토튠 역할)
- Key: 곡 키로 설정 (예: C# minor)
- Speed: **0~5%** (빠를수록 하드튠. hyperpop은 거의 0)
- Correction: 100%
- Formant: 켬
- Gender: 50 (기본). 더 "애니/칩멍크" 느낌은 55~60

### 2. Fruity Parametric EQ 2 (정리)
- Band 1 High Pass: **100 Hz**, 기울기 가파르게
- Band 3 Bell: **250~350 Hz -3 dB**, Q 넓게 (먹먹함 제거)
- Band 5 Bell: **3~4 kHz +2 dB** (가사 잘 들리게)
- Band 7 High Shelf: **10 kHz +3 dB** (공기감)

### 3. Fruity Limiter → COMP 모드 (1차 압축)
- Threshold: **-18 dB** 근처 (게인 리덕션 4~6 dB 보이게)
- Ratio: **4:1**
- Knee: 중간
- Attack: **5 ms**
- Release: **60 ms**
- Gain: 줄어든 만큼 다시 올림

### 4. 치찰음 정리 (Parametric EQ 2 하나 더)
- 6~8 kHz 에서 "ㅅ, ㅊ" 소리 튀면 **-3 dB** 좁게

### 5. Fruity Fast Dist (디지털 질감, hyperpop 핵심)
- Type: A
- Pre: 20~30%
- Threshold: 80% 이상
- **Mix: 10~20%** (많이 넣으면 망가짐)

### 6. Fruity Limiter (2차, 피크만 잡기)
- Ceiling: **-1 dB**
- Threshold: 게인 리덕션 1~2 dB만

### 7. Send 트랙 (보컬 트랙에서 보내기)
**Send A — 딜레이 (Fruity Delay 3)**
- Time: **1/4 또는 1/8 dotted**, Tempo sync 켬
- Feedback: 25%
- Filter: Low cut 300 Hz, High cut 6 kHz
- Wet 100% (Send 양으로 조절)

**Send B — 리버브 (Fruity Reeverb 2)**
- Room size: 40~60
- Decay: **1.2~1.8 s**
- Low cut: 300 Hz
- Wet 100% (Send 양으로 조절)

---

## 저장 방법 (Jae가 FL에서 직접)
1. 믹서 트랙에 위 체인 다 넣기
2. 믹서 트랙 이름 우클릭 → **File → Save mixer track state as...**
3. 이름: `82 Jae Hyperpop Lead.fst`
4. 다음부터 빈 트랙에 드래그하면 바로 불러와짐

## 확인할 것 (아직 안 함)
- [ ] 실제 내 목소리 녹음에 걸어 보기
- [ ] Pitcher Speed / Fast Dist Mix 귀로 조정
- [ ] 레퍼런스 곡과 A/B 비교
