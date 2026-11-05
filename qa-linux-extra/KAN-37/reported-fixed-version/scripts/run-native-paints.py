from pathlib import Path
import json,time,sys
from control import send as raw_send
def send(case,id,**kwargs):return raw_send(case,"native-"+id,**kwargs)
sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages')
from PIL import Image
r=Path(__file__).parent;c=sys.argv[1];out=r/'trials'/c;rows=[];script=(r/'paint-probe.js').read_text()
def paint(id):
 v=send(c,id,target='about:newtab',context='content',script=script,screenshot=id+'.png');assert 'value' in v,v;x=v['value'];im=Image.open(out/(id+'.png')).convert('RGB');rect=x['iconRect'];scr=x['screen'];coords=[(int((scr['x']+rect['x']+rect['width']*u)*scr['dpr']),int((scr['y']+rect['y']+rect['height']*w)*scr['dpr'])) for u,w in [(0.25,0.25),(.75,.25),(.25,.75),(.75,.75)]];pixels=[list(im.getpixel(q)) for q in coords];expected=[[255,0,0],[0,255,0],[0,0,255],[255,255,0]];row={'id':id,'case':c,'observation':x,'screenSampleCoordinates':coords,'pixels':pixels,'expectedPixels':expected,'allExact':pixels==expected};rows.append(row);(out/'paint-results.json').write_text(json.dumps(rows,indent=2)+'\n');print(id,pixels,row['allExact'],flush=True);assert row['allExact'];return row
print(send(c,'dismiss-standard-default-browser-prompt',nativeKeys=['Escape']),flush=True)
if '--restart-only' in sys.argv:paint('native-restart-verified')
else:
 for design in ['proton','fluerial','lepton']:
  q=send(c,design+'-setting',script='const p=Services.prefs;const d=JSON.parse(p.getStringPref("floorp.design.configs"));d.globalConfigs.userInterface='+json.dumps(design)+';p.setStringPref("floorp.design.configs",JSON.stringify(d));return {design:JSON.parse(p.getStringPref("floorp.design.configs")).globalConfigs.userInterface,disableFloorpStart:d.uiCustomization.disableFloorpStart};');assert q['value']['design']==design
  for condition in ['reload','cache']:
   if condition=='cache':print(send(c,design+'-native-cache-clear',script='Services.cache2.clear();return true;'),flush=True)
   print(send(c,design+'-'+condition+'-navigate',script='gBrowser.selectedBrowser.reload();return true;'),flush=True);time.sleep(2);paint(design+'-'+condition)
if c.startswith('reported-fixed'):
 import subprocess
 subprocess.run([sys.executable,str(r/'check-fixed-version.py'),c,'final'],check=True)
print(send(c,'state-final'),flush=True);print(send(c,'quit',stop=True),flush=True)
