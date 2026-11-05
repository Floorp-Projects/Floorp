from pathlib import Path
import json,hashlib
import sys
try:from PIL import Image
except ImportError:
 sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages');from PIL import Image
r=Path(__file__).resolve().parents[1]
def load(p):return json.loads((r/p).read_text())
s=load('summary.json');assert len(s['currentNativePaints'])==7
expected=[[255,0,0],[0,255,0],[0,0,255],[255,255,0]]
for row in s['currentNativePaints']:
 assert row['observation']['mainClass']=='newtab-main' and row['observation']['rootLength']>1000 and row['observation']['style']['backgroundImageIsDataPng'];im=Image.open(r/row['screenshotFile']).convert('RGB');pixels=[list(im.getpixel(tuple(q))) for q in row['screenSampleCoordinates']];assert pixels==expected==row['pixels'];assert row['allExact']
for c in s['qualifiedCurrentAndControlSessions']:
 assert load('raw/'+c+'/shutdown.json')=={'requestedNormalQuit':True,'exitCode':0};v=load('raw/'+c+'/ready.json')['value'];assert v['prefs']['remote.prefs.recommended']['value'] is False;assert v['prefs']['termsofuse.acceptedVersion']['value']==0;assert v['prefs']['termsofuse.acceptedDate']['value']=='0';assert not v['prefs']['termsofuse.acceptedVersion']['user'];assert not v['prefs']['termsofuse.acceptedDate']['user'];assert v['protections']['sandbox']==6 and not v['protections']['sandboxUser'] and not v['protections']['signatureUser']
v=load('raw/firefox-policy-v2/ready.json')['value'];assert not v['native']['activityStreamPresent'];assert v['prefs']['distribution.id']['defaultValue']=='mozilla-official';assert v['prefs']['termsofuse.bypassNotification']['value'] is False
products=load('product-verification.json');assert all(x['allMatch'] for x in products if x['product']!='floorp-reported');assert not next(x for x in products if x['product']=='floorp-reported')['allMatch']
for row in load('export-manifest.json')['files']:
 p=r/row['path'];assert p.stat().st_size==row['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
print('7 native framebuffer four-quadrant paints, 4 qualified normal exits, unchanged acceptance prefs, Firefox-null boundary, product/reference and export hashes verified')
