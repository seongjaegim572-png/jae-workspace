#!/usr/bin/env python3
"""콘텐츠 자동화 파이프라인: Collector -> Picker -> Creator -> Scheduler -> Reviewer

게시는 하지 않아요. 예약표(CSV)까지만 만들고, 올리는 건 Jae가 직접 해요.

사용 예:
  python pipeline.py add clinic "야간 투석 시작, 직장인도 가능"      # 직접 작성
  python pipeline.py add jae --link https://...                      # 링크 입력
  python pipeline.py add jae --file memo.txt                         # 파일 업로드
  python pipeline.py collect                                         # RSS 자동 수집
  python pipeline.py pick --top 3                                    # AI 선별
  python pipeline.py create                                          # 초안 생성
  python pipeline.py approve <id>                                    # Jae 검수 통과
  python pipeline.py schedule                                        # 예약표 만들기
  python pipeline.py review data/metrics.csv                         # 성과 보고서 + 재활용
"""
import argparse
import csv
import datetime as dt
import hashlib
import json
import os
import re
import sys
import urllib.request
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parent
CONFIG = ROOT / "config"
DATA = ROOT / "data"
INBOX = DATA / "inbox.jsonl"
PICKED = DATA / "picked.jsonl"
DRAFTS = DATA / "drafts"
SCHEDULE = DATA / "schedule.csv"
REPORTS = DATA / "reports"

MODEL = "claude-opus-5-5"


# ---------- 공통 ----------

def load_json(path):
    return json.loads(Path(path).read_text(encoding="utf-8"))


def personas():
    return load_json(CONFIG / "personas.json")


def sources():
    return load_json(CONFIG / "sources.json")


def read_jsonl(path):
    if not Path(path).exists():
        return []
    return [json.loads(l) for l in Path(path).read_text(encoding="utf-8").splitlines() if l.strip()]


def write_jsonl(path, rows):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    Path(path).write_text("".join(json.dumps(r, ensure_ascii=False) + "\n" for r in rows), encoding="utf-8")


def make_id(text):
    return hashlib.sha1(text.encode("utf-8")).hexdigest()[:8]


def add_to_inbox(item):
    rows = read_jsonl(INBOX)
    if any(r["id"] == item["id"] for r in rows):
        return False
    rows.append(item)
    write_jsonl(INBOX, rows)
    return True


# ---------- Claude 호출 ----------

_client = None


def ai_available():
    if os.environ.get("PIPELINE_OFFLINE") == "1":
        return False
    try:
        import anthropic  # noqa: F401
    except ImportError:
        return False
    return True


def ask_claude(system, prompt, schema):
    """JSON 스키마로 답을 받아요. 실패하면 None (오프라인 템플릿으로 대체)."""
    global _client
    import anthropic

    if _client is None:
        _client = anthropic.Anthropic()
    try:
        resp = _client.beta.messages.create(
            model=MODEL,
            max_tokens=16000,
            betas=["server-side-fallback-2026-07-01"],
            fallbacks="default",
            output_config={"effort": "medium", "format": {"type": "json_schema", "schema": schema}},
            system=system,
            messages=[{"role": "user", "content": prompt}],
        )
    except anthropic.AuthenticationError:
        print("! API 키가 없어요. ANTHROPIC_API_KEY를 설정하거나 `ant auth login` 하세요. 오프라인 모드로 진행해요.")
        return None
    except anthropic.RateLimitError:
        print("! 요청이 너무 많아요. 잠시 후 다시 해 주세요. 오프라인 모드로 진행해요.")
        return None
    except anthropic.APIStatusError as e:
        print(f"! API 오류 ({e.status_code}): {e.message}. 오프라인 모드로 진행해요.")
        return None
    except anthropic.APIConnectionError:
        print("! 네트워크 오류. 오프라인 모드로 진행해요.")
        return None
    if resp.stop_reason == "refusal":
        print("! Claude가 이 소재는 거절했어요. 건너뛰어요.")
        return None
    text = next((b.text for b in resp.content if b.type == "text"), None)
    return json.loads(text) if text else None


def persona_system(p):
    return (
        f"너는 '{p['name']}'의 콘텐츠 담당자야.\n"
        f"말투: {p['voice']}\n제목 공식: {p['title_formula']}\n독자: {p['audience']}\n"
        f"금지어: {', '.join(p['banned_words']) or '없음'}\n"
        "규칙:\n- " + "\n- ".join(p["rules"])
    )


# ---------- 1. Collector ----------

