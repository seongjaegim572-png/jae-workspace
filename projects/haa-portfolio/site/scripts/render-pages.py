"""Render the static English fallback from one canonical catalog. No HTML reserialization."""
from html.parser import HTMLParser
from pathlib import Path
import json,re,html,sys
ROOT=Path(__file__).resolve().parents[1]
class Node:
 def __init__(self,tag,attrs,start,inner):
  self.tag,self.attrs,self.start,self.inner=tag,dict(attrs),start,inner
  self.end=inner
class Parser(HTMLParser):
 def __init__(self,raw):
  super().__init__(convert_charrefs=False);self.raw=raw;self.lines=[0];self.nodes=[];self.stack=[]
  for match in re.finditer('\n',raw):self.lines.append(match.end())
  self.feed(raw)
 def position(self):
  line,col=self.getpos();return self.lines[line-1]+col
 def handle_starttag(self,tag,attrs):
  start=self.position();node=Node(tag,attrs,start,start+len(self.get_starttag_text()));self.nodes.append(node)
  if tag not in ('area','base','br','col','embed','hr','img','input','link','meta','param','source','track','wbr'):self.stack.append(node)
 def handle_endtag(self,tag):
  for i in range(len(self.stack)-1,-1,-1):
   if self.stack[i].tag==tag:self.stack[i].end=self.position();self.stack=self.stack[:i];break

def materialize(raw,catalog,blank=False):
 parser=Parser(raw);edits=[]
 for node in parser.nodes:
  starttag=raw[node.start:node.inner];tag=starttag
  for name,key in node.attrs.items():
   if name=='data-i18n':
    if key not in catalog:raise ValueError(f'Missing English key: {key}')
    edits.append((node.inner,node.end,'' if blank else catalog[key]))
   elif name.startswith('data-i18n-'):
    target=name[len('data-i18n-'):]
    if target=='initial-value':target='value'
    if target not in ('alt','aria-label','placeholder','content','title','value'):continue
    if key not in catalog:raise ValueError(f'Missing English attribute key: {key}')
    value='' if blank else html.escape(catalog[key],quote=True)
    pattern=r'(?<![\w-])'+re.escape(target)+r'=(?:"[^"]*"|\x27[^\x27]*\x27)'
    if re.search(pattern,tag):tag=re.sub(pattern,lambda _ :target+'="'+value+'"',tag,count=1)
    else:tag=tag[:-1]+' '+target+'="'+value+'">'
  if tag!=starttag:edits.append((node.start,node.inner,tag))
 for start,end,text in sorted(edits,reverse=True):raw=raw[:start]+text+raw[end:]
 return raw

if __name__=='__main__':
 catalog=json.loads((ROOT/'locales/en.json').read_text())
 blank='--blank-templates' in sys.argv
 revision=None if blank else json.loads((ROOT/'dist/assets/i18n-manifest.json').read_text())['revision']
 for source in sorted((ROOT/'src/pages').rglob('*.html')):
  raw=materialize(source.read_text(),catalog,blank)
  if blank:source.write_text(raw);continue
  # Content-derived URLs refresh the catalogs and scripts only when inputs actually change.
  raw=re.sub(r'(assets/(?:style\.css|preferences\.js|site\.js|theme\.js|i18n-source\.js))(?:\?v=[^"\s>]*)?',lambda m:m[1]+'?v='+revision,raw)
  target=ROOT/'dist'/source.relative_to(ROOT/'src/pages');target.parent.mkdir(parents=True,exist_ok=True);target.write_text(raw)
 print('Prepared blank templates.' if blank else 'Rendered six pages from the English catalog.')
