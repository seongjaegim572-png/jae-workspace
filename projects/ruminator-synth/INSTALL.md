# Ruminator 설치 (FL Studio)

## Mac (가장 쉬운 방법: 내 Mac에서 직접 빌드)
터미널에서:
```
xcode-select --install      # 처음 한 번
brew install cmake          # 처음 한 번
git clone https://github.com/seongjaegim572-png/jae-workspace.git
cd jae-workspace && git checkout claude/blissful-lovelace-frpw52
./projects/build-mac.sh        # 3개 다 (Ruminator, 82 Keys, Halo)
```
빌드 15~25분 걸리고, 끝나면 자동으로 플러그인 폴더에 설치돼요. 그다음 3번(스캔)부터 하면 돼요.

## Mac (빌드된 파일이 있을 때)
1. `Ruminator.vst3` 를 아래 폴더에 복사해요.
   `~/Library/Audio/Plug-Ins/VST3/`
   (AU도 쓰려면 `Ruminator.component` → `~/Library/Audio/Plug-Ins/Components/`)
2. 서명 안 된 플러그인이라 Mac이 막을 수 있어요. 터미널에서 한 번 실행:
   ```
   xattr -dr com.apple.quarantine ~/Library/Audio/Plug-Ins/VST3/Ruminator.vst3
   ```
3. FL Studio → Options → Manage plugins → **Find more plugins** (스캔).
4. Channel rack `+` → **Ruminator** 선택.

## Windows
1. `.vst3` 폴더(Ruminator / 82 Keys / Halo)를 `C:\Program Files\Common Files\VST3\` 에 복사해요.
2. FL Studio → Options → Manage plugins → **Find more plugins**.
3. Channel rack `+` → **Ruminator**.

## 팁
- 프리셋: 플러그인 창 위쪽 드롭다운, 또는 FL 플러그인 메뉴의 preset 목록.
- 노브 더블클릭 = 기본값으로 리셋.
- Macro 1~4, 모든 노브는 FL에서 자동화(automation) 가능해요.
