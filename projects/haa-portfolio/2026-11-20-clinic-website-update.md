# HAA 포트폴리오 — 의원 웹사이트 제작 사례 추가

- 작업일: 2026-11-20 (UTC)
- 상태: 포트폴리오 배포 완료
- 포트폴리오: https://jae-kim-haa-portfolio.seong-jae-kim.chatgpt.site/
- 추가한 실제 웹사이트: https://namwonmiraeyeonhap.com/
- 상세 설명: https://jae-kim-haa-portfolio.seong-jae-kim.chatgpt.site/work/growth.html#website

## 반영 내용

홈페이지에 ‘Built since applying’ 프로젝트 블록을 추가했다. 기존 Audience Growth 상세 페이지에도 ‘Website build’ 항목, 비용 설명, 실제 사이트 링크를 추가했다. 기존 디자인과 공개 범위를 유지했다. 영어·한국어·스페인어·일본어·중국어 번역을 모두 추가했다.

사용자 제공 비용을 같은 기간으로 환산했다.

- 기존: 2년에 360,000원 = 연 180,000원
- 현재: 연 약 20,000원
- 연간 절감: 약 160,000원, 약 88.9% (표시: 약 89%)

### 영어 본문

**A clinic website, rebuilt with Claude Code.**

Since submitting my HAA application, I rebuilt Namwon Miraeyeonhap Clinic’s website using Claude Code.

The previous website cost KRW 360,000 every two years. Rebuilding it myself brought the ongoing cost to approximately KRW 20,000 per year—about 89% lower on an annual basis.

### 한국어 본문

**Claude Code로 다시 만든 의원 웹사이트.**

HAA 지원서를 제출한 뒤, Claude Code를 사용해 남원 미래연합의원 웹사이트를 직접 다시 만들었습니다.

기존에는 2년에 36만 원이 들었습니다. 직접 다시 만든 후 연간 운영 비용을 약 2만 원으로 줄였습니다. 연 18만 원에서 약 2만 원으로, 연간 기준 약 89% 절감했습니다.

## 구현 및 검증

- 수정한 원본: `src/pages/index.html`, `src/pages/work/growth.html`
- 번역: `locales/{en,ko,es,ja,zh}.json`의 `clinic_build.*` 키 9개씩
- 기존 번역 빌드로 게시용 HTML과 언어 파일 생성
- 번역 키·서식·참조 검증 통과: 404개 키, 5개 언어
- 런타임 테스트 통과: 테마, 언어 선택, 저장 설정, 로딩 실패 처리 등
- 번역 워크플로 테스트 통과: 변경 감지, 영어 대체, 결정적 빌드 등
- 로컬 링크·앵커·신규 외부 링크·비용 수치 검증 통과
- 화면 검증: 미리보기 서버의 포트 권한 제한(EPERM)으로 미실시
- Sites 소스 커밋: `48154404f069784e4bdfa578ae63a8da51570c49`
- 배포 버전: 10
- 배포 결과: succeeded (2026-11-20 12:17 UTC)

전체 포트폴리오 원본은 기존 Sites 소스 저장소에 커밋했다. 이 GitHub 기록에는 이번에 추가한 내용과 검증 결과를 저장한다.

