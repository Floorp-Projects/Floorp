from pathlib import Path
import json,hashlib,sys
try:from PIL import Image
except ImportError:
 sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages');from PIL import Image
r=Path(__file__).resolve().parents[1]
def load(p):return json.loads((r/p).read_text())
s=load('summary.json');base=load('files-before.json');assert len(base)==46 and load('files-after-all.json')['files']==base
expected=[[255,0,0],[0,255,0],[0,0,255],[255,255,0]]
assert len(s['qualifiedSessions'])==3 and len(s['paintResults'])==7
for session in s['qualifiedSessions']:
 c=session['case'];ready=load('raw/'+c+'/ready.json')['value'];assert ready['version']=='156.0.1' and ready['buildID']=='20260916130640';assert ready['prefs']['app.update.disabledForTesting']['value'] is True;assert ready['prefs']['remote.prefs.recommended']['value'] is False;assert ready['protections']=={'sandbox':6,'sandboxUser':False,'signatureRequired':False,'signatureUser':False};assert load('raw/'+c+'/shutdown.json')=={'requestedNormalQuit':True,'exitCode':0}
 checkpoints=list((r/'raw'/c).glob('fixed-version-contract*.json'));assert checkpoints
 for p in checkpoints:
  x=json.loads(p.read_text());v=x['live'];assert v['version']=='156.0.1' and v['buildID']=='20260916130640' and v['pref'] and v['user'] and v['disabledForTesting'] and v['disabled'];assert x['files']==base and x['all46EntriesMatchArchive']
for row in s['paintResults']:
 assert row['allExact'] and row['observation']['mainClass']=='content-main';im=Image.open(r/row['screenshotFile']).convert('RGB');assert [list(im.getpixel(tuple(q))) for q in row['screenSampleCoordinates']]==row['pixels']==expected
for x in load('export-manifest.json')['files']:
 p=r/x['path'];assert p.stat().st_size==x['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==x['sha256']
print('reported156: 3normal exits; startup/final native gate/version and46entry hashes; 7Nova framebuffer4quadrant paints; export hashes verified')
