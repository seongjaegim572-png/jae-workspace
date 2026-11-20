import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import crypto from 'node:crypto';
import {fileURLToPath} from 'node:url';
import {spawnSync} from 'node:child_process';
const original=path.resolve(path.dirname(fileURLToPath(import.meta.url)),'..');
const temporary=fs.mkdtempSync(path.join(os.tmpdir(),'haa-i18n-test-'));
const json=file=>JSON.parse(fs.readFileSync(path.join(temporary,file),'utf8'));
const checksum=file=>crypto.createHash('sha256').update(fs.readFileSync(path.join(temporary,file))).digest('hex');
const run=(...args)=>{const result=spawnSync('node',['scripts/i18n.mjs',...args],{cwd:temporary,encoding:'utf8'});assert.equal(result.status,0,result.stderr||result.stdout);return result.stdout};
try{
 for(const directory of ['locales','.i18n','scripts','src','dist'])fs.cpSync(path.join(original,directory),path.join(temporary,directory),{recursive:true});
 fs.copyFileSync(path.join(original,'i18n.config.json'),path.join(temporary,'i18n.config.json'));
 const styles=checksum('dist/assets/style.css');const photo=checksum('dist/assets/jae-portrait.jpeg');
 const korean=json('locales/ko.json');const english=json('locales/en.json');const firstRevision=json('dist/assets/i18n-manifest.json').revision;
 run('set','en','nav.work','Selected work');
 const report=JSON.parse(run('status'));
 for(const issue of Object.values(report)){assert.deepEqual(issue.stale,['nav.work']);assert.deepEqual(issue.missing,[])}
 assert.equal(json('dist/assets/locales/ko.json')['nav.work'],'Selected work','stale translations use English');
 assert.deepEqual(json('locales/ko.json'),korean,'source edit does not rewrite translations');
 assert.ok(fs.readFileSync(path.join(temporary,'dist/index.html'),'utf8').includes('data-i18n="nav.work">Selected work</a>'));
 assert.notEqual(json('dist/assets/i18n-manifest.json').revision,firstRevision,'cache version changes with source');
 run('set','ko','nav.work','주요 작업');
 assert.deepEqual(JSON.parse(run('status')).ko.stale,[]);
 const updated=json('locales/ko.json');assert.equal(updated['nav.work'],'주요 작업');for(const [key,value]of Object.entries(korean))if(key!=='nav.work')assert.equal(updated[key],value);
 assert.equal(styles,checksum('dist/assets/style.css'));assert.equal(photo,checksum('dist/assets/jae-portrait.jpeg'));
 const nested={nav:{about:'소개'}};fs.writeFileSync(path.join(temporary,'import.json'),JSON.stringify(nested));run('import','ko',path.join(temporary,'import.json'));assert.equal(json('locales/ko.json')['nav.about'],'소개');
 run('export',path.join(temporary,'export'));assert.equal(json('export/source-fingerprints.json')['nav.work'].length,64);
 run('set','en','nav.work','Work v2');fs.writeFileSync(path.join(temporary,'returned.json'),JSON.stringify({'nav.work':'오래된 원문 번역'}));
 run('import','ko',path.join(temporary,'returned.json'),path.join(temporary,'export/source-fingerprints.json'));
 assert.equal(json('locales/ko.json')['nav.work'],'주요 작업','outdated export cannot overwrite current translation');
 const savedKorean=json('locales/ko.json');const invalid=spawnSync('node',['scripts/i18n.mjs','set','ko','nav.work','Bad {unexpected}'],{cwd:temporary,encoding:'utf8'});assert.equal(invalid.status,1);assert.deepEqual(json('locales/ko.json'),savedKorean,'rejected updates do not modify the catalog');
 const data=json('locales/en.json');data['common.new_example']='New source';fs.writeFileSync(path.join(temporary,'locales/en.json'),JSON.stringify(data));run('build');assert.deepEqual(JSON.parse(run('status')).ko.missing,['common.new_example']);assert.equal(json('dist/assets/locales/ko.json')['common.new_example'],'New source');
 const before=json('dist/assets/i18n-manifest.json').revision;run('build');assert.equal(json('dist/assets/i18n-manifest.json').revision,before,'unchanged rebuild is deterministic');
 console.log('PASS: one-key source edits, stale detection, English fallback, Korean-only updates, unchanged design/assets, nested imports, export fingerprints, outdated-import protection, and deterministic builds.');
}finally{fs.rmSync(temporary,{recursive:true,force:true})}
