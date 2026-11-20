import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
import vm from 'node:vm';

const root = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');
const dist = path.join(root, 'dist');
const origin = 'https://jae-kim-haa-portfolio.seong-jae-kim.chatgpt.site';
const pages = ['index.html', 'work/music.html', 'work/growth.html', 'work/military.html', 'explorations.html', 'project.html'];
const languages = ['en', 'ko', 'es', 'ja', 'zh'];
const read = relative => fs.readFileSync(path.join(dist, relative), 'utf8');
const decode = value => value
  .replace(/&#x([0-9a-f]+);/gi, (_, value) => String.fromCodePoint(parseInt(value, 16)))
  .replace(/&#([0-9]+);/g, (_, value) => String.fromCodePoint(Number(value)))
  .replace(/&(amp|lt|gt|quot|apos|nbsp);/g, (_, name) => ({amp: '&', lt: '<', gt: '>', quot: '"', apos: "'", nbsp: ' '})[name]);
const readable = value => decode(value.replace(/<[^>]*>/g, ' '))
  .replace(/\\([\\`*_{}\[\]()#+\-.!>])/g, '$1').replace(/\s+/g, ' ').trim();
function attrs(tag) {
  const result = Object.create(null);
  for (const match of tag.matchAll(/([\w:-]+)(?:\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+)))?/g)) {
    result[match[1]] = decode(match[2] ?? match[3] ?? match[4] ?? '');
  }
  return result;
}
const anchors = html => [...html.matchAll(/<a\b[^>]*>[\s\S]*?<\/a>/gi)].map(match => ({
  attributes: attrs(match[0].slice(0, match[0].indexOf('>') + 1)),
  label: readable(match[0]),
}));
function switchNav(html, filename) {
  const navs = [...html.matchAll(/<nav\b[^>]*>[\s\S]*?<\/nav>/gi)];
  const nav = navs.find(match => (attrs(match[0].slice(0, match[0].indexOf('>') + 1)).class ?? '').split(/\s+/).includes('view-switch'));
  assert.ok(nav, `${filename}: view switch must be a navigable landmark`);
  const links = anchors(nav[0]).concat([...nav[0].matchAll(/<span\b[^>]*>[\s\S]*?<\/span>/gi)].map(match => ({
    attributes: attrs(match[0].slice(0, match[0].indexOf('>') + 1)), label: readable(match[0]),
  })));
  assert.ok(links.some(link => link.label === 'Human'), `${filename}: visible Human label`);
  assert.ok(links.some(link => link.label === 'Machine'), `${filename}: visible Machine label`);
  return links;
}
function localTarget(reference, source) {
  const url = new URL(reference, origin + '/' + source);
  if (url.origin !== origin) return null;
  let target = path.join(dist, decodeURIComponent(url.pathname));
  if (fs.existsSync(target) && fs.statSync(target).isDirectory()) target = path.join(target, 'index.html');
  return {target, fragment: decodeURIComponent(url.hash.slice(1))};
}
function checkLinks(html, filename) {
  for (const match of html.matchAll(/<(?:a|link|script|img)\b[^>]*>/gi)) {
    const a = attrs(match[0]);
    const reference = a.href ?? a.src;
    if (!reference || /^(?:mailto|tel|data|javascript):/i.test(reference)) continue;
    const local = localTarget(reference, filename);
    if (!local) continue;
    assert.ok(fs.existsSync(local.target), `${filename}: missing local target ${reference}`);
    if (local.fragment && local.target.endsWith('.html')) {
      const destination = fs.readFileSync(local.target, 'utf8');
      const ids = [...destination.matchAll(/\bid\s*=\s*(?:"([^"]*)"|'([^']*)'|([^\s>]+))/g)]
        .map(match => decode(match[1] ?? match[2] ?? match[3]));
      assert.ok(ids.includes(local.fragment), `${filename}: missing anchor ${reference}`);
    }
  }
}
for (const filename of pages) {
  const html = read(filename);
  const links = switchNav(html, filename);
  const human = links.find(link => link.label === 'Human');
  const machine = links.find(link => Object.hasOwn(link.attributes, 'data-machine-link'));
  assert.equal(human.attributes['aria-current'], 'page', `${filename}: Human is current`);
  assert.ok(machine?.attributes.href, `${filename}: usable Machine destination without JavaScript`);
  assert.ok(fs.existsSync(localTarget(machine.attributes.href, filename).target), `${filename}: Machine destination exists`);
}
for (const lang of languages) {
  const directory = lang === 'en' ? 'machine' : `machine/${lang}`;
  const filename = directory + '/index.html';
  const html = read(filename);
  const markdown = read(directory + '/profile.md');
  const catalog = JSON.parse(fs.readFileSync(path.join(dist, 'assets/locales', lang + '.json'), 'utf8'));
  for (const [format, content] of [['HTML', html], ['Markdown', markdown]]) {
    assert.ok(readable(content).includes(readable(catalog['clinic_build.savings'])), `${lang} ${format}: clinic costs match the current catalog`);
    assert.ok(content.includes('DD0FeDDxj5h'), `${lang} ${format}: Suno evidence link`);
    assert.ok(!/instagram\.com\/embed\.js/i.test(content), `${lang} ${format}: text view does not execute Instagram embeds`);
    assert.ok(!/\{\{[^}]+\}\}|\{(?:name|goal|language|project|profile)\}/.test(content), `${lang} ${format}: no unresolved templates`);
    for (const page of pages) {
      assert.ok(content.includes('/' + page), `${lang} ${format}: source link to ${page}`);
    }
  }
  const links = switchNav(html, filename);
  assert.ok(links.some(link => Object.hasOwn(link.attributes, 'data-human-link') && link.attributes.href), `${lang}: Human return link`);
  assert.equal(links.find(link => link.label === 'Machine').attributes['aria-current'], 'page', `${lang}: Machine is current`);
  const canonical = [...html.matchAll(/<link\b[^>]*>/gi)].map(match => attrs(match[0])).find(a => a.rel === 'canonical');
  assert.ok(canonical?.href, `${lang}: canonical URL`);
  assert.ok([origin + '/', origin + '/index.html'].includes(canonical.href), `${lang}: canonical points to the human home page`);
  checkLinks(html, filename);
  for (const match of markdown.matchAll(/\[[^\]]*\]\(([^)]+)\)/g)) {
    const reference = match[1].trim().replace(/^<([\s\S]*)>$/, '$1');
    const target = localTarget(reference, directory + '/profile.md');
    if (target) assert.ok(fs.existsSync(target.target), `${lang} Markdown: missing local destination ${match[1]}`);
  }
}

// The view switch changes links, never the visitor's page automatically.
const switchScript = read('assets/view-switch.js');
function switchHarness({url, view = 'human', savedLanguage, appliedLanguage, documentLanguage, storageBlocked = false}) {
  const location = new URL(url);
  const documentEvents = new Map();
  const windowEvents = new Map();
  const saved = new Map(savedLanguage ? [['jae-portfolio-language', savedLanguage]] : []);
  const anchor = () => ({href: '', dataset: {}, events: new Map(), addEventListener(name, callback) {this.events.set(name, callback);}});
  const machine = anchor();
  const human = anchor();
  const languageLinks = languages.map(language => ({href: '', dataset: {machineLanguage: language}, events: new Map(),
    addEventListener(name, callback) {this.events.set(name, callback);},
  }));
  let navigations = 0;
  const rejectNavigation = () => {navigations++; throw new Error('View switching must require an explicit link click');};
  const browserLocation = {
    get href() {return location.href;}, set href(_) {rejectNavigation();},
    get hash() {return location.hash;}, set hash(_) {rejectNavigation();},
    assign: rejectNavigation, replace: rejectNavigation,
  };
  const document = {
    currentScript: {src: origin + '/assets/view-switch.js?v=test'},
    baseURI: location.href,
    documentElement: {dataset: documentLanguage ? {language: documentLanguage} : {}},
    body: {dataset: {view}},
    querySelectorAll(selector) {return ({'[data-machine-link]': [machine], '[data-human-link]': [human], '[data-machine-language]': languageLinks})[selector] ?? [];},
    addEventListener(name, callback) {documentEvents.set(name, callback);},
  };
  const window = {
    location: browserLocation,
    portfolioI18n: appliedLanguage ? {language: appliedLanguage} : undefined,
    addEventListener(name, callback) {windowEvents.set(name, callback);},
  };
  const localStorage = {
    getItem(key) {if (storageBlocked) throw new Error('Storage blocked'); return saved.get(key) ?? null;},
    setItem(key, value) {if (storageBlocked) throw new Error('Storage blocked'); saved.set(key, value);},
  };
  vm.runInNewContext(switchScript, {window, document, localStorage, URL}, {filename: 'view-switch.js'});
  assert.equal(navigations, 0, 'loading a view must never navigate automatically');
  assert.equal(browserLocation.href, url, 'initial location stays unchanged');
  return {machine, human, languageLinks, saved, window, document,
    setHash(hash) {location.hash = hash; windowEvents.get('hashchange')?.();},
    setLanguage(language) {window.portfolioI18n = {language}; document.documentElement.dataset.language = language; documentEvents.get('portfolio:languagechange')?.();},
  };
}
const visitor = switchHarness({url: origin + '/work/growth.html#clinic', savedLanguage: 'ko'});
assert.equal(new URL(visitor.machine.href).pathname, '/machine/ko/index.html', 'saved supported language is preserved');
assert.equal(new URL(visitor.machine.href).searchParams.get('from'), 'work/growth.html#clinic', 'source page and section are preserved');
assert.equal(new URL(visitor.machine.href).hash, '#growth', 'machine view opens the matching case study');
visitor.setHash('#website');
assert.equal(new URL(visitor.machine.href).searchParams.get('from'), 'work/growth.html#website', 'in-page navigation updates the return destination');
visitor.setLanguage('ja');
assert.equal(new URL(visitor.machine.href).pathname, '/machine/ja/index.html', 'applied language changes update the Machine destination');
const from = 'work/music.html#track';
const machineUrl = new URL('/machine/ja/index.html', origin);
machineUrl.searchParams.set('from', from);
const machineVisitor = switchHarness({url: machineUrl.href, view: 'machine', savedLanguage: 'ko', documentLanguage: 'ja'});
assert.equal(machineVisitor.human.href, origin + '/work/music.html#track', 'Human returns to the original page and section');
machineVisitor.human.events.get('click')();
assert.equal(machineVisitor.saved.get('jae-portfolio-language'), 'ja', 'Human return preserves the machine view language');
for (const link of machineVisitor.languageLinks) {
  assert.equal(new URL(link.href).searchParams.get('from'), from, 'changing machine language preserves the safe Human destination');
}
const spanish = machineVisitor.languageLinks.find(link => link.dataset.machineLanguage === 'es');
spanish.events.get('click')();
assert.equal(machineVisitor.saved.get('jae-portfolio-language'), 'es', 'explicit machine language choice is remembered');
for (const rejected of ['https://evil.example/redirect', '//evil.example/redirect', '../index.html', '/work/music.html', 'private.html', 'work/music.html?next=https://evil.example/', 'work/music.html#one#two', 'work/music.html#bad\nfragment']) {
  const url = new URL('/machine/index.html', origin);
  url.searchParams.set('from', rejected);
  const visitor = switchHarness({url: url.href, view: 'machine', documentLanguage: 'en'});
  assert.equal(visitor.human.href, origin + '/index.html', `unapproved return destination is rejected: ${JSON.stringify(rejected)}`);
}
const blocked = switchHarness({url: origin + '/index.html', storageBlocked: true});
assert.equal(new URL(blocked.machine.href).pathname, '/machine/index.html', 'blocked storage keeps an English destination usable');

// An actual source edit must refresh both views in the ordinary build command.
const temporary = fs.mkdtempSync(path.join(os.tmpdir(), 'haa-machine-view-test-'));
try {
  for (const directory of ['locales', '.i18n', 'scripts', 'src', 'dist']) {
    fs.cpSync(path.join(root, directory), path.join(temporary, directory), {recursive: true});
  }
  fs.copyFileSync(path.join(root, 'i18n.config.json'), path.join(temporary, 'i18n.config.json'));
  const marker = 'Machine view synchronization check 2026: the annual savings came from this shared source.';
  const run = spawnSync(process.execPath, ['scripts/i18n.mjs', 'set', 'en', 'clinic_build.savings', marker], {
    cwd: temporary, encoding: 'utf8', maxBuffer: 8 * 1024 * 1024,
  });
  assert.equal(run.status, 0, run.error?.message || run.stderr || run.stdout);
  for (const filename of ['index.html', 'work/growth.html', 'machine/index.html', 'machine/profile.md']) {
    const content = fs.readFileSync(path.join(temporary, 'dist', filename), 'utf8');
    assert.ok(readable(content).includes(marker), `${filename}: ordinary source editing updates this view automatically`);
  }
} finally {
  fs.rmSync(temporary, {recursive: true, force: true});
}
console.log('PASS: Human/Machine navigation, five localized text exports, safe page returns, language and section preservation, canonical URLs, and shared-source rebuilds.');
