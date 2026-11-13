#!/usr/bin/env python3
"""미래연합의원 홈페이지 만들기.

사용법:  python3 build.py
- 공통 정보(전화, 주소, 진료시간, 원장 이력 등)는 site.json 에서 고칩니다.
- 페이지 글은 _src/*.html 에서 고칩니다.
- 실행하면 index.html, 각 폴더의 index.html, sitemap.xml, robots.txt, llms.txt 가 새로 만들어집니다.
  (만들어진 파일은 직접 고치지 마세요. 다음 실행 때 덮어써집니다.)
- dist/ 폴더에 인터넷에 올릴 파일만 모입니다. Cloudflare Pages 는 이 폴더만 올립니다.
"""
import html
import json
import re
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent
S = json.loads((ROOT / "site.json").read_text(encoding="utf-8"))
D = S["domain"].rstrip("/")
TEL = "tel:" + S["phone"].replace("-", "")
TEL2 = "tel:" + S["phone2"].replace("-", "")
TELD = "tel:" + S["phone_dialysis"].replace("-", "")

NAV = [
    ("dialysis", "/dialysis/", "인공신장실(투석)"),
    ("inpatient", "/inpatient/", "입원"),
    ("pediatrics", "/pediatrics/", "소아청소년과"),
    ("chronic", "/chronic-care/", "고혈압·당뇨"),
    ("pt", "/physical-therapy/", "도수·물리치료"),
    ("location", "/#location", "진료시간·오시는 길"),
]

# 가변 글꼴 + 필요한 글자 조각만 받는 방식 (전체 글꼴 약 3MB → 약 0.4MB)
FONT = "https://cdn.jsdelivr.net/gh/orioncactus/pretendard@v1.3.9/dist/web/variable/pretendardvariable-dynamic-subset.min.css"


def esc(s):
    return html.escape(s, quote=True)


def clinic_node():
    return {
        "@type": ["MedicalClinic", "LocalBusiness"],
        "@id": f"{D}/#clinic",
        "name": S["name"],
        "alternateName": [S["site_name"], S["name_en"]],
        "url": f"{D}/",
        "telephone": "+82-" + S["phone"][1:],
        "image": f"{D}/assets/og-cover-v4.jpg",
        "logo": f"{D}/assets/favicon.svg",
        "description": (
            f"{S['road_address']}에 있는 의원. 1층 인공신장실(혈액투석), 소아청소년과 전문의 진료, "
            "고혈압·당뇨·고지혈증 만성질환 관리, 도수·물리치료, 29병상 입원 치료, 예방접종을 합니다."
        ),
        "address": {
            "@type": "PostalAddress",
            "streetAddress": S["street"],
            "addressLocality": S["locality"],
            "addressRegion": S["region"],
            "addressCountry": "KR",
        },
        "geo": {"@type": "GeoCoordinates", "latitude": S["lat"], "longitude": S["lng"]},
        "hasMap": S["naver_map"],
        "areaServed": S["area_served"],
        "medicalSpecialty": ["Pediatric", "Renal", "PrimaryCare", "PhysicalTherapy"],
        "availableService": [
            {"@type": "MedicalTherapy", "name": "혈액투석 (인공신장실)", "url": f"{D}/dialysis/"},
            {"@type": "MedicalProcedure", "name": "소아청소년 진료, 영유아 검진, 예방접종", "url": f"{D}/pediatrics/"},
            {"@type": "MedicalTherapy", "name": "고혈압·당뇨·고지혈증 관리", "url": f"{D}/chronic-care/"},
            {"@type": "MedicalTherapy", "name": "도수치료·물리치료", "url": f"{D}/physical-therapy/"},
            {"@type": "MedicalTherapy", "name": "입원 치료", "url": f"{D}/inpatient/"},
            {"@type": "MedicalProcedure", "name": "독감 예방접종"},
        ],
        "openingHoursSpecification": [
            {"@type": "OpeningHoursSpecification", "dayOfWeek": h["days"], "opens": h["opens"], "closes": h["closes"]}
            for h in S["hours"]
        ],
        "paymentAccepted": S["payment"],
        "amenityFeature": [
            {"@type": "LocationFeatureSpecification", "name": a, "value": True} for a in S["amenities"]
        ],
        "employee": {"@id": f"{D}/#doctor"},
        "sameAs": [S["blog"], S["instagram"], S["threads"], S["naver_place"]],
    }


