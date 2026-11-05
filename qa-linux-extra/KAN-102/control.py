import json,time
from pathlib import Path
root=Path('/workspace/qa-linux-22/C-extra/KAN-102')
def send(case,id,script='return true;',**opts):
 p=root/case;result=p/('result-'+id+'.json');assert not result.exists(),str(result)
 data=json.dumps({'id':id,'script':script,**opts});(p/('request-'+id+'.json')).write_text(data);pending=p/'command.pending';pending.write_text(data);pending.replace(p/'command.json')
 for _ in range(200):
  if result.exists():
   try:
    data=json.loads(result.read_text())
    if isinstance(data,dict) and data.get('error'):raise RuntimeError(data['error'])
    return data
   except json.JSONDecodeError:pass
  time.sleep(.2)
 raise TimeoutError(id)
