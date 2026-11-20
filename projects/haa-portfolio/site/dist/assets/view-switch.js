/* Explicit links switch presentation; every view remains publicly readable. */
(() => {
  const script = document.currentScript;
  const base = new URL('../', script?.src || new URL('assets/view-switch.js', document.baseURI));
  const current = new URL(window.location.href);
  const languages = ['en', 'ko', 'es', 'ja', 'zh'];
  const sections = {
    'index.html': 'home',
    'work/music.html': 'music',
    'work/growth.html': 'growth',
    'work/military.html': 'military',
    'explorations.html': 'questions',
    'project.html': 'sibyl'
  };
  const machineLinks = [...document.querySelectorAll('[data-machine-link]')];
  const humanLinks = [...document.querySelectorAll('[data-human-link]')];
  const languageLinks = [...document.querySelectorAll('[data-machine-language]')];
  const isMachine = document.body?.dataset.view === 'machine';
  function safeSource(value) {
    if (typeof value !== 'string') return null;
    const [pathname, fragment = '', ...extra] = value.split('#');
    if (extra.length || !Object.hasOwn(sections, pathname) || /[\u0000-\u0020]/.test(fragment)) return null;
    return pathname + (fragment ? '#' + fragment : '');
  }
  const incomingSource = safeSource(current.searchParams.get('from'));
  const relativePath = current.pathname.startsWith(base.pathname) ? current.pathname.slice(base.pathname.length) : '';
  const source = () => isMachine ? incomingSource || 'index.html' : safeSource((relativePath || 'index.html') + (window.location.hash ?? current.hash)) || 'index.html';
  function language() {
    const applied = window.portfolioI18n?.language || document.documentElement.dataset.language;
    if (languages.includes(applied)) return applied;
    try {
      const saved = localStorage.getItem('jae-portfolio-language');
      if (languages.includes(saved)) return saved;
    } catch (_) {}
    return 'en';
  }
  function machineUrl(lang) {
    const from = source();
    const url = new URL('machine/' + (lang === 'en' ? '' : lang + '/') + 'index.html', base);
    url.searchParams.set('from', from);
    url.hash = sections[from.split('#')[0]];
    return url.href;
  }
  function refresh() {
    machineLinks.forEach(link => { link.href = machineUrl(language()); });
    humanLinks.forEach(link => { link.href = new URL(source(), base).href; });
    languageLinks.forEach(link => {
      const lang = link.dataset.machineLanguage;
      if (!languages.includes(lang)) return;
      link.href = machineUrl(lang);
    });
  }
  languageLinks.forEach(link => link.addEventListener('click', () => {
    const lang = link.dataset.machineLanguage;
    if (languages.includes(lang)) try { localStorage.setItem('jae-portfolio-language', lang); } catch (_) {}
  }));
  humanLinks.forEach(link => link.addEventListener('click', () => {
    try { localStorage.setItem('jae-portfolio-language', language()); } catch (_) {}
  }));
  document.addEventListener('portfolio:languagechange', refresh);
  window.addEventListener('hashchange', refresh);
  refresh();
})();
