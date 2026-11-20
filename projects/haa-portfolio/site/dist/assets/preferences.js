(() => {
  const bundle = window.PORTFOLIO_I18N;
  if (!bundle?.source) return; // Static English and the independent theme switch remain usable.
  const {config, manifest, source} = bundle;
  const languages = config.languages;
  const get = key => {try {return localStorage.getItem(key)} catch {return null}};
  const set = (key,value) => {try {localStorage.setItem(key,value)} catch {}};
  const root = document.documentElement;
  const selector = document.getElementById('language-select');
  const languageTrigger = document.querySelector('.language-trigger');
  const currentLanguage = document.getElementById('current-language');
  const toggle = document.getElementById('theme-toggle');
  const status = document.getElementById('preference-status');
  const scriptUrl = document.currentScript?.src || new URL(document.querySelector('script[src*="preferences.js"]')?.getAttribute('src') || 'assets/preferences.js', document.baseURI).href;
  const textElements = [...document.querySelectorAll('[data-i18n]')];
  const attributes = [];
  ['alt','aria-label','placeholder','content','title'].forEach(attr => document.querySelectorAll(`[data-i18n-${attr}]`).forEach(el => attributes.push({el,attr,key:el.getAttribute(`data-i18n-${attr}`)})));
  const dictionaries = new Map([[config.sourceLanguage,source]]);
  let current = config.sourceLanguage;
  let request = 0;
  const normalize = language => languages.includes(language) ? language : languages.includes(String(language).split('-')[0]) ? String(language).split('-')[0] : config.defaultLanguage;
  const flatten = (value,prefix='',out=Object.create(null)) => {
    for (const [key,entry] of Object.entries(value)) {
      const full = prefix ? `${prefix}.${key}` : key;
      if (full.split('.').some(part => ['__proto__','constructor','prototype'].includes(part))) continue;
      if (typeof entry === 'string') out[full] = entry;
      else if (entry && typeof entry === 'object' && !Array.isArray(entry)) flatten(entry,full,out);
    }
    return out;
  };
  const markup = value => [...value.matchAll(/<\/?[a-z][^>]*>/gi)].map(match => match[0]).sort().join('|');
  const params = value => [...value.matchAll(/\{([A-Za-z]\w*)\}/g)].map(match => match[1]).sort().join('|');
  function register(language,values) {
    const dictionary = flatten(values);
    const validated = Object.create(null);
    for (const [key,value] of Object.entries(dictionary)) {
      if (typeof source[key] !== 'string' || !value) continue;
      // Translations may change text, but not the markup, links, or placeholders of the English source.
      if (markup(value) !== markup(source[key]) || params(value) !== params(source[key])) continue;
      validated[key] = value;
    }
    dictionaries.set(language,validated);
    return validated;
  }
  function translate(key,values={},language=current) {
    const dictionary = dictionaries.get(language);
    const text = dictionary?.[key] ?? source[key] ?? key;
    return text.replace(/\{([A-Za-z]\w*)\}/g, (match,name) => values[name] === undefined ? match : String(values[name]));
  }
  const escapeHtml = value => String(value).replace(/[&<>"']/g,char => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[char]));
  const translateHtml = (key,values={}) => translate(key,Object.fromEntries(Object.entries(values).map(([name,value])=>[name,escapeHtml(value)])));
  function refreshLabels() {
    const selectedName = source[`ui.language_names.${current}`];
    toggle.setAttribute('aria-label',translate('ui.theme.dark_mode'));
    toggle.title = translate(toggle.checked ? 'ui.theme.switch_to_light' : 'ui.theme.switch_to_dark');
    languageTrigger.setAttribute('aria-label',translate('ui.language_selected',{label:translate('ui.language'),name:selectedName}));
    languageTrigger.title = translate('ui.language');
    currentLanguage.textContent = selectedName;
    currentLanguage.lang = config.languageTags[current] || current;
    document.querySelectorAll('.language-choice').forEach(button => button.setAttribute('aria-pressed',String(button.dataset.language === current)));
    document.querySelector('.site-tools')?.setAttribute('aria-label',translate('ui.preferences'));
    document.querySelector('.menu-toggle')?.setAttribute('aria-label',translate(document.querySelector('.nav-links')?.classList.contains('open') ? 'ui.navigation.close' : 'ui.navigation.open'));
  }
  function apply() {
    textElements.forEach(el => {el.innerHTML = translateHtml(el.dataset.i18n)});
    attributes.forEach(({el,attr,key}) => el.setAttribute(attr,translate(key)));
    root.lang = config.languageTags[current] || current;
    root.dataset.language = current;
    selector.value = current;
    refreshLabels();
    document.dispatchEvent(new CustomEvent('portfolio:languagechange',{detail:{language:current}}));
  }
  let loader = async ({language,url}) => {
    const response = await fetch(url);
    if (!response.ok) throw new Error('Locale unavailable');
    return response.json();
  };
  async function changeLanguage(language,remember=false) {
    language = normalize(language);
    const token = ++request;
    selector.value = language;
    selector.setAttribute('aria-busy','true');
    try {
      if (!dictionaries.has(language)) {
        const url = new URL(`locales/${language}.json?v=${manifest.revision}`,scriptUrl);
        const loaded = await loader({language,url,revision:manifest.revision});
        register(language,loaded);
      }
      if (token !== request) return;
      current = language;
      selector.open = false;
      apply();
      if (remember) {set(config.storageKey,current);status.textContent = translate('ui.language_changed');}
    } catch {
      if (token !== request) return;
      selector.value = current;
      status.textContent = translate('ui.translation_load_error');
    } finally {if (token === request) selector.removeAttribute('aria-busy');}
  }
  window.portfolioI18n = {
    get language() {return current},
    t:translate,
    html:translateHtml,
    setLanguage:changeLanguage,
    menuLabel:open=>translate(open ? 'ui.navigation.close' : 'ui.navigation.open'),
    registerTranslations:(language,values)=>{register(normalize(language),values);if(normalize(language)===current)apply()},
    setLoader:custom=>{if(typeof custom!=='function')throw new TypeError('Translation loader must be a function');loader=custom;for(const language of languages)if(language!==config.sourceLanguage)dictionaries.delete(language)},
    config
  };
  document.addEventListener('portfolio:themechange',refreshLabels);
  selector.addEventListener('change',()=>changeLanguage(selector.value,true));
  document.querySelectorAll('.language-choice').forEach(button=>button.addEventListener('click',()=>changeLanguage(button.dataset.language,true)));
  document.addEventListener('click',event=>{if(!selector.contains(event.target))selector.open=false});
  document.addEventListener('keydown',event=>{if(event.key==='Escape'&&selector.open){selector.open=false;languageTrigger.focus()}});
  apply();
  const saved=get(config.storageKey);
  const detected=config.autoDetect ? (navigator.languages||[navigator.language||config.defaultLanguage]).map(lang=>lang.split('-')[0]).find(lang=>languages.includes(lang)) : null;
  changeLanguage(languages.includes(saved) ? saved : detected || config.defaultLanguage);
})();
