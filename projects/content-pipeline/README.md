# 콘텐츠 자동화 파이프라인

X에서 본 구조 그대로: **Collector → Picker → Creator → Scheduler → Reviewer → 다시 소재로**.
브랜드는 2개: `clinic`(미래연합의원), `jae`(82 Jae).

| 단계 | 명령 | 하는 일 |
|---|---|---|
| 1. Collector | `collect` / `add` | 뉴스·Reddit RSS 자동 수집 + 직접 작성·링크·파일 추가 + 경쟁사 수집 |
| 2. Picker | `pick --top 3` | Claude가 0~100점 매겨서 상위만 선별 |
| 3. Creator | `create` → `approve <id>` | 브랜드 말투로 채널별 초안 생성 → **Jae가 검수** |
| 4. Scheduler | `schedule` | 채널별 시간표(`data/schedule.csv`) 생성 |
| 5. Reviewer | `review 성과.csv` | 성과 보고서 + 잘 된 글을 다시 소재함에 넣음(재활용) |

## 시작하기 (5분)
```bash
cd projects/content-pipeline
pip install -r requirements.txt
export ANTHROPIC_API_KEY=...   # 없으면 오프라인 템플릿으로 돌아가요
python pipeline.py add clinic "야간 투석, 직장 다니면서 받을 수 있을까"
python pipeline.py collect
python pipeline.py pick --top 3
python pipeline.py create        # data/drafts/ 에 초안 생성
python pipeline.py approve <id>  # 읽고 고친 뒤
python pipeline.py schedule
python pipeline.py status
```
일주일 뒤: 각 플랫폼 인사이트 숫자를 `examples/metrics-sample.csv` 모양으로 적고 `python pipeline.py review 파일.csv`.

## 바꾸는 곳
- `config/personas.json` — 브랜드 말투, 제목 공식, 금지어, 채널
- `config/sources.json` — 수집할 RSS(구글 뉴스 검색어 바꾸면 주제가 바뀜), 채널별 올릴 시간

## 일부러 안 한 것
- **자동 게시 안 함.** 예약표까지만. 올리는 건 Jae가 직접 (작업 규칙).
- `data/` 폴더는 git에 안 올라가요. 환자 정보가 섞일 수 있어서요.
- 인스타·틱톡·스레드는 공식 RSS가 없어서 경쟁사 계정 자동 수집은 못 해요. 링크를 `add --link`로 넣으면 돼요.

## 상태
- 완료: 5단계 전체 오프라인으로 실행 확인, RSS 6개에서 60개 수집 확인 (2026-11-11)
- 계획: API 키 넣고 AI 선별·초안 품질 확인
