from pathlib import Path
import json,hashlib
r=Path(__file__).resolve().parents[1]
def load(p):return json.loads((r/p).read_text())
s=load('summary.json');assert len(s['qualifiedFloorpCases'])==2 and len(s['unqualifiedFirefoxCases'])==2
for c in s['qualifiedFloorpCases']:
 case=c['case'];rows=load('raw/'+case+'/paste-results.json');assert len(rows)==14
 for row in rows:
  for v in row['snapshots']:
   assert v['text']==s['url'];events=[e for e in v['log'] if e['type']=='paste'];assert events and all(e['trusted'] for e in events)
   if row['explicitOpacityControl']:assert v['opacityZero'] and any('text/html' in e['types'] for e in events)
   else:assert not v['opacityZero'] and v['anchors']<=1
   if row['id'].endswith(('plain','plain-shift','rich-shift','textarea-recopy','subject-recopy')):assert all(e['types']==['text/plain'] for e in events)
 for target in ['plain','rooster']:
  a=next(x for x in rows if x['id']==target+'-textarea-recopy')['snapshots'][-1];b=next(x for x in rows if x['id']==target+'-subject-recopy')['snapshots'][-1];assert a['text']==b['text'] and a['anchors']==b['anchors']
for c in s['unqualifiedFirefoxCases']:
 case=c['case'];rows=load('raw/'+case+'/paste-results.json');assert len(rows)==1 and not any(e['type']=='paste' for e in rows[0]['snapshots'][-1]['log']);assert not load('raw/'+case+'/result-input-path-diagnostic.json')['value']['focused'];p=load('raw/'+case+'/result-tou-state.json')['value'];assert not p['bypass'] and not p['bypassUser'] and p['acceptedVersion']==0 and not p['acceptanceUser'];assert (r/'raw'/case/'input-path-diagnostic.png').is_file()
for c in [v['case'] for v in s['qualifiedFloorpCases']+s['unqualifiedFirefoxCases']]:
 assert load('raw/'+c+'/shutdown.json')=={'requestedNormalQuit':True,'exitCode':0};v=load('raw/'+c+'/ready.json')['value'];assert v['updatePin'] and v['recommended'] is False;assert v['protections']['sandbox']==6 and not v['protections']['sandboxUser'] and not v['protections']['signatureUser']
assert all(x['allUnchangedFromRecordedSnapshot'] for x in load('product-hash-final.json'))
assert all(x['matchesPublishedHash'] for x in load('acquisition/verification.json'))
assert load('source-analysis/proton/fixture-version-comparison.json')['matchesPublishedDependencyGraph']
for x in load('export-manifest.json')['files']:
 p=r/x['path'];assert p.stat().st_size==x['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
print('24 ordinary Floorp native paste observations +4explicit opacity controls verified;2Firefox input-blocked controls remain unqualified;4normal exits/product/export hashes verified')
