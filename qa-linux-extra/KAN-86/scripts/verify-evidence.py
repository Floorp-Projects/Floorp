from pathlib import Path
import json,hashlib
r=Path(__file__).resolve().parents[1]
def load(p):return json.loads((r/p).read_text())
summary=load('raw/summary.json');assert len(summary)==6
for row in summary:
 assert row['shutdown']=={'requestedNormalQuit':True,'exitCode':0}
 case=row['case'];a=load('raw/'+case+'/cpu-samples.json');assert len(a)==row['sampleCount'];assert a
 v=row['ready']['value'];assert v['protections']=={'sandbox':6,'sandboxUser':False,'signatureRequired':False,'signatureUser':False}
assert load('update-chain/product-verification.json')['allMatch']
assert (r/'update-chain/patch/update.status').read_text().strip()=='succeeded'
v=load('raw/mar-to18/ready.json')['value'];assert v['version']=='156.0.1' and v['buildID']=='20260916130640';assert v['startup']=={'isFirstRun':False,'isUpdated':True}
assert any(t['url']=='about:welcome?releaseNotes=1' for t in v['tabs'])
for case in ['mar-to18','current-prompt-save']:
 v=load('raw/'+case+'/result-after-save.json')['value'];assert v['prefs']['confirmed']['value'];assert not any(t['url'].startswith('about:welcome') for t in v['tabs'])
v=load('raw/current-update-fixture/result-after-dismiss.json')['value'];assert not v['prefs']['confirmed']['value'];assert not any(t['url'].startswith('about:welcome') for t in v['tabs'])
v=load('raw/mar18-normal-restart/ready.json')['value'];assert not v['startup']['isUpdated'] and v['prefs']['confirmed']['value']
assert load('raw/mar18-normal-restart/result-blog-response.json')['value']['navigation']['responseStatus']==200
for row in load('export-manifest.json')['files']:
 p=r/row['path'];assert p.stat().st_size==row['bytes'];assert hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256'],str(p)
print('6 normal product sessions; native MAR success +46 matching files; expected save/dismiss/restart observations and export hashes verified')
