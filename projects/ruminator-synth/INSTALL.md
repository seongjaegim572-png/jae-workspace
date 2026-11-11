# Ruminator 설치 (FL Studio)

## Mac
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
1. `Ruminator.vst3` 폴더를 `C:\Program Files\Common Files\VST3\` 에 복사해요.
2. FL Studio → Options → Manage plugins → **Find more plugins**.
3. Channel rack `+` → **Ruminator**.

## 팁
- 프리셋: 플러그인 창 위쪽 드롭다운, 또는 FL 플러그인 메뉴의 preset 목록.
- 노브 더블클릭 = 기본값으로 리셋.
- Macro 1~4, 모든 노브는 FL에서 자동화(automation) 가능해요.
