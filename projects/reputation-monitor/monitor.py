"""미래연합의원 평판 모니터: 네이버 블로그·카페·뉴스·지식iN에서 새 글을 모은다.

사용법:  python3 monitor.py            # 새 글을 out/new_items.json 에 저장, state/seen.json 갱신
        python3 monitor.py --dry-run  # seen.json 을 바꾸지 않음

외부 패키지 없이 표준 라이브러리만 쓴다.
네이버 검색 결과 페이지(공개)를 최신순으로 한 번씩만 읽는다. 일주일에 몇 번 정도만 돌린다.
분석(긍정/부정 판단, 보고서)은 이 스크립트가 아니라 REPORT_PROMPT.md 를 읽은 Claude 가 한다.
"""
import argparse, datetime as dt, html, json, re, sys, time, urllib.parse, urllib.request
from html.parser import HTMLParser
from pathlib import Path

ROOT = Path(__file__).resolve().parent
UA = ("Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 "
      "(KHTML, like Gecko) Chrome/130.0 Safari/537.36")

SEARCH = {  # 최신순 정렬 URL
    "blog": "https://search.naver.com/search.naver?ssc=tab.blog.all&query={q}&nso=so%3Add%2Cp%3Aall",
    "cafe": "https://search.naver.com/search.naver?ssc=tab.cafe.all&query={q}&nso=so%3Add",
    "news": "https://search.naver.com/search.naver?ssc=tab.news.all&where=news&query={q}&sort=1",
    "kin":  "https://search.naver.com/search.naver?ssc=tab.kin.kqna&where=kin&query={q}&kin_sort=2",
}
ITEM_URL = {
    "blog": re.compile(r"^https://(?:m\.)?blog\.naver\.com/[A-Za-z0-9_-]+/\d+"),
    "cafe": re.compile(r"^https://(?:m\.)?cafe\.naver\.com/[A-Za-z0-9_-]+/\d+"),
    "news": re.compile(r"^https?://(?!(?:www\.)?(?:news\.naver\.com/main|help\.naver|search\.naver|media\.naver))"
                       r"[^/]*(?:news|n\.news\.naver\.com|\.co\.kr|\.com|\.kr)/.*(?:article|news|\d{5,})"),
    "kin":  re.compile(r"^https://kin\.naver\.com/qna/detail\.naver\?"),
}
DATE_RE = re.compile(r"(20\d\d)\.\s?(\d{1,2})\.\s?(\d{1,2})\.|(\d+)\s?(분|시간|일|주)\s?전")


class Anchors(HTMLParser):
    """페이지 안의 모든 <a href> 와 그 안의 글자를 모은다."""
    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.out, self._stack = [], []

    def handle_starttag(self, tag, attrs):
        if tag == "a":
            self._stack.append([dict(attrs).get("href") or "", []])

    def handle_data(self, data):
        for a in self._stack:
            a[1].append(data)

    def handle_endtag(self, tag):
        if tag == "a" and self._stack:
            href, parts = self._stack.pop()
            text = re.sub(r"\s+", " ", "".join(parts)).replace("새 창 열림", "").strip()
            self.out.append((href, text))


def norm_url(u):
    u = html.unescape(u)
    if "kin.naver.com/qna/detail" in u:  # docId 만 남긴다
        q = urllib.parse.parse_qs(urllib.parse.urlparse(u).query)
        return f"https://kin.naver.com/qna/detail.naver?d1id={q.get('d1id',[''])[0]}&dirId={q.get('dirId',[''])[0]}&docId={q.get('docId',[''])[0]}"
    u = u.split("#")[0]
    u = re.sub(r"^https://m\.", "https://", u)
    if "n.news.naver.com" in u or "cafe.naver.com" in u or "blog.naver.com" in u:
        u = u.split("?")[0]
    return u


