from pathlib import Path
import json,collections,hashlib
r=Path(__file__).resolve().parents[1]
def load(p):return json.loads((r/p).read_text())
for case in ['baseline','candidate']:
 root='raw/'+case+'/';a=load(root+'http-requests.json');m=load(root+'measurements.json')
 for x in m:
  assert 0<=x['fromIndex']<=x['toIndex']<=len(a)
  counts=dict(collections.Counter(v['path'] for v in a[x['fromIndex']:x['toIndex']]))
  assert counts==x['counts'],(case,x['phase'])
 assert load(root+'shutdown.json')=={'requestedNormalQuit':True,'exitCode':0}
 ready=load(root+'ready.json')['value'];assert ready['version']=='159.0.1' and ready['buildID']=='20261103153232'
 assert ready['protections']=={'sandbox':6,'sandboxUser':False,'signatureRequired':False,'signatureUser':False}
 assert ready['pwa']['enabled']
 phases={x['phase']:x for x in m}
 for name in ['no-manifest','iframe-only','ineligible-about']:assert not phases[name]['state']['value']['pwa']['actionPresent']
 if case=='candidate':
  for name in ['background-B','subframes-four','other-window-C','two-window-background']:
   paths=['/manifest/A.json','/manifest/C.json'] if name=='two-window-background' else ['/manifest/A.json'];assert sum(phases[name]['counts'].get(p,0) for p in paths)==0
  for name in ['select-B','reload-B']:assert phases[name]['counts'].get('/manifest/B.json',0)>0 and phases[name]['state']['value']['pwa']['actionPresent']
  panel=phases['panel-metadata']['actionResult']['value'];assert panel['panelState']=='open' and panel['title']=='KAN77 A'
p=load('provenance/package-verification.json');assert not p['changedResources'];assert load('provenance/product-hash-after.json')['allUnchanged']
for x in load('export-manifest.json')['files']:
 p=r/x['path'];assert p.stat().st_size==x['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
print('HTTP phase indices/counts, unrelated fetch suppression, selected fetch/UI, negative UI, stock protection observations, normal exits and export hashes verified')