def doctor_node():
    d = S["doctor"]
    return {
        "@type": "Physician",
        "@id": f"{D}/#doctor",
        "name": d["name"],
        "jobTitle": d["title"],
        "medicalSpecialty": "Pediatric",
        "description": " / ".join(d["career"]),
        "alumniOf": {"@type": "CollegeOrUniversity", "name": "인하대학교 의과대학"},
        "worksFor": {"@id": f"{D}/#clinic"},
        "sameAs": [S["facebook_doctor"]],
    }


# ---------- 화면 조각 ----------

def hours_table():
    rows = "".join(
        f"<tr><th>{esc(h['label'])}</th><td>{h['opens']} ~ {h['closes']}<br><span class=\"notice\">{esc(h['note'])}</span></td></tr>"
        for h in S["hours"]
    )
    rows += "<tr><th>일요일</th><td>휴진</td></tr>"
    rows += "<tr><th>공휴일</th><td>휴진</td></tr>"
    return f'<div class="table-wrap"><table>{rows}</table></div>'


def location_table():
    return (
        '<div class="table-wrap"><table>'
        f"<tr><th>주소</th><td>{esc(S['road_address'])} {esc(S['name'])}<br><span class=\"notice\">지번: {esc(S['jibun_address'])}</span></td></tr>"
        f"<tr><th>전화</th><td>대표 <a href=\"{TEL}\">{S['phone']}</a><br>직통 <a href=\"{TEL2}\">{S['phone2']}</a><br>투석 상담 <a href=\"{TELD}\">{S['phone_dialysis']}</a></td></tr>"
        f"<tr><th>주차</th><td>{esc(S['parking'])}</td></tr>"
        f"<tr><th>대중교통</th><td>{esc(S['transit'])}</td></tr>"
        f"<tr><th>결제</th><td>{esc(S['payment'])}</td></tr>"
        "</table></div>"
    )


def doctor_list():
    return '<ul class="list">' + "".join(f"<li>{esc(c)}</li>" for c in S["doctor"]["career"]) + "</ul>"


def equipment_list():
    return '<ul class="list">' + "".join(f"<li>{esc(e)}</li>" for e in S["equipment"]) + "</ul>"


def header(active):
    links = "".join(
        f'<a href="{href}"{" aria-current=\"page\"" if key == active else ""}>{label}</a>' for key, href, label in NAV
    )
    return f"""<header class="top">
  <div class="wrap">
    <a class="logo" href="/"><img src="/assets/favicon.svg" alt="" width="30" height="30"><b>미래<em>연합</em><i>의원</i></b></a>
    <nav class="nav" aria-label="주요 메뉴">{links}</nav>
    <a class="call-btn" href="{TEL}">{S['phone']}</a>
  </div>
</header>"""