def fetch_rss(url, limit=10):
    req = urllib.request.Request(url, headers={"User-Agent": "content-pipeline/1.0"})
    with urllib.request.urlopen(req, timeout=15) as r:
        root = ET.fromstring(r.read())
    items = []
    # RSS 2.0
    for it in root.iter("item"):
        items.append({"title": (it.findtext("title") or "").strip(),
                      "link": (it.findtext("link") or "").strip(),
                      "summary": re.sub("<[^>]+>", "", it.findtext("description") or "")[:400].strip()})
    # Atom
    ns = "{http://www.w3.org/2005/Atom}"
    for it in root.iter(f"{ns}entry"):
        link = it.find(f"{ns}link")
        items.append({"title": (it.findtext(f"{ns}title") or "").strip(),
                      "link": link.get("href", "") if link is not None else "",
                      "summary": re.sub("<[^>]+>", "", it.findtext(f"{ns}content") or it.findtext(f"{ns}summary") or "")[:400].strip()})
    return items[:limit]


def cmd_collect(args):
    new = 0
    for feed in sources()["feeds"]:
        try:
            items = fetch_rss(feed["url"])
        except Exception as e:  # 한 피드가 실패해도 나머지는 계속
            print(f"- {feed['name']}: 실패 ({e.__class__.__name__})")
            continue
        added = 0
        for it in items:
            item = {"id": make_id(it["link"] or it["title"]), "persona": feed["persona"],
                    "source": feed["name"], "type": feed["type"], "title": it["title"],
                    "text": it["summary"], "link": it["link"], "status": "new",
                    "collected_at": dt.datetime.now().isoformat(timespec="minutes")}
            if add_to_inbox(item):
                added += 1
        new += added
        print(f"- {feed['name']}: {added}개 새로 수집")
    print(f"완료: 새 소재 {new}개 -> {INBOX.relative_to(ROOT)}")


def cmd_add(args):
    if args.persona not in personas():
        sys.exit(f"페르소나는 {list(personas())} 중 하나예요.")
    if args.file:
        text, title, kind = Path(args.file).read_text(encoding="utf-8"), Path(args.file).stem, "파일"
    elif args.link:
        text, title, kind = args.text or "", args.link, "링크"
    else:
        text, title, kind = args.text, (args.text or "")[:40], "직접 작성"
    if not (text or args.link):
        sys.exit("내용을 넣어 주세요.")
    item = {"id": make_id(title + text), "persona": args.persona, "source": kind, "type": "manual",
            "title": title, "text": text, "link": args.link or "", "status": "new",
            "collected_at": dt.datetime.now().isoformat(timespec="minutes")}
    print("추가했어요:" if add_to_inbox(item) else "이미 있는 소재예요:", item["id"], item["title"])


# ---------- 2. Picker ----------

PICK_SCHEMA = {
    "type": "object",
    "properties": {"scores": {"type": "array", "items": {
        "type": "object",
        "properties": {"id": {"type": "string"}, "score": {"type": "integer"},
                       "reason": {"type": "string"}, "angle": {"type": "string"}},
        "required": ["id", "score", "reason", "angle"], "additionalProperties": False}}},
    "required": ["scores"], "additionalProperties": False,
}


def heuristic_score(item, p):
    text = (item["title"] + " " + item["text"]).lower()
    s = 30 + 10 * sum(t.lower() in text for t in p["topics"])
    if item["type"] in ("manual", "remix"):
        s += 30  # Jae가 직접 고른 것, 성과 좋았던 것 우선
    if any(ch.isdigit() for ch in item["title"]):
        s += 5
    return min(s, 100)


