import sys,json
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
traces=[]
for filename in ['gesture-29301.json','gesture-29314.json','KAN-98/rightdown-29321.json']:
 d=json.loads((R/filename).read_text())
 for o in d['observations']:
  if o.get('name',o.get('case')) not in ['rightdown','sparse-fast','sparse-slow','dense']:continue
  start=next(e for e in o['events'] if e['type']=='mousedown')
  pts=[{'x':start['x'],'y':start['y']}]
  for e in o['events']:
   if e['type']!='mousemove' or e['buttons']!=2:continue
   if (e['x']-pts[-1]['x'])**2+(e['y']-pts[-1]['y'])**2>=1.5**2:pts.append({'x':e['x'],'y':e['y']})
  traces.append({'source':filename,'case':o.get('name',o.get('case')),'repeat':o.get('repeat'), 'tabDelta':o['afterTabs']-o['beforeTabs'],'trail':pts})
code=(R/'KAN-98/recognizer-replay.js').read_text()
s=code+''';return arguments[0].map(t=>{const actions=[{pattern:["up"],action:"gecko-open-new-tab"},{pattern:["up","down"],action:"gecko-open-new-tab"},{pattern:["down","up"],action:"gecko-open-new-tab"},{pattern:["right","down"],action:"gecko-open-new-tab"}];const {recognizer,shapeDb}=qaRec.createRecognizer(actions);return {...t,firstDirection:qaRec.calculateMeanDirection(t.trail),sequence:qaRec.extractDirectionSequence(t.trail,5),recognized:qaRec.recognize(recognizer,t.trail,0.73,shapeDb,5)}});'''
r=m.execute_script(s,script_args=[traces]);(R/f'KAN-98/rightdown-replay-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps([{k:v for k,v in x.items() if k!='trail'} for x in r],indent=2));m.delete_session()