def footer():
    menu = "".join(f'<p><a href="{href}">{label}</a></p>' for _, href, label in NAV[:5])
    return f"""<footer>
  <div class="wrap">
    <div class="cols">
      <div>
        <h4>{S['name']}</h4>
        <p>대표자: {S['doctor']['name']}</p>
        <p>주소: {S['road_address']}</p>
        <p>전화: <a href="{TEL}">{S['phone']}</a></p>
        <p>진료: 평일 08:40~17:50 · 토 08:30~13:00 · 일요일·공휴일 휴진</p>
      </div>
      <div><h4>진료 안내</h4>{menu}</div>
      <div>
        <h4>소식</h4>
        <p><a href="{S['blog']}" rel="noopener">네이버 블로그</a></p>
        <p><a href="{S['instagram']}" rel="noopener">인스타그램</a></p>
        <p><a href="{S['threads']}" rel="noopener">스레드</a></p>
        <p><a href="{S['facebook_doctor']}" rel="noopener">원장님 페이스북</a></p>
        <p><a href="{S['naver_place']}" rel="noopener">네이버 플레이스</a></p>
      </div>
    </div>
    <p class="kw">남원 병원 · 남원 의원 · 남원 투석 · 남원 인공신장실 · 남원 소아과 · 남원 소아청소년과 · 남원 내과 · 남원 고혈압 · 남원 당뇨 · 남원 도수치료 · 남원 물리치료 · 남원 입원 · 남원 교통사고 입원 · 남원 예방접종 · 임실 투석 · 순창 투석 · 장수 투석 · 구례 투석 · 곡성 투석 · 함양 투석 · 남원 인근 입원</p>
    <p class="kw">이 홈페이지의 건강 정보는 일반 안내입니다. 정확한 진단과 치료는 진료를 통해 받으세요. 최종 검토: {S['last_reviewed']}</p>
  </div>
</footer>
<div class="mobile-cta">
  <a class="tel" href="{TELD}">투석 상담</a>
  <a class="tel2" href="{TEL}">입원·진료 전화</a>
  <a class="map-link" href="{S['naver_map']}" rel="noopener">길찾기</a>
</div>"""


def faq_html(faq, title):
    items = "".join(
        f'<details{" open" if i == 0 else ""}><summary>{esc(f["q"])}</summary><div class="a"><p>{esc(f["a"])}</p></div></details>'
        for i, f in enumerate(faq)
    )
    return f'<section id="faq"><div class="wrap"><h2>{esc(title)}</h2>{items}</div></section>'


def fill(text):
    rep = {
        "{{phone}}": S["phone"], "{{phone2}}": S["phone2"], "{{tel}}": TEL, "{{tel2}}": TEL2, "{{phone_dialysis}}": S["phone_dialysis"], "{{tel_dialysis}}": TELD,
        "{{naver_map}}": S["naver_map"], "{{kakao_map}}": S["kakao_map"], "{{blog}}": S["blog"],
        "{{address}}": S["road_address"], "{{hours_table}}": hours_table(), "{{location_table}}": location_table(),
        "{{doctor_list}}": doctor_list(), "{{equipment_list}}": equipment_list(), "{{parking}}": S["parking"],
        "{{doctor_name}}": S["doctor"]["name"],
        "{{doctor_photo}}": (
            f'<img class="photo" src="/assets/img/doctor.jpg" alt="{S["name"]} {S["doctor"]["name"]} 원장" width="860" height="830" loading="lazy">'
            if (ROOT / "assets/img/doctor.jpg").exists() else '<div class="photo">원장님 사진<br>(assets/img/doctor.jpg)</div>'
        ),
    }
    for k, v in rep.items():
        text = text.replace(k, v)
    left = re.findall(r"\{\{\w+\}\}", text)
    if left:
        raise SystemExit(f"채워지지 않은 자리: {left}")
    return text


