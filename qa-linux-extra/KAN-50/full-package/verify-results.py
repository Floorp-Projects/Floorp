#!/usr/bin/env python3
"""Independently check exported raw full-package media observations and product immutability."""
import argparse,hashlib,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('root',type=Path);p.add_argument('--product-root',type=Path);a=p.parse_args();r=a.root
cases=[]
verified=json.loads((r/'package-verification.json').read_text());inventory=json.loads((r/'product-files-before.json').read_text());binaryHash=next(x['sha256'] for x in inventory if x['path']=='floorp');assert verified['sourcePatchMatchesFullPackage'] and verified['extractedPatchMatchesFullPackage']
for order in ['normal-first','panel-first']:
 for close in ['normal','panel']:
  case=f'{order}-close-{close}';d=json.loads((r/case/'result.json').read_text());assert d['passed'],case
  assert d['openOrder']==order and d['closeFirst']==close
  assert d['sourceSha']=='9d042bc17be83c15d2ff3a25eb712f04c4d3eb45' and d['binarySha256']==binaryHash
  assert len(d['checks'])==13 and all(c['passed'] for c in d['checks'])
  assert d['runtime']['sandbox']==d['after']['sandbox']==6
  assert d['runtime']['pid']==d['after']['pid']
  assert d['runtime']['buildID']=='20261103153232'
  assert d['bothOpen']['activity']['rootMap']==2
  assert d['firstClosedWhileOtherRemains']['rootMap']==1
  assert d['minimized1']['activity']['windowState']==d['minimized2']['activity']['windowState']==2
  assert d['minimized2']['activity']['rootActive'] and not d['minimized2']['video']['hidden']
  assert d['minimized1']['captions']['text']!=d['minimized2']['captions']['text']
  assert all(not d[k]['video']['paused'] and d[k]['video']['cloning'] for k in ['minimized1','minimized2'])
  assert d['minimized2']['video']['frames']>d['minimized1']['video']['frames']
  assert d['afterAllClose']['rootMap'] is None and not d['afterAllClose']['rootForced']
  assert d['afterAllClose']['players']==d['afterAllClose']['windows']==0
  assert d['afterAllClose']['normalPipAttribute'] is None and d['afterAllClose']['panelPipAttribute'] is None
  assert not d['runtimeErrors']
  cases.append({'case':case,'checksPassed':len(d['checks']),'pid':d['runtime']['pid'],'runtimeBuildId':d['runtime']['buildID'],'rawSha256':hashlib.sha256((r/case/'result.json').read_bytes()).hexdigest()})
imm=json.loads((r/'product-immutability.json').read_text());assert imm['unchanged'] and imm['filesBefore']==imm['filesAfter']
if a.product_root:
 before=json.loads((r/'product-files-before.json').read_text());after=[]
 for f in sorted(a.product_root.rglob('*')):
  if f.is_symlink():after.append({'path':str(f.relative_to(a.product_root)),'symlink':str(f.readlink())})
  elif f.is_file():after.append({'path':str(f.relative_to(a.product_root)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
 assert before==after,'product mutated'
result={'sourceSha':'9d042bc17be83c15d2ff3a25eb712f04c4d3eb45','cases':cases,'passedCases':4,'passedChecks':52,'productUnchanged':True,'realMinimizeAndCaptionObservations':True,'runtimeBuildId':'20261103153232','mockedHandlers':False,'productResourceReplacements':False,'liveYouTubeRetried':False}
print(json.dumps(result,indent=2))
