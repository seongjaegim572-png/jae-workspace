"""Build public, text-first profiles from the portfolio's existing page/catalog sources.

This is a projection of the human pages, not a separate biography. All factual
copy, metric labels, image descriptions, and evidence URLs come from localized
templates. Run after the normal i18n build has compiled the five catalogs.
"""

from dataclasses import dataclass, field
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urljoin, urlsplit
import html
import importlib.util
import json
import re


ROOT = Path(__file__).resolve().parents[1]
VOID = {"area", "base", "br", "col", "embed", "hr", "img", "input", "link", "meta", "param", "source", "track", "wbr"}
PAGES = (
    ("home", "index.html"),
    ("music", "work/music.html"),
    ("growth", "work/growth.html"),
    ("military", "work/military.html"),
    ("questions", "explorations.html"),
    ("sibyl", "project.html"),
)
SKIP_TAGS = {"nav", "aside", "script", "style", "noscript", "form", "button", "input", "select", "option", "textarea", "label", "template", "svg"}
SKIP_CLASSES = {"breadcrumb", "case-aside", "eyebrow", "arrow", "number", "art-badge", "hero-foot", "prototype", "sample-result", "sr-only"}
LABEL_DEFAULTS = {
    "view.human": "Human",
    "view.machine": "Machine",
    "view.label": "View",
    "machine.title": "Jae Kim — Machine view",
    "machine.description": "A text-first version of the portfolio, generated from the same content as the human view.",
    "machine.download": "Download Markdown",
    "machine.contents": "Contents",
    "machine.source": "Source",
    "machine.language": "Language",
    "machine.profile": "Profile",
    "machine.contact": "Contact and public profiles",
}


@dataclass
class Element:
    tag: str
    attrs: dict = field(default_factory=dict)
    children: list = field(default_factory=list)

    @property
    def classes(self):
        return set(self.attrs.get("class", "").split())


class TreeParser(HTMLParser):
    def __init__(self, source):
        super().__init__(convert_charrefs=True)
        self.root = Element("document")
        self.stack = [self.root]
        self.feed(source)
        self.close()

    def handle_starttag(self, tag, attrs):
        node = Element(tag, dict(attrs))
        self.stack[-1].children.append(node)
        if tag not in VOID:
            self.stack.append(node)

    def handle_startendtag(self, tag, attrs):
        self.handle_starttag(tag, attrs)
        if tag not in VOID:
            self.handle_endtag(tag)

    def handle_endtag(self, tag):
        for i in range(len(self.stack) - 1, 0, -1):
            if self.stack[i].tag == tag:
                self.stack = self.stack[:i]
                return

    def handle_data(self, data):
        self.stack[-1].children.append(data)


def descendants(node):
    for child in node.children:
        if isinstance(child, Element):
            yield child
            yield from descendants(child)


def first(node, tag):
    return next((child for child in descendants(node) if child.tag == tag), None)


def whitespace(value):
    return re.sub(r"\s+", " ", value).strip()


def plain(node):
    if isinstance(node, str):
        return whitespace(node)
    if node.attrs.get("aria-hidden") == "true" or node.classes & {"arrow", "sr-only"}:
        return ""
    if node.tag == "img":
        return whitespace(node.attrs.get("alt", ""))
    return whitespace(" ".join(plain(child) for child in node.children))


def md_text(value):
    return re.sub(r"([\\`*\[\]<>])", r"\\\1", value)


def join_inline(parts):
    """Keep adjacent catalog spans readable without changing their wording."""
    result = " ".join(part for part in parts if part)
    return re.sub(r"\s+([,.;:!?，。；：！？])", r"\1", result).strip()