def render(src: Path):
    raw = src.read_text(encoding="utf-8")
    m = re.match(r"\s*<!--META\s*(\{.*?\})\s*META-->\s*", raw, re.S)
    meta = json.loads(m.group(1))
    body = fill(raw[m.end():])
    path = meta["path"]
    url = D + path
    faq = meta.get("faq", [])

    graph = [clinic_node(), doctor_node()]
    if path == "/":
        graph.append({"@type": "WebSite", "@id": f"{D}/#website", "url": f"{D}/", "name": S["site_name"],
                      "inLanguage": "ko-KR", "publisher": {"@id": f"{D}/#clinic"}})
    else:
        page = {"@type": "MedicalWebPage", "url": url, "name": meta["title"].split("|")[0].strip(),
                "inLanguage": "ko-KR", "lastReviewed": S["last_reviewed"],
                "reviewedBy": {"@id": f"{D}/#doctor"}, "mainEntity": {"@id": f"{D}/#clinic"}}
        if meta.get("about"):
            page["about"] = meta["about"]
        graph.append(page)
        graph.append({"@type": "BreadcrumbList", "itemListElement": [
            {"@type": "ListItem", "position": 1, "name": "홈", "item": f"{D}/"},
            {"@type": "ListItem", "position": 2, "name": meta["crumb"], "item": url}]})
    if faq:
        graph.append({"@type": "FAQPage", "mainEntity": [
            {"@type": "Question", "name": f["q"], "acceptedAnswer": {"@type": "Answer", "text": f["a"]}} for f in faq]})
    ld = json.dumps({"@context": "https://schema.org", "@graph": graph}, ensure_ascii=False, indent=1)

    verify = ""
    if S.get("naver_verification"):
        verify += f'<meta name="naver-site-verification" content="{esc(S["naver_verification"])}">\n'
    if S.get("google_verification"):
        verify += f'<meta name="google-site-verification" content="{esc(S["google_verification"])}">\n'

    crumb = "" if path == "/" else f'<div class="wrap breadcrumb"><a href="/">홈</a> › {esc(meta["crumb"])}</div>'
    faq_block = faq_html(faq, meta.get("faq_title", "자주 묻는 질문")) if faq else ""

    out = f"""<!doctype html>
<html lang="ko">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{esc(meta['title'])}</title>
<meta name="description" content="{esc(meta['description'])}">
<meta name="keywords" content="{esc(meta.get('keywords', ''))}">
<link rel="canonical" href="{url}">
<meta name="robots" content="index,follow,max-image-preview:large">
{verify}<meta property="og:type" content="{'website' if path == '/' else 'article'}">
<meta property="og:site_name" content="{esc(S['site_name'])}">
<meta property="og:title" content="{esc(meta.get('og_title', meta['title']))}">
<meta property="og:description" content="{esc(meta.get('og_description', meta['description']))}">
<meta property="og:url" content="{url}">
<meta property="og:image" content="{D}/assets/og-cover-v4.jpg">
<meta property="og:image:width" content="1200">
<meta property="og:image:height" content="630">
<meta property="og:locale" content="ko_KR">
<meta name="twitter:card" content="summary_large_image">
<meta name="geo.region" content="KR-13">
<meta name="geo.placename" content="{S['locality']}">
<meta name="geo.position" content="{S['lat']};{S['lng']}">
<meta name="theme-color" content="#1b3f7a">
<link rel="icon" href="/assets/favicon.svg" type="image/svg+xml">
<link rel="preconnect" href="https://cdn.jsdelivr.net">
<link rel="stylesheet" href="{FONT}">
<link rel="stylesheet" href="/assets/style.css">
<script type="application/ld+json">
{ld}
</script>
</head>
<body>
{header(meta.get('nav', ''))}
<main>
{crumb}
{body}
{faq_block}
</main>
{footer()}
</body>
</html>
"""
    dest = ROOT / ("index.html" if path == "/" else path.strip("/") + "/index.html")
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text(out, encoding="utf-8")
    return meta


