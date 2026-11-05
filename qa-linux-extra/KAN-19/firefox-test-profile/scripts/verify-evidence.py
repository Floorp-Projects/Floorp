from pathlib import Path
import json,hashlib
r=Path(__file__).resolve().parents[1]
def load(p):return json.loads((r/p).read_text())
s=load('summary.json');assert len(s['qualifiedFirefoxCases'])==2 and len(s['comparisons'])==28
for info in s['qualifiedFirefoxCases']:
 case=info['case'];p=r/'raw'/case;rows=json.loads((p/'paste-results.json').read_text());assert len(rows)==14
 baseline='floorp-current' if 'current' in case else 'floorp-reported';ref=json.loads((r.parent/'raw'/baseline/'paste-results.json').read_text())
 for row,b in zip(rows,ref):
  assert row['id']==b['id']
  for v in row['snapshots']:
   ev=[e for e in v['log'] if e['type']=='paste'];assert ev and all(e['trusted'] for e in ev);assert v['text']=='https://example.org/kan19/status/123/video/1';assert v['opacityZero']==row['explicitOpacityControl']
   if not row['explicitOpacityControl']:assert v['anchors']<=1
  v=row['snapshots'][-1];bv=b['snapshots'][-1];assert v['html']==bv['html'] and v['text']==bv['text'] and v['anchors']==bv['anchors'] and v['opacityZero']==bv['opacityZero'];assert [e['types'] for e in v['log'] if e['type']=='paste']==[e['types'] for e in bv['log'] if e['type']=='paste']
 for name in ['ready.json','result-product-final.json']:
  v=json.loads((p/name).read_text())['value'];assert v['TOU']=={'notificationTestPref':True,'acceptedVersion':0,'acceptedDate':'0','acceptanceUser':False,'dateUser':False};assert v['protections']=={'sandbox':6,'sandboxUser':False,'signatureRequired':True,'signatureUser':False};assert v['recommended'] is False and v['updatePin']
 assert json.loads((p/'shutdown.json').read_text())=={'requestedNormalQuit':True,'exitCode':0}
assert all(x['unchanged'] and x['entries']==51 for x in load('product-hash-final.json'))
source=load('official-test-setting.json');assert len(source['modules'])==4
for x in load('export-manifest.json')['files']:
 p=r/x['path'];assert p.stat().st_size==x['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
print('2official Firefox testprofiles:28native pastes,28exact finalHTML/MIME matches to Floorp,consent0/stockchecks,2normal exits,51entry prefixes unchanged; hashes verified')