class Projection:
    def __init__(self, source_url):
        self.source_url = source_url
        self.emitted_links = set()

    def url(self, value):
        absolute = urljoin(self.source_url, value)
        return absolute if urlsplit(absolute).scheme in {"https", "http", "mailto"} else ""

    def skip(self, node):
        return (
            node.tag in SKIP_TAGS
            or bool(node.classes & SKIP_CLASSES)
            or node.attrs.get("aria-hidden") == "true"
            or "hidden" in node.attrs
            or node.attrs.get("id") == "demo"
        )

    def inline(self, node, linked=False):
        if isinstance(node, str):
            text = whitespace(node)
            return html.escape(text), md_text(text)
        if self.skip(node):
            return "", ""
        if node.tag == "br":
            return " ", " "
        if node.tag == "img":
            text = plain(node)
            return html.escape(text), md_text(text)
        items = [self.inline(child, linked or node.tag == "a") for child in node.children]
        h = join_inline([part[0] for part in items])
        m = join_inline([part[1] for part in items])
        if node.tag == "a":
            target = self.url(node.attrs.get("href", ""))
            if target and not linked:
                if not h:
                    label = plain(node) or node.attrs.get("aria-label") or target
                    h, m = html.escape(label), md_text(label)
                self.emitted_links.add(target)
                return f'<a href="{html.escape(target, quote=True)}">{h}</a>', f'[{m}](<{target}>)'
        if node.tag in {"strong", "b"}:
            return f"<strong>{h}</strong>", f"**{m}**"
        if node.tag in {"em", "i"}:
            return f"<em>{h}</em>", f"*{m}*"
        return h, m

    def paragraph(self, node, css=""):
        h, m = self.inline(node)
        if not h:
            return [], []
        attr = f' class="{css}"' if css else ""
        return [f"<p{attr}>{h}</p>"], [m]

    def blocks(self, node):
        if isinstance(node, str):
            return [], []
        if self.skip(node) or node.tag == "h1":
            return [], []
        if re.fullmatch(r"h[2-6]", node.tag):
            level = min(int(node.tag[1]) + 1, 6)
            h, m = self.inline(node)
            return [f"<h{level}>{h}</h{level}>"], ["#" * level + " " + m]
        if node.tag in {"p", "figcaption"}:
            return self.paragraph(node, "machine-caption" if node.tag == "figcaption" or "notice" in node.classes else "")
        if node.tag == "blockquote" or "callout" in node.classes:
            h, m = self.inline(node)
            return ([f"<blockquote><p>{h}</p></blockquote>"], ["> " + m]) if h else ([], [])
        if node.tag in {"ul", "ol"}:
            rows_h, rows_m = [], []
            for child in node.children:
                if not isinstance(child, Element) or child.tag != "li":
                    continue
                h, m = self.inline(child)
                if h:
                    rows_h.append(f"<li>{h}</li>")
                    marker = f"{len(rows_m) + 1}." if node.tag == "ol" else "-"
                    rows_m.append(f"{marker} {m}")
            return ([f'<{node.tag}>\n'+"\n".join(rows_h)+f'\n</{node.tag}>'], ["\n".join(rows_m)]) if rows_h else ([], [])
        if node.tag == "dl":
            pairs_h, pairs_m = [], []
            current = None
            for child in descendants(node):
                if child.tag == "dt":
                    current = self.inline(child)
                elif child.tag == "dd":
                    h, m = self.inline(child)
                    term_h, term_m = current or ("", "")
                    pairs_h.append(f"<dt>{term_h}</dt><dd>{h}</dd>")
                    pairs_m.append(f"- **{term_m}:** {m}")
                    current = None
            return (["<dl>"+"\n".join(pairs_h)+"</dl>"], ["\n".join(pairs_m)]) if pairs_h else ([], [])
        if node.classes & {"stat", "data-cell"}:
            number = next((child for child in node.children if isinstance(child, Element) and child.tag in {"b", "strong"}), None)
            if number:
                value_h, value_m = self.inline(number)
                labels = [self.inline(child) for child in node.children if child is not number]
                label_h = join_inline([item[0] for item in labels])
                label_m = join_inline([item[1] for item in labels])
                return [f"<p>{value_h} — {label_h}</p>"], [f"- {value_m} — {label_m}"]
        if node.tag == "img":
            target = self.url(node.attrs.get("src", ""))
            label = plain(node)
            if target and label and target not in self.emitted_links:
                self.emitted_links.add(target)
                return [f'<p class="machine-evidence"><a href="{html.escape(target, quote=True)}">{html.escape(label)}</a></p>'], [f'[{md_text(label)}](<{target}>)']
            return [], []
        if node.tag == "a":
            # Cards carry headings and paragraphs, while ordinary/evidence links
            # are emitted once as text. Image evidence remains an absolute link.
            if any(child.tag in {"h2", "h3", "h4", "p"} for child in descendants(node)):
                parts_h, parts_m = self.children(node)
                target = self.url(node.attrs.get("href", ""))
                if target:
                    title = next((plain(child) for child in descendants(node) if child.tag in {"h2", "h3", "h4"}), plain(node))
                    parts_h.append(f'<p><a href="{html.escape(target, quote=True)}">{html.escape(title)}</a></p>')
                    parts_m.append(f'[{md_text(title)}](<{target}>)')
                    self.emitted_links.add(target)
                return parts_h, parts_m
            target = self.url(node.attrs.get("href", ""))
            if target and target in self.emitted_links:
                return [], []
            return self.paragraph(node, "machine-evidence")
        if "img-caption" in node.classes:
            return self.paragraph(node, "machine-caption")
        if "data-i18n" in node.attrs and not any(child.tag in {"h1", "h2", "h3", "p", "ul", "ol", "dl"} for child in descendants(node)):
            key = node.attrs["data-i18n"]
            if key.endswith(("read_case_study", "read_experience", "click_to_read_each_story")):
                return [], []
            return self.paragraph(node)
        return self.children(node)

    def children(self, node):
        result_h, result_m = [], []
        for child in node.children:
            h, m = self.blocks(child)
            result_h.extend(h)
            result_m.extend(m)
        return result_h, result_m