def main():
    metas = [render(p) for p in sorted((ROOT / "_src").glob("*.html"))]
    metas.sort(key=lambda m: (m["path"] != "/", m["path"]))

    urls = "".join(f"  <url><loc>{D}{m['path']}</loc><lastmod>{S['last_reviewed']}</lastmod></url>\n" for m in metas)
    (ROOT / "sitemap.xml").write_text(
        f'<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n{urls}</urlset>\n',
        encoding="utf-8")

    bots = ["GPTBot", "OAI-SearchBot", "ChatGPT-User", "ClaudeBot", "Claude-SearchBot", "PerplexityBot", "Google-Extended", "Yeti", "Daum"]
    robots = "User-agent: *\nAllow: /\n\n# AI 검색·네이버·다음: 답변에 인용될 수 있게 허용\n"
    robots += "".join(f"User-agent: {b}\nAllow: /\n" for b in bots)
    robots += f"\nSitemap: {D}/sitemap.xml\n"
    (ROOT / "robots.txt").write_text(robots, encoding="utf-8")

    d = S["doctor"]
    hours = " / ".join(f"{h['label']} {h['opens']}~{h['closes']} ({h['note']})" for h in S["hours"])
    pages = "".join(f"- [{m['title'].split('|')[0].strip()}]({D}{m['path']}): {m['description']}\n" for m in metas)
    faqs = "".join(f"\n### {f['q']}\n{f['a']}\n" for m in metas for f in m.get("faq", []))
    llms = f"""# {S['site_name']}

> {S['road_address']}에 있는 의원. 1층 인공신장실(혈액투석, 인공신장기 13대), 29병상 입원실, 소아청소년과 전문의 진료, 고혈압·당뇨·고지혈증 관리, 도수·물리치료, 입원 치료, 예방접종을 합니다. 원장은 남원에서 25년 이상 진료한 소아청소년과 전문의 {d['name']}입니다.

## 기본 정보
- 주소: {S['road_address']} (지번 {S['jibun_address']})
- 전화: 대표 {S['phone']}, 직통 {S['phone2']}, 투석 상담 {S['phone_dialysis']}
- 진료시간: {hours}, {S['closed']}. {S['holiday_note']}
- 주차: {S['parking']}
- 교통: {S['transit']}
- 결제: {S['payment']}
- 장비: {', '.join(S['equipment'])}
- 원장: {d['name']} ({d['specialty']}). {' / '.join(d['career'][1:])}
- 네이버 블로그: {S['blog']}
- 인스타그램: {S['instagram']}
- 스레드: {S['threads']}
- 원장 페이스북: {S['facebook_doctor']}
- 네이버 플레이스: {S['naver_place']}

## 페이지
{pages}
## 자주 묻는 질문
{faqs}
## 참고
- 건강 정보는 일반 안내이며, 진단과 치료는 진료를 통해 받아야 합니다.
"""
    (ROOT / "llms.txt").write_text(llms, encoding="utf-8")
    # 공개용 폴더 dist/ : 인터넷에 올라가는 파일만 모읍니다 (메모·원본 제외).
    dist = ROOT / "dist"
    if dist.exists():
        shutil.rmtree(dist)
    dist.mkdir()
    for name in ["index.html", "404.html", "sitemap.xml", "robots.txt", "llms.txt"]:
        shutil.copy2(ROOT / name, dist / name)
    shutil.copytree(ROOT / "assets", dist / "assets")
    # 보안 머리말과 사진 캐시(하루). Cloudflare 가 읽는 파일.
    (dist / "_headers").write_text(
        "/*\n  X-Content-Type-Options: nosniff\n  Referrer-Policy: strict-origin-when-cross-origin\n  X-Frame-Options: SAMEORIGIN\n"
        "/assets/img/*\n  Cache-Control: public, max-age=86400\n",
        encoding="utf-8",
    )
    for m in metas:
        if m["path"] != "/":
            sub = m["path"].strip("/")
            (dist / sub).mkdir(parents=True)
            shutil.copy2(ROOT / sub / "index.html", dist / sub / "index.html")
    print(f"완료: 페이지 {len(metas)}개, sitemap.xml, robots.txt, llms.txt, dist/ (공개용)")
    if "YOUR-DOMAIN" in D:
        print("알림: site.json 의 domain 을 실제 주소로 바꾼 뒤 다시 실행하세요.")


if __name__ == "__main__":
    main()
