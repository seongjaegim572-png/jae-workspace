const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const rootPath = path.resolve(__dirname,'../dist');
const keyMap=JSON.parse(fs.readFileSync(path.resolve(__dirname,'../locales/legacy-key-map.json'),'utf8'));
const locales = Object.fromEntries(['en','ko','es','ja','zh'].map(lang => [lang,JSON.parse(fs.readFileSync(`${rootPath}/assets/locales/${lang}.json`,'utf8'))]));
class Element {
  constructor(tag='div', attrs={}) {
    this.tagName=tag.toUpperCase();this.attrs={...attrs};this.dataset={};this.style={};this.listeners={};this.children=[];this.value='';this.hidden=false;this._html='';this._text='';
    this.classes=new Set();this.classList={contains:v=>this.classes.has(v),toggle:(v,on)=>on?this.classes.add(v):this.classes.delete(v),add:v=>this.classes.add(v)};
    for(const [k,v] of Object.entries(attrs))if(k.startsWith('data-'))this.dataset[k.slice(5).replace(/-([a-z])/g,(_,c)=>c.toUpperCase())]=v;
  }
  set innerHTML(value){this._html=value;this._text=value.replace(/<[^>]*>/g,'');this.children=[]}
  get innerHTML(){return this._html}
  set textContent(value){this._text=String(value);this._html=String(value);this.children=[]}
  get textContent(){return this._text+this.children.map(x=>x.textContent).join(' ')}
  setAttribute(key,value){this.attrs[key]=String(value);if(key.startsWith('data-'))this.dataset[key.slice(5).replace(/-([a-z])/g,(_,c)=>c.toUpperCase())]=String(value)}
  getAttribute(key){return this.attrs[key]??null}
  removeAttribute(key){delete this.attrs[key]}
  addEventListener(type,listener){(this.listeners[type]??=[]).push(listener)}
  dispatch(type,extra={}){for(const listener of this.listeners[type]??[])listener({preventDefault(){},target:this,...extra})}
  append(...nodes){this.children.push(...nodes)}
  replaceChildren(...nodes){this.children=nodes;this._text='';this._html=''}
  querySelectorAll(selector){return selector==='a'?this.links||[]:[]}
  contains(target){return target===this||this.children.includes(target)}
  focus(){this.focused=true}
  reportValidity(){return true}
  scrollIntoView(){this.scrolled=true}
}
function setup({stored={},language='en',dark=false,storageBlocked=false,delayFetch=false,form=true}={}) {
  const elements={};for(const id of ['language-select','current-language','theme-toggle','preference-status','artist','genre','goal','platform','sibyl-result-title','sibyl-ideas','sibyl-result'])elements[id]=new Element();
  if(form)elements['sibyl-form']=new Element('form');
  elements.artist.value='82 Jae';elements.genre.value='hyperpop';elements.goal.value='discover';elements.platform.value='Instagram Reels';elements['sibyl-result'].hidden=true;
  const html=new Element('html');const menu=new Element('button');const nav=new Element('nav');nav.links=[new Element('a')];const meta=new Element('meta');const tools=new Element();const languageTrigger=new Element('summary');const choices=['en','ko','es','ja','zh'].map(lang=>new Element('button',{'data-language':lang}));elements['language-select'].children=choices;
  const heading=new Element('h1',{'data-i18n':'home.hero.headline'});heading.innerHTML=locales.en['home.hero.headline'];
  const description=new Element('p',{'data-i18n':'home.hero.introduction'});description.innerHTML=locales.en['home.hero.introduction'];
  const image=new Element('img',{'data-i18n-alt':keyMap['54'],alt:locales.en[keyMap['54']]});
  const docEvents={};const pending={};const store=new Map(Object.entries(stored));
  const document={documentElement:html,currentScript:{src:'https://portfolio.test/assets/preferences.js'},
    getElementById:id=>elements[id]||null,
    querySelector:selector=>({'.menu-toggle':menu,'.nav-links':nav,'.site-tools':tools,'.language-trigger':languageTrigger,'meta[name="theme-color"]':meta}[selector]||null),
    querySelectorAll:selector=>selector==='[data-i18n]'?[heading,description]:selector==='[data-i18n-alt]'?[image]:selector==='.language-choice'?choices:[],
    createElement:tag=>new Element(tag),
    addEventListener(type,fn){(docEvents[type]??=[]).push(fn)},
    dispatchEvent(event){for(const fn of docEvents[event.type]??[])fn(event)}
  };
  const media={matches:dark,listeners:[],addEventListener(type,fn){this.listeners.push(fn)},change(value){this.matches=value;this.listeners.forEach(fn=>fn())}};
  const window={matchMedia:query=>query.includes('reduced-motion')?{matches:true}:media};
  const sandbox={window,document,navigator:{language,languages:[language]},localStorage:{getItem:key=>{if(storageBlocked)throw Error('blocked');return store.get(key)??null},setItem:(key,value)=>{if(storageBlocked)throw Error('blocked');store.set(key,value)}},URL,CustomEvent:class{constructor(type,init){this.type=type;this.detail=init.detail}},fetch:async url=>{
    const lang=String(url).match(/locales\/(\w+)\.json/)[1];
    if(delayFetch)await new Promise(resolve=>pending[lang]=resolve);
    return {ok:true,json:async()=>locales[lang]};
  }};
  vm.createContext(sandbox);
  vm.runInContext(fs.readFileSync(`${rootPath}/assets/i18n-source.js`,'utf8'),sandbox);
  vm.runInContext(fs.readFileSync(`${rootPath}/assets/theme.js`,'utf8'),sandbox);
  vm.runInContext(fs.readFileSync(`${rootPath}/assets/preferences.js`,'utf8'),sandbox);
  vm.runInContext(fs.readFileSync(`${rootPath}/assets/site.js`,'utf8'),sandbox);
  return {elements,html,menu,nav,meta,tools,heading,image,store,media,window,pending,sandbox,choices,languageTrigger};
}
const flush=()=>new Promise(resolve=>setImmediate(resolve));
(async()=>{
  const app=setup({dark:true});await flush();assert.equal(app.html.dataset.theme,'dark');assert.equal(app.elements['theme-toggle'].checked,true);
  app.elements['theme-toggle'].checked=false;app.elements['theme-toggle'].dispatch('change');assert.equal(app.html.dataset.theme,'light');assert.equal(app.store.get('jae-portfolio-theme'),'light');
  app.media.change(true);assert.equal(app.html.dataset.theme,'light','explicit preference survives OS change');
  const automatic=setup();automatic.media.change(true);assert.equal(automatic.html.dataset.theme,'dark','automatic theme follows OS');
  for(const lang of ['ko','es','ja','zh','en']){
    app.elements['language-select'].value=lang;app.elements['language-select'].dispatch('change');await flush();
    assert.equal(app.html.lang,lang==='zh'?'zh-Hans':lang);
    assert.equal(app.heading.innerHTML,locales[lang]['home.hero.headline']);assert.equal(app.image.attrs.alt,locales[lang][keyMap['54']]);
    assert.equal(app.store.get('jae-portfolio-language'),lang);
    app.menu.dispatch('click');assert.equal(app.menu.attrs['aria-expanded'],'true');assert.notEqual(app.menu.attrs['aria-label'],'Open navigation');app.menu.dispatch('click');
  }
  const restored=setup({stored:{'jae-portfolio-theme':'dark','jae-portfolio-language':'ko'}});await flush();assert.equal(restored.html.lang,'ko');assert.equal(restored.html.dataset.theme,'dark');
  const blocked=setup({storageBlocked:true,language:'ja-JP'});await flush();assert.equal(blocked.html.lang,'ja');blocked.elements['theme-toggle'].checked=true;blocked.elements['theme-toggle'].dispatch('change');assert.equal(blocked.html.dataset.theme,'dark');
  const race=setup({delayFetch:true});race.elements['language-select'].value='ko';race.elements['language-select'].dispatch('change');race.elements['language-select'].value='es';race.elements['language-select'].dispatch('change');race.pending.es();await flush();race.pending.ko();await flush();assert.equal(race.html.lang,'es','latest language selection wins');
  const failed=setup();failed.sandbox.fetch=async()=>({ok:false});failed.elements['language-select'].value='ko';failed.elements['language-select'].dispatch('change');await flush();assert.equal(failed.html.lang,'en');assert.equal(failed.elements['language-select'].value,'en');assert.ok(failed.elements['preference-status'].textContent.includes('try again'));
  const demo=setup();const genres=['hyperpop','hiphop','indie','electronic'];const goals=['discover','engage','save'];
  for(const lang of ['en','ko','es','ja','zh']){
    demo.elements['language-select'].value=lang;demo.elements['language-select'].dispatch('change');await flush();
    for(const genre of genres)for(const goal of goals){
      demo.elements.genre.value=genre;demo.elements.goal.value=goal;demo.elements['sibyl-form'].dispatch('submit');
      assert.equal(demo.elements['sibyl-ideas'].children.length,3);assert.equal(demo.elements['sibyl-result'].hidden,false);
      for(const card of demo.elements['sibyl-ideas'].children)assert.ok(card.textContent.length>20&&!card.textContent.includes('undefined'));
      assert.ok(demo.elements['sibyl-result-title'].textContent.startsWith('82 Jae'));
    }
  }
  const before=demo.elements['sibyl-ideas'].textContent;demo.elements['language-select'].value='en';demo.elements['language-select'].dispatch('change');await flush();assert.notEqual(demo.elements['sibyl-ideas'].textContent,before,'visible results update on language change');
  demo.elements.artist.value='<img src=x onerror=alert(1)>';demo.elements['sibyl-form'].dispatch('submit');assert.ok(demo.elements['sibyl-result-title'].textContent.includes('<img src=x'),'artist input remains literal text');
  const noForm=setup({form:false,language:'es'});await flush();assert.equal(noForm.html.lang,'es','other pages retain preference behavior');
  const buttons=setup();buttons.elements['language-select'].open=true;buttons.choices[1].dispatch('click');await flush();assert.equal(buttons.html.lang,'ko');assert.equal(buttons.elements['language-select'].open,false);assert.equal(buttons.choices[1].attrs['aria-pressed'],'true');assert.equal(buttons.elements['current-language'].textContent,'한국어');
  for(const restriction of ['missing','legacy','throws']){
    const limited=setup({storageBlocked:true});
    delete limited.elements['theme-toggle'].listeners.change;
    if(restriction==='missing')limited.window.matchMedia=undefined;
    if(restriction==='throws')limited.window.matchMedia=()=>{throw Error('blocked API')};
    if(restriction==='legacy'){limited.media.addEventListener=undefined;limited.media.addListener=fn=>limited.media.listeners.push(fn)}
    vm.runInContext(fs.readFileSync(`${rootPath}/assets/theme.js`,'utf8'),limited.sandbox);
    limited.elements['theme-toggle'].checked=true;limited.elements['theme-toggle'].dispatch('change');
    assert.equal(limited.html.dataset.theme,'dark',restriction);
    limited.elements['theme-toggle'].checked=false;limited.elements['theme-toggle'].dispatch('change');assert.equal(limited.html.dataset.theme,'light');
  }
  const sparse=setup();sparse.window.portfolioI18n.registerTranslations('ko',{'nav':{'work':'작업'}});await sparse.window.portfolioI18n.setLanguage('ko');assert.equal(sparse.window.portfolioI18n.t('nav.work'),'작업');assert.equal(sparse.window.portfolioI18n.t('questions.truth_question'),locales.en['questions.truth_question']);
  const injected=setup();injected.window.portfolioI18n.registerTranslations('ko',{'questions.truth_question':'<img src=x onerror=alert(1)>','sibyl.plan.title':'bad {unexpected}'});await injected.window.portfolioI18n.setLanguage('ko');assert.equal(injected.window.portfolioI18n.t('questions.truth_question'),locales.en['questions.truth_question']);assert.equal(injected.window.portfolioI18n.t('sibyl.plan.title',{name:'A',goal:'B'}),'A / Example plan to B');
  const custom=setup();custom.window.portfolioI18n.setLoader(async()=>({'nav.work':'Custom translation'}));await custom.window.portfolioI18n.setLanguage('ko');assert.equal(custom.window.portfolioI18n.t('nav.work'),'Custom translation');
  assert.equal(custom.window.portfolioI18n.html('ui.language_selected',{label:'<script>',name:'&'}),'&lt;script&gt;: &amp;');
  console.log('PASS: theme defaults, OS changes, saved choices, five languages, alt text, menu labels, blocked storage, request races, loading failures, and all 60 localized demo combinations.');
})();