def cmd_pick(args):
    inbox = read_jsonl(INBOX)
    todo = [r for r in inbox if r["status"] == "new"]
    if not todo:
        return print("새 소재가 없어요. collect 또는 add 먼저 해 주세요.")
    ps = personas()
    results = {}
    for pname, p in ps.items():
        group = [r for r in todo if r["persona"] == pname]
        if not group:
            continue
        data = None
        if ai_available():
            listing = "\n".join(f"- id={r['id']} [{r['type']}/{r['source']}] {r['title']} :: {r['text'][:200]}" for r in group)
            data = ask_claude(
                persona_system(p),
                "아래 소재마다 우리 독자에게 조회수·저장이 잘 나올지 0~100점으로 매겨 줘. "
                "type=competitor 는 경쟁사 글이니 베끼지 말고 '우리라면 어떻게 다르게 쓸지' angle을 줘. "
                "type=remix 는 예전에 성과 좋았던 글이니 새로운 각도를 줘. "
                "환자 정보가 들어 있으면 0점.\n\n" + listing,
                PICK_SCHEMA)
        if data:
            for s in data["scores"]:
                results[s["id"]] = s
        else:
            for r in group:
                results[r["id"]] = {"id": r["id"], "score": heuristic_score(r, p),
                                    "reason": "키워드 기반 점수 (오프라인)", "angle": ""}

    ranked = sorted((r for r in todo if r["id"] in results), key=lambda r: -results[r["id"]]["score"])
    chosen = ranked[: args.top]
    picked = read_jsonl(PICKED)
    for r in inbox:
        if r["id"] in results:
            r["score"] = results[r["id"]]["score"]
            r["status"] = "picked" if r in chosen else "skipped"
    for r in chosen:
        picked.append({**r, "reason": results[r["id"]]["reason"], "angle": results[r["id"]]["angle"]})
    write_jsonl(INBOX, inbox)
    write_jsonl(PICKED, picked)
    print(f"선별 완료 (상위 {len(chosen)}개):")
    for r in chosen:
        res = results[r["id"]]
        print(f"  {res['score']:>3}점  {r['id']}  [{r['persona']}] {r['title'][:50]}\n        이유: {res['reason']}")


# ---------- 3. Creator ----------

FORMATS = {
    "threads": "스레드 글 (첫 줄 훅 1줄 + 본문 5~8줄, 500자 이내)",
    "instagram": "인스타 캡션 (훅 + 본문 + 해시태그 5~8개) + 카드뉴스 5장 문구",
    "naver_blog": "네이버 블로그 글 (검색 키워드 들어간 제목, 소제목 3개, 1200~1800자)",
    "youtube_shorts": "숏폼 영상 대본 (0~3초 훅, 30초 이내, 장면별 자막)",
    "tiktok": "틱톡 영상 대본 (0~2초 훅, 20초 이내, 장면별 자막)",
}

CREATE_SCHEMA = {
    "type": "object",
    "properties": {
        "title_options": {"type": "array", "items": {"type": "string"}},
        "posts": {"type": "array", "items": {
            "type": "object",
            "properties": {"platform": {"type": "string"}, "body": {"type": "string"},
                           "image_idea": {"type": "string"}},
            "required": ["platform", "body", "image_idea"], "additionalProperties": False}},
        "check_notes": {"type": "array", "items": {"type": "string"}},
    },
    "required": ["title_options", "posts", "check_notes"], "additionalProperties": False,
}


def offline_draft(item, p):
    t = item["title"]
    return {
        "title_options": [f"{t}, 꼭 알아야 할 3가지", f"{t}? 직접 답해 드립니다", f"{t}, 대부분이 놓치는 것"],
        "posts": [{"platform": pl, "body": f"[{FORMATS[pl]}]\n\n(오프라인 템플릿) 소재: {t}\n{item['text'][:300]}",
                   "image_idea": "주제가 한눈에 보이는 실제 사진 1장"} for pl in p["platforms"]],
        "check_notes": ["API 없이 만든 뼈대예요. 직접 채우거나 API 키 설정 후 다시 create 하세요."],
    }


def banned_hits(text, p):
    return [w for w in p["banned_words"] if w in text]


def cmd_create(args):
    picked = read_jsonl(PICKED)
    todo = [r for r in picked if r.get("status") == "picked"]
    if not todo:
        return print("만들 소재가 없어요. pick 먼저 해 주세요.")
    DRAFTS.mkdir(parents=True, exist_ok=True)
    ps = personas()
    for r in todo:
        p = ps[r["persona"]]
        data = None
        if ai_available():
            fmt = "\n".join(f"- {pl}: {FORMATS[pl]}" for pl in p["platforms"])
            data = ask_claude(
                persona_system(p),
                f"소재: {r['title']}\n내용: {r['text']}\n링크: {r['link']}\n선별 이유: {r.get('reason','')}\n"
                f"추천 각도: {r.get('angle','')}\n\n"
                f"이 소재로 아래 채널별 글을 하나씩 써 줘. 제목 후보는 3개.\n{fmt}\n"
                "check_notes 에는 사람이 올리기 전에 꼭 확인할 것(사실 확인, 광고 규정, 사진 등)을 적어 줘.",
                CREATE_SCHEMA)
        data = data or offline_draft(r, p)

        all_text = " ".join(x["body"] for x in data["posts"]) + " ".join(data["title_options"])
        hits = banned_hits(all_text, p)
        if hits:
            data["check_notes"].insert(0, f"⚠️ 금지어 발견: {', '.join(hits)} — 고쳐야 해요.")

        path = DRAFTS / f"{dt.date.today()}-{r['id']}.md"
        lines = [f"# {r['title']}", "",
                 f"- id: {r['id']}", f"- 브랜드: {p['name']}", "- 상태: 초안 (검수 필요)",
                 f"- 출처: {r['source']} {r['link']}", "",
                 "## 제목 후보", *[f"{i+1}. {t}" for i, t in enumerate(data["title_options"])], ""]
        for post in data["posts"]:
            lines += [f"## {post['platform']}", "", post["body"], "", f"> 사진 아이디어: {post['image_idea']}", ""]
        lines += ["## 올리기 전 확인", *[f"- [ ] {n}" for n in data["check_notes"]], ""]
        path.write_text("\n".join(lines), encoding="utf-8")
        r["status"] = "drafted"
        r["draft"] = str(path.relative_to(ROOT))
        print(f"초안: {r['draft']}")
    write_jsonl(PICKED, picked)
    print("다음: 초안을 읽고 고친 뒤 `python pipeline.py approve <id>` 하세요.")