def load_materializer():
    spec = importlib.util.spec_from_file_location("portfolio_page_renderer", ROOT / "scripts/render-pages.py")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.materialize


def source_origin():
    tree = TreeParser((ROOT / "src/pages/index.html").read_text()).root
    canonical = next((node.attrs.get("href") for node in descendants(tree) if node.tag == "link" and node.attrs.get("rel") == "canonical"), None)
    if not canonical or urlsplit(canonical).scheme != "https":
        raise ValueError("A canonical HTTPS portfolio homepage is required")
    parsed = urlsplit(canonical)
    return f"{parsed.scheme}://{parsed.netloc}"


def build():
    materialize = load_materializer()
    config = json.loads((ROOT / "i18n.config.json").read_text())
    manifest = json.loads((ROOT / "dist/assets/i18n-manifest.json").read_text())
    origin = source_origin()
    human_home = origin + "/index.html"
    revision = manifest.get("revision", "")
    asset_suffix = "?v=" + revision if revision else ""
    language_paths = {lang: "/machine/" + ("" if lang == "en" else lang + "/") for lang in config["languages"]}
    sources = [(section_id, path, (ROOT / "src/pages" / path).read_text()) for section_id, path in PAGES]

    for lang in config["languages"]:
        catalog = json.loads((ROOT / "dist/assets/locales" / (lang + ".json")).read_text())
        label = lambda key: catalog.get(key, LABEL_DEFAULTS.get(key, key))
        profile_url = origin + language_paths[lang]
        sections, markdown_sections, toc, home_tree = [], [], [], None
        for section_id, path, source in sources:
            tree = TreeParser(materialize(source, catalog)).root
            if path == "index.html":
                home_tree = tree
            main = first(tree, "main")
            if main is None:
                raise ValueError(f"Main content required: {path}")
            heading = first(main, "h1")
            if heading is None:
                raise ValueError(f"Main h1 required: {path}")
            title = plain(heading)
            source_url = origin + "/" + path
            projector = Projection(source_url)
            content_h, content_m = projector.children(main)
            # An aside may hold an embedded reference rather than body copy.
            # Preserve its original external URL, without copying aside UI.
            for aside in (node for node in descendants(main) if node.tag == "aside" and "question-reference" in node.classes):
                for anchor in (node for node in descendants(aside) if node.tag == "a"):
                    target = projector.url(anchor.attrs.get("href", ""))
                    if target and urlsplit(target).netloc != urlsplit(origin).netloc and target not in projector.emitted_links:
                        title_link = plain(anchor) or label("machine.source")
                        content_h.append(f'<p class="machine-evidence"><a href="{html.escape(target, quote=True)}">{html.escape(title_link)}</a></p>')
                        content_m.append(f'[{md_text(title_link)}](<{target}>)')
                        projector.emitted_links.add(target)
            source_h = f'<p class="machine-source">{html.escape(label("machine.source"))}: <a href="{html.escape(source_url, quote=True)}">{html.escape(source_url)}</a></p>'
            sections.append(f'<section class="machine-section" id="{section_id}"><h2>{html.escape(title)}</h2>\n{source_h}\n'+"\n".join(content_h)+"\n</section>")
            markdown_sections.append("## " + title + "\n\n" + label("machine.source") + ": " + source_url + "\n\n" + "\n\n".join(content_m))
            toc.append((section_id, title))

        footer = first(home_tree, "footer")
        contact_h, contact_m = [], []
        contact_seen = set()
        if footer:
            for anchor in (node for node in descendants(footer) if node.tag == "a"):
                target = Projection(human_home).url(anchor.attrs.get("href", ""))
                if not target or target in contact_seen:
                    continue
                text = plain(anchor).rstrip(" ↗→") or target
                contact_h.append(f'<li><a href="{html.escape(target, quote=True)}">{html.escape(text)}</a></li>')
                contact_m.append(f'- [{md_text(text)}](<{target}>)')
                contact_seen.add(target)
        contact_title = label("machine.contact")
        sections.append('<section class="machine-contact" id="contact"><h2>'+html.escape(contact_title)+"</h2><ul>"+"\n".join(contact_h)+"</ul></section>")
        markdown_sections.append("## " + contact_title + "\n\n" + "\n".join(contact_m))
        toc.append(("contact", contact_title))
        title, description = label("machine.title"), label("machine.description")
        human_label, machine_label = label("view.human"), label("view.machine")
        assets = "../assets/" if lang == "en" else "../../assets/"
        human_return = "../index.html" if lang == "en" else "../../index.html"
        def language_link(code):
            if code == lang:
                return "index.html"
            if lang == "en":
                return code + "/index.html"
            return "../index.html" if code == "en" else "../" + code + "/index.html"
        lang_links = "\n".join(
            f'<a href="{language_link(code)}" data-machine-language="{code}" lang="{html.escape(config["languageTags"].get(code, code), quote=True)}"'+(' aria-current="page"' if code == lang else '')+f'>{html.escape(catalog.get("ui.language_names." + code, code))}</a>'
            for code in config["languages"]
        )
        toc_html = "\n".join(f'<li><a href="#{section_id}">{html.escape(text)}</a></li>' for section_id, text in toc)
        markdown = "# " + title + "\n\n" + description + "\n\n" + label("machine.profile") + ": " + human_home + "\n\n" + "\n\n".join(markdown_sections) + "\n"
        language_tag = config["languageTags"].get(lang, lang)
        document = f'''<!doctype html>
<html lang="{html.escape(language_tag, quote=True)}" data-language="{lang}">
<head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{html.escape(description, quote=True)}">
<title>{html.escape(title)}</title>
<link rel="canonical" href="{human_home}">
<link rel="alternate" type="text/markdown" href="{profile_url}profile.md" title="{html.escape(label('machine.download'), quote=True)}">
<link rel="stylesheet" href="{assets}machine.css{asset_suffix}">
<link rel="stylesheet" href="{assets}view-switch.css{asset_suffix}">
<script src="{assets}view-switch.js{asset_suffix}" defer></script>
</head>
<body data-view="machine">
<div class="machine-shell">
<header class="machine-header">
<nav class="view-switch" aria-label="{html.escape(label('view.label'), quote=True)}"><a href="{human_return}" data-human-link>{html.escape(human_label)}</a><span aria-current="page">{html.escape(machine_label)}</span></nav>
<h1>{html.escape(title)}</h1>
<p>{html.escape(description)}</p>
<p><a href="profile.md" download="profile.md">{html.escape(label('machine.download'))}</a></p>
<nav class="machine-language-nav" aria-label="{html.escape(label('machine.language'), quote=True)}">{lang_links}</nav>
</header>
<nav class="machine-toc" aria-label="{html.escape(label('machine.contents'), quote=True)}"><h2>{html.escape(label('machine.contents'))}</h2><ul>{toc_html}</ul></nav>
<main class="machine-document" id="main">{''.join(sections)}</main>
<footer class="machine-footer"><p><a href="{human_return}" data-human-link>{html.escape(human_label)}</a> · <a href="profile.md">{html.escape(label('machine.download'))}</a></p></footer>
</div>
</body>
</html>
'''
        directory = ROOT / "dist" / language_paths[lang].lstrip("/")
        directory.mkdir(parents=True, exist_ok=True)
        (directory / "index.html").write_text(document)
        (directory / "profile.md").write_text(markdown)
    # The source human pages are canonical; the Machine variants are alternate
    # representations rather than additional independently authored profiles.
    sitemap = '<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
    sitemap += "\n".join("  <url><loc>" + html.escape(origin + "/" + path) + "</loc></url>" for _, path in PAGES)
    sitemap += "\n</urlset>\n"
    (ROOT / "dist/sitemap.xml").write_text(sitemap)
    (ROOT / "dist/robots.txt").write_text("User-agent: *\nAllow: /\n\nSitemap: " + origin + "/sitemap.xml\n")
    print(f'Generated Machine HTML and Markdown from six canonical pages in {len(config["languages"])} languages.')


if __name__ == "__main__":
    build()
