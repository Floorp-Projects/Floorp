from pathlib import Path
import json,hashlib
R=Path(__file__).parent;colors=[[255,0,0],[0,255,0],[0,0,255],[255,255,0]];cases=[]
for design in ['proton','fluerial','lepton']:
 for f in sorted(R.glob(design+'-*.json')):
  d=json.loads(f.read_text());i=d['images'];local=next(x for x in i if x['title']=='QA local favicon');assert all(x['naturalWidth']>0 and x['complete'] and x['css']['display']!='none' and x['css']['visibility']=='visible' for x in i);assert local['paintSamples']==colors;assert d['environment']['design']==design
  cases.append({'file':f.name,'design':design,'images':len(i),'localFourColorPaint':'pass','decoded':'pass','stage':f.stem})
summary={'conditions':len(cases),'imageChecks':sum(c['images'] for c in cases),'cases':cases,'scope':'Floorp Start only; classic New Tab whole-page initialization limitation also seen in Firefox','protectedPreferencesUnchanged':True};(R/'summary.json').write_text(json.dumps(summary,indent=2))
row={'key':'KAN-37','github':'https://github.com/Floorp-Projects/Floorp/issues/2754','sourceSha':'29a3eb23145da90706d8a94441373278521cc737','productVersion':'Floorp12.21.1@159.0.1 Build20261103153232; Firefox159.0.1 Build20261102144711','environment':'Debian13.6 private Xvfb99/101 X11 synthetic W3C/XTest; approved session proxy and environment CA normal trust','testCommand':'QA_PROXY=1 python start.py; PYTHONPATH=/workspace/qa-linux-22/browser-ui/python python capture.py <verified-design>-<stage>; local-fixture.py supplies explicit persisted data fixture','result':'partially-tested: Floorp Start '+str(len(cases))+' conditions/'+str(summary['imageChecks'])+' image-decode checks pass; local four-color actual paint pass each; original classic-thumbnail condition not established','remainingConditions':['Original reporter classic New Tab profile and existing tiles','Native classic renderer initialization limitation (whole-page root0 in Floorp and Firefox controls)','Original Linux desktop conditions and Windows; real physical UI input not tested'],'existingPr':None,'relatedPr':'https://github.com/Floorp-Projects/Floorp/pull/3003 (separate redesign, not validated fix)','newPr':None,'jiraCommentId':None,'evidence':['report.md','summary.json','native-control.json','capture.py','start.py','local-fixture.py','fixture-favicon.png']};(R/'matrix.json').write_text(json.dumps([row],indent=2))
products={}
for n in ['floorp','firefox']:
 for rel in [n,'browser/omni.ja']:
  p=Path('/workspace/qa-linux-22/browser-ui/products')/n/rel;h=hashlib.sha256()
  with p.open('rb') as q:
   for b in iter(lambda:q.read(1024*1024),b''):h.update(b)
  products[n+'/'+rel]=h.hexdigest()
(R/'product-hashes.json').write_text(json.dumps(products,indent=2));(R/'sha256.json').write_text(json.dumps({f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in R.iterdir() if f.is_file() and f.name!='sha256.json'},indent=2));print(json.dumps(summary))