def cmd_approve(args):
    picked = read_jsonl(PICKED)
    for r in picked:
        if r["id"] == args.id:
            if r.get("status") != "drafted":
                sys.exit(f"이 소재는 지금 '{r.get('status')}' 상태라 승인할 수 없어요.")
            r["status"] = "approved"
            path = ROOT / r["draft"]
            path.write_text(path.read_text(encoding="utf-8").replace("상태: 초안 (검수 필요)", "상태: 검수 완료"), encoding="utf-8")
            write_jsonl(PICKED, picked)
            return print(f"승인했어요: {r['id']} {r['title']}")
    sys.exit("그 id는 없어요.")


# ---------- 4. Scheduler (게시는 안 함, 예약표만) ----------

def cmd_schedule(args):
    picked = read_jsonl(PICKED)
    todo = [r for r in picked if r.get("status") == "approved"]
    if not todo:
        return print("승인된 초안이 없어요. approve 먼저 해 주세요.")
    slots = sources()["schedule_slots"]
    ps = personas()
    rows = []
    if SCHEDULE.exists():
        with SCHEDULE.open(encoding="utf-8") as f:
            rows = list(csv.DictReader(f))
    taken = {(x["date"], x["time"], x["platform"]) for x in rows}
    start = dt.date.today() + dt.timedelta(days=1)
    for r in todo:
        for pl in ps[r["persona"]]["platforms"]:
            day = start
            placed = False
            while not placed:
                for t in slots.get(pl, ["12:00"]):
                    if (day.isoformat(), t, pl) not in taken:
                        taken.add((day.isoformat(), t, pl))
                        rows.append({"date": day.isoformat(), "time": t, "platform": pl, "id": r["id"],
                                     "brand": ps[r["persona"]]["name"], "title": r["title"],
                                     "draft": r["draft"], "posted": "no"})
                        placed = True
                        break
                day += dt.timedelta(days=1)
        r["status"] = "scheduled"
    rows.sort(key=lambda x: (x["date"], x["time"]))
    with SCHEDULE.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=["date", "time", "platform", "id", "brand", "title", "draft", "posted"])
        w.writeheader()
        w.writerows(rows)
    write_jsonl(PICKED, picked)
    print(f"예약표: {SCHEDULE.relative_to(ROOT)} ({len(rows)}줄)")
    for x in rows[-10:]:
        print(f"  {x['date']} {x['time']}  {x['platform']:<15} {x['title'][:40]}")
    print("※ 자동 게시는 하지 않아요. 시간 되면 Jae가 직접 올리고 posted를 yes로 바꿔 주세요.")


# ---------- 5. Reviewer ----------

def engagement(row):
    n = lambda k: float(row.get(k) or 0)  # noqa: E731
    views = max(n("views"), 1)
    # 저장·공유를 더 크게 쳐요 (알고리즘이 좋아하는 신호)
    return (n("likes") + 2 * n("comments") + 3 * n("saves") + 3 * n("shares")) / views * 100


