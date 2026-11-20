#!/usr/bin/env node
import fs from 'node:fs';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const root=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const config=JSON.parse(fs.readFileSync(path.join(root,'i18n.config.json'),'utf8'));
const stateFile=path.join(root,'.i18n/state.json');
const hash=text=>crypto.createHash('sha256').update(text).digest('hex');
const write=(file,data)=>{fs.mkdirSync(path.dirname(file),{recursive:true});fs.writeFileSync(file,JSON.stringify(data,null,2)+'\n')};
export function flatten(value,prefix='',out=Object.create(null)){
 for(const [key,entry] of Object.entries(value)){
  const full=prefix?prefix+'.'+key:key;
  if(full.split('.').some(part=>['__proto__','constructor','prototype'].includes(part)))throw Error('Unsafe translation key');
  if(typeof entry==='string')out[full]=entry;
  else if(entry && typeof entry==='object' && !Array.isArray(entry))flatten(entry,full,out);
  else throw Error(`Translation must be a string: ${full}`);
 }
 return out;
}
const catalog=lang=>flatten(JSON.parse(fs.readFileSync(path.join(root,'locales',lang+'.json'),'utf8')));
const loadState=()=>fs.existsSync(stateFile)?JSON.parse(fs.readFileSync(stateFile,'utf8')):{version:1,translations:{}};
const placeholders=text=>[...text.matchAll(/\{([A-Za-z]\w*)\}/g)].map(match=>match[1]).sort();
const tags=text=>[...text.matchAll(/<\/?[a-z][^>]*>/gi)].map(match=>match[0]).sort();
function validateMarkup(value,key){
 const stack=[];
 for(const match of value.matchAll(/<\/?([a-z][a-z0-9]*)\b[^>]*>/gi)){
  const token=match[0];const tag=match[1].toLowerCase();
  if(!['span','strong','em','br','a'].includes(tag))throw Error(`Unsupported markup in ${key}: ${tag}`);
  if(/\son\w+\s*=|(?:javascript|data):|<[^>]*<|\s(?:src|srcdoc)\s*=/i.test(token))throw Error(`Unsafe markup in ${key}`);
  if(token.startsWith('</')){if(stack.pop()!==tag)throw Error(`Unbalanced markup in ${key}`)}
  else if(tag!=='br')stack.push(tag);
 }
 if(stack.length)throw Error(`Unclosed markup in ${key}`);
}
function validateTranslation(source,value,key){
 validateMarkup(value,key);
 if(!value)return;
 if(JSON.stringify(placeholders(source))!==JSON.stringify(placeholders(value)))throw Error(`Placeholder mismatch: ${key}`);
 if(JSON.stringify(tags(source))!==JSON.stringify(tags(value)))throw Error(`Formatting mismatch: ${key}`);
}
function inspect({infer=false}={}){
 const source=catalog(config.sourceLanguage);const state=loadState();const catalogs={en:source};const issues={};
 for(const [key,value]of Object.entries(source)){if(!value)throw Error(`English source is empty: ${key}`);validateMarkup(value,key)}
 for(const lang of config.languages.filter(lang=>lang!==config.sourceLanguage)){
  const values=catalog(lang);catalogs[lang]=values;state.translations[lang]??={};issues[lang]={missing:[],stale:[]};
  for(const [key,value]of Object.entries(source)){
   const translated=values[key];const fingerprint=hash(value);let record=state.translations[lang][key];
   if(!translated){issues[lang].missing.push(key);continue}
   validateMarkup(translated,lang+':'+key);
   // A directly edited translated value is considered synchronized with the current English source.
   if(infer && (!record || record.translationHash!==hash(translated))){record={sourceHash:fingerprint,translationHash:hash(translated)};state.translations[lang][key]=record}
   if(!record || record.sourceHash!==fingerprint){issues[lang].stale.push(key);continue}
   if(JSON.stringify(placeholders(value))!==JSON.stringify(placeholders(translated)))throw Error(`Placeholder mismatch: ${lang}:${key}`);
   if(JSON.stringify(tags(value))!==JSON.stringify(tags(translated)))throw Error(`Formatting mismatch: ${lang}:${key}`);
  }
 }
 return {source,state,catalogs,issues};
}
function build(){
 const result=inspect({infer:true});const {source,state,catalogs,issues}=result;
 const compiled={};
 for(const lang of config.languages){
  const unusable=new Set([...(issues[lang]?.missing||[]),...(issues[lang]?.stale||[])]);
  compiled[lang]=Object.fromEntries(Object.keys(source).map(key=>[key,unusable.has(key)?source[key]:catalogs[lang][key]??source[key]]));
 }
 const assetInputs=['preferences.js','site.js','theme.js','style.css','view-switch.js','view-switch.css','machine.css'].map(file=>fs.readFileSync(path.join(root,'dist/assets',file),'utf8'));
 const revision=hash(JSON.stringify({config,compiled,issues,assetInputs})).slice(0,14);
 const manifest={revision,sourceLanguage:config.sourceLanguage,secondaryLanguage:config.secondaryLanguage,issues};
 const destination=path.join(root,'dist/assets/locales');
 for(const [lang,values]of Object.entries(compiled))write(path.join(destination,lang+'.json'),values);
 write(path.join(root,'dist/assets/i18n-manifest.json'),manifest);
 const bundle='/* Generated from locales/en.json; edit that source file, not this output. */\nwindow.PORTFOLIO_I18N='+JSON.stringify({config,manifest,source})+';\n';
 fs.writeFileSync(path.join(root,'dist/assets/i18n-source.js'),bundle);
 write(stateFile,state);
 const rendered=spawnSync('python',['scripts/render-pages.py'],{cwd:root,encoding:'utf8'});
 if(rendered.status!==0)throw Error(rendered.stderr||rendered.stdout);
 process.stdout.write(rendered.stdout);
 const machine=spawnSync('python',['scripts/build-machine-view.py'],{cwd:root,encoding:'utf8'});
 if(machine.status!==0)throw Error(machine.stderr||machine.stdout);
 process.stdout.write(machine.stdout);
 console.log(`${Object.keys(source).length} source keys; catalog revision ${revision}.`);
 for(const [lang,issue]of Object.entries(issues))if(issue.missing.length||issue.stale.length)console.log(`${lang}: ${issue.missing.length} missing, ${issue.stale.length} stale (English fallback).`);
 return result;
}
function status(){const {issues}=inspect();console.log(JSON.stringify(issues,null,2));}
function check(){
 const {source,issues}=inspect();
 const templates=[];const walk=directory=>{for(const entry of fs.readdirSync(directory,{withFileTypes:true})){const file=path.join(directory,entry.name);if(entry.isDirectory())walk(file);else if(file.endsWith('.html'))templates.push(file)}};walk(path.join(root,'src/pages'));
 for(const file of templates){
  const html=fs.readFileSync(file,'utf8');
  for(const match of html.matchAll(/data-i18n(?:-[\w-]+)?="([^"]+)"/g))if(!(match[1]in source))throw Error(`Unknown page key: ${match[1]}`);
 }
 if(Object.values(issues).some(issue=>issue.missing.length||issue.stale.length))throw Error('Translations need updates. Run status to see only the affected keys.');
 console.log(`PASS: ${Object.keys(source).length} keys, ${config.languages.length} locales, placeholders, markup, and page references.`);
}
const [command,...args]=process.argv.slice(2);
try{
 if(command==='build')build();
 else if(command==='check')check();
 else if(command==='status')status();
 else if(command==='set'){
  const [lang,key,value]=args;if(!config.languages.includes(lang)||!key||value===undefined)throw Error('Usage: set <language> <key> <text>');
  const values=catalog(lang);const english=catalog(config.sourceLanguage);
  if(lang===config.sourceLanguage){if(!value)throw Error('English source cannot be empty');validateMarkup(value,key)}
  else{if(!(key in english))throw Error('English key must exist first');validateTranslation(english[key],value,lang+':'+key)}
  values[key]=value;write(path.join(root,'locales',lang+'.json'),values);
  if(lang!==config.sourceLanguage){const state=loadState();state.translations[lang]??={};state.translations[lang][key]={sourceHash:hash(english[key]),translationHash:hash(value)};write(stateFile,state)}
  build();
 }else if(command==='accept'){
  const [lang,...keys]=args;if(!config.languages.includes(lang)||lang===config.sourceLanguage||!keys.length)throw Error('Usage: accept <language> <key...>');
  const english=catalog(config.sourceLanguage),values=catalog(lang),state=loadState();state.translations[lang]??={};
  for(const key of keys){if(!(key in english)||!values[key])throw Error(`Missing key ${key}`);validateTranslation(english[key],values[key],lang+':'+key);state.translations[lang][key]={sourceHash:hash(english[key]),translationHash:hash(values[key])}}
  write(stateFile,state);build();
 }else if(command==='export'){
  const directory=path.resolve(root,args[0]||'translation-export');const english=catalog(config.sourceLanguage);
  for(const lang of config.languages)write(path.join(directory,lang+'.json'),catalog(lang));
  write(path.join(directory,'source-fingerprints.json'),Object.fromEntries(Object.entries(english).map(([key,value])=>[key,hash(value)])));
  console.log('Exported flat JSON catalogs and English fingerprints to '+directory);
 }else if(command==='import'){
  const [lang,file,snapshot]=args;if(!config.languages.includes(lang)||lang===config.sourceLanguage||!file)throw Error('Usage: import <language> <json-file> [source-fingerprints.json]');
  const incoming=flatten(JSON.parse(fs.readFileSync(path.resolve(file),'utf8'))),english=catalog(config.sourceLanguage),values=catalog(lang),state=loadState();state.translations[lang]??={};
  const exported=snapshot?JSON.parse(fs.readFileSync(path.resolve(snapshot),'utf8')):null;
  for(const [key,value]of Object.entries(incoming)){
   if(!(key in english))continue;validateMarkup(value,key);
   if(exported && exported[key]!==hash(english[key])){console.log('Skipped translation for changed source: '+key);continue}
   validateTranslation(english[key],value,lang+':'+key);
   if(value===values[key]&&!exported)continue;
   values[key]=value;state.translations[lang][key]={sourceHash:hash(english[key]),translationHash:hash(value)};
  }
  write(path.join(root,'locales',lang+'.json'),values);write(stateFile,state);build();
 }else throw Error('Commands: build | check | status | set | accept | export | import');
}catch(error){console.error(error.message);process.exitCode=1;}
