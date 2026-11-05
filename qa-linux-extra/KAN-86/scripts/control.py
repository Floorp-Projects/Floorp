import json,time
from pathlib import Path
R=Path('/workspace/qa-linux-22/C-extra/KAN-86/trials')
def send(case,id,**kwargs):
 out=R/case;result=out/('result-'+id+'.json');assert not result.exists();payload={'id':id,**kwargs};(out/('request-'+id+'.json')).write_text(json.dumps(payload,indent=2)+'\n');tmp=out/'command.pending';tmp.write_text(json.dumps(payload));tmp.replace(out/'command.json')
 if kwargs.get('stop'):return {'normalQuitRequested':True}
 for _ in range(200):
  if result.exists():
   try:return json.loads(result.read_text())
   except json.JSONDecodeError:pass
  time.sleep(.15)
 raise TimeoutError(id)