def cmd_review(args):
    with open(args.metrics, encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        return print("성과 데이터가 비어 있어요.")
    for r in rows:
        r["eng"] = engagement(r)
        r["views_n"] = int(float(r.get("views") or 0))
    titles = {r["id"]: r["title"] for r in read_jsonl(PICKED)}
    by_views = sorted(rows, key=lambda r: -r["views_n"])
    by_eng = sorted(rows, key=lambda r: -r["eng"])

    REPORTS.mkdir(parents=True, exist_ok=True)
    today = dt.date.today().isoformat()
    report = REPORTS / f"{today}-report.md"
    out = [f"# 성과 보고서 {today}", "", f"글 {len(rows)}개 · 총 조회 {sum(r['views_n'] for r in rows):,}", "",
           "| id | 채널 | 제목 | 조회 | 반응률 |", "|---|---|---|---|---|"]
    for r in by_views:
        out.append(f"| {r['id']} | {r['platform']} | {titles.get(r['id'], '')[:30]} | {r['views_n']:,} | {r['eng']:.2f}% |")
    out += ["", "## 잘 된 것", f"- 조회 1등: {by_views[0]['id']} ({by_views[0]['views_n']:,})",
            f"- 반응률 1등: {by_eng[0]['id']} ({by_eng[0]['eng']:.2f}%)", "",
            "## 다음 할 일 1개", f"- 반응률 1등 글({by_eng[0]['id']})을 다른 각도로 하나 더 만들기 (재활용 소재로 넣어 둠)", ""]
    report.write_text("\n".join(out), encoding="utf-8")

    # 리믹스: 상위 글을 다시 소재함으로 (파이프라인 순환)
    picked = {r["id"]: r for r in read_jsonl(PICKED)}
    remixed = 0
    seen = set()
    for r in by_eng[: args.remix]:
        if r["id"] in seen or r["id"] not in picked:
            continue  # 파이프라인에서 만든 글만 재활용 (제목을 알아야 하니까)
        seen.add(r["id"])
        src = picked[r["id"]]
        item = {"id": make_id("remix" + r["id"] + today), "persona": src["persona"],
                "source": f"재활용({r['id']})", "type": "remix",
                "title": src["title"],
                "text": f"예전 성과: 조회 {r['views_n']}, 반응률 {r['eng']:.2f}%. 같은 주제, 새 각도로.",
                "link": "", "status": "new", "collected_at": dt.datetime.now().isoformat(timespec="minutes")}
        if add_to_inbox(item):
            remixed += 1
    print(f"보고서: {report.relative_to(ROOT)}")
    print(f"재활용 소재 {remixed}개를 소재함에 다시 넣었어요 -> 다음 pick 때 우선 검토돼요.")


def cmd_status(args):
    inbox, picked = read_jsonl(INBOX), read_jsonl(PICKED)
    count = lambda rows, s: sum(r.get("status") == s for r in rows)  # noqa: E731
    print(f"소재함   new {count(inbox,'new')} / skipped {count(inbox,'skipped')}")
    print(f"제작     picked {count(picked,'picked')} / drafted {count(picked,'drafted')} / "
          f"approved {count(picked,'approved')} / scheduled {count(picked,'scheduled')}")
    print(f"AI       {'켜짐' if ai_available() else '꺼짐 (오프라인 템플릿)'}")


def main():
    ap = argparse.ArgumentParser(description="콘텐츠 자동화 파이프라인")
    sub = ap.add_subparsers(dest="cmd", required=True)

    sub.add_parser("collect", help="1. RSS에서 소재 자동 수집").set_defaults(fn=cmd_collect)
    a = sub.add_parser("add", help="1. 소재 직접 추가 (글/링크/파일)")
    a.add_argument("persona")
    a.add_argument("text", nargs="?")
    a.add_argument("--link")
    a.add_argument("--file")
    a.set_defaults(fn=cmd_add)
    p = sub.add_parser("pick", help="2. AI 선별")
    p.add_argument("--top", type=int, default=3)
    p.set_defaults(fn=cmd_pick)
    sub.add_parser("create", help="3. 브랜드 보이스로 초안 생성").set_defaults(fn=cmd_create)
    ap_ = sub.add_parser("approve", help="3. 사용자 검수 통과")
    ap_.add_argument("id")
    ap_.set_defaults(fn=cmd_approve)
    sub.add_parser("schedule", help="4. 멀티 플랫폼 예약표").set_defaults(fn=cmd_schedule)
    r = sub.add_parser("review", help="5. 성과 보고서 + 재활용")
    r.add_argument("metrics")
    r.add_argument("--remix", type=int, default=2)
    r.set_defaults(fn=cmd_review)
    sub.add_parser("status", help="지금 상태 보기").set_defaults(fn=cmd_status)

    args = ap.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