def parse_date(raw, today):
    m = DATE_RE.search(raw)
    if not m:
        return None
    if m.group(1):
        return dt.date(int(m.group(1)), int(m.group(2)), int(m.group(3))).isoformat()
    n, unit = int(m.group(4)), m.group(5)
    days = {"분": 0, "시간": 0, "일": n, "주": 7 * n}[unit]
    return (today - dt.timedelta(days=days)).isoformat()


def fetch(url):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept-Language": "ko-KR,ko;q=0.9"})
    with urllib.request.urlopen(req, timeout=20) as r:
        return r.read().decode("utf-8", "replace")


def extract(channel, page, today, must):
    p = Anchors()
    p.feed(page)
    groups = {}
    for href, text in p.out:
        if not ITEM_URL[channel].match(html.unescape(href)):
            continue
        key = norm_url(href)
        path = key.split("://", 1)[1][:60]
        g = groups.setdefault(key, {"texts": [], "first": page.find(path)})
        if text and text not in g["texts"]:
            g["texts"].append(text)
    items = []
    for url, g in groups.items():
        texts = [t for t in g["texts"] if len(t) > 3]
        if not texts:
            continue
        title = min(texts, key=len) if len(texts) > 1 else texts[0]
        snippet = max(texts, key=len)
        if not any(term in " ".join(texts) for term in must):
            continue  # 검색어가 제목·요약에 없으면 사이드바 광고·다른 뉴스로 보고 버린다
        # 날짜: 링크가 처음 나오는 곳 앞쪽 가까운 글자에서 찾는다
        start = g["first"] if g["first"] >= 0 else 0
        window = re.sub(r"<[^>]+>", " ", page[max(0, start - 2500): start + 2500])
        items.append({"channel": channel, "url": url, "title": title[:200],
                      "snippet": snippet[:400], "date": parse_date(window, today)})
    return items


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--dry-run", action="store_true")
    args = ap.parse_args()

    cfg = json.loads((ROOT / "config.json").read_text())
    seen_path = ROOT / "state" / "seen.json"
    seen = json.loads(seen_path.read_text()) if seen_path.exists() else {}
    today = dt.date.today()
    first_run = not seen

    found, errors = {}, []
    for kw in cfg["keywords"]:
        for ch in cfg["channels"]:
            url = SEARCH[ch].format(q=urllib.parse.quote(kw))
            try:
                page = fetch(url)
            except Exception as e:  # 한 곳이 막혀도 나머지는 계속
                errors.append(f"{ch} / {kw}: {e}")
                continue
            for it in extract(ch, page, today, cfg["must_contain"]):
                it["keyword"] = kw
                it["own"] = any(s in it["url"] for s in cfg["own_sources"])
                found.setdefault(it["url"], it)
            time.sleep(1.5)

    new = [it for u, it in found.items() if u not in seen]
    new.sort(key=lambda x: x["date"] or "", reverse=True)

    out = ROOT / "out"
    out.mkdir(exist_ok=True)
    result = {"run_date": today.isoformat(), "first_run": first_run,
              "total_found": len(found), "new_count": len(new),
              "errors": errors, "items": new}
    (out / "new_items.json").write_text(json.dumps(result, ensure_ascii=False, indent=2))

    if not args.dry_run:
        for u, it in found.items():
            seen.setdefault(u, it["date"] or today.isoformat())
        cutoff = (today - dt.timedelta(days=cfg["seen_keep_days"])).isoformat()
        seen = {u: d for u, d in seen.items() if d >= cutoff}
        seen_path.write_text(json.dumps(dict(sorted(seen.items())), ensure_ascii=False, indent=0))

    by_ch = {}
    for it in new:
        by_ch[it["channel"]] = by_ch.get(it["channel"], 0) + 1
    print(f"찾은 글 {len(found)}개, 새 글 {len(new)}개 {by_ch}" + (" (첫 실행)" if first_run else ""))
    for e in errors:
        print("오류:", e, file=sys.stderr)


if __name__ == "__main__":
    main()
