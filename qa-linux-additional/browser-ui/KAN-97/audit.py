from pathlib import Path
import json,hashlib
R=Path(__file__).parent;rows=[]
for kind in ['official','combined','firefox','patched']:
 d=json.loads((R/(kind+'.json')).read_text());assert not d.get('error'),d.get('error');assert d['metadata']['focusTestMode']==False
 for c in d['observations']:
  own=c['expectedTarget'];untargeted=[k for k in ['root','overflow','frame'] if k!=own];unchanged=all(abs(c['before'][k]['top']-c['after'][k]['top'])<=1 for k in untargeted)
  row={'product':kind,'target':c['target'],'mode':c['mode'],'direction':c['direction'],'endpoint':c['atExpectedEndpoint'],'untargetedUnchanged':unchanged,'popups':c['events']['popups']};rows.append(row)
  if kind=='patched' or c['mode']=='keyboard':assert c['atExpectedEndpoint'] and unchanged,row
  if c['mode']=='gesture':assert c['events']['popups']==0,row
 if kind!='firefox':
  count=(R/'logs'/(kind+'-browser.log')).read_text(errors='replace').count('Sending scroll command to content:');assert count==8,(kind,count)
summary={'observations':len(rows),'officialGesturePass':sum(r['endpoint'] for r in rows if r['product']=='official' and r['mode']=='gesture'),'officialGestureCases':8,'combinedGesturePass':sum(r['endpoint'] for r in rows if r['product']=='combined' and r['mode']=='gesture'),'combinedGestureCases':8,'patchedGesturePass':sum(r['endpoint'] for r in rows if r['product']=='patched' and r['mode']=='gesture'),'patchedGestureCases':8,'nativeKeyboardAllPass':all(r['endpoint'] and r['untargetedUnchanged'] for r in rows if r['mode']=='keyboard'),'patchedUntargetedAllUnchanged':all(r['untargetedUnchanged'] for r in rows if r['product']=='patched'),'gestureContextMenusAllZero':all(r['popups']==0 for r in rows if r['mode']=='gesture'),'dispatchesPerFloorpProduct':8,'unitBefore':'5pass/2assertfail old-route adapter, not literal old-module execution','unitAfter':'7pass/0fail actual source helper in browser','rows':rows}
(R/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps({k:v for k,v in summary.items() if k!='rows'}))
canonical=Path('/workspace/qa-linux-22/browser-ui/products/floorp');patch=json.loads((R/'candidate-patch.json').read_text());hashes={}
for name in ['floorp','libxul.so','omni.ja']:
 h=hashlib.sha256()
 with (canonical/name).open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 hashes[name]=h.hexdigest()
assert hashes['omni.ja']==patch['archiveBefore'];assert hashes['floorp']=='4f05c1330b38e50040440be6a3b6e0e7a5b52bc79edb5c5478d978c81f5a603d';assert hashes['libxul.so']=='760ff8a689f52c9b1f3c89a45ea8813e59f187990caed747e4a7680020c8dbc7';(R/'canonical-after-hashes.json').write_text(json.dumps(hashes,indent=2))
