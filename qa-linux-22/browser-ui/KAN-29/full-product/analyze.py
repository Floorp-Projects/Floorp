"""Summarize preserved full-product observations; never equate collection with pass."""
import sys,json,re
from pathlib import Path
root=Path(__file__).parent
runs=sys.argv[1:]
result={'source':'actual unmodified package UI measurement JSON','runs':{}}
for run in runs:
 folder=root/'runs'/run;entry={'directions':{}}
 for f in sorted(folder.glob('measure-*.json')):
  data=json.load(open(f));states=data.get('observations',[]);rounds=[q for s in states for q in s.get('rounds',[])];offsets=[];large=0
  for q in rounds:
   samples=q.get('samples',[])
   if not samples:continue
   snap=samples[-1];browser=next((e['rect'] for e in snap['elements'] if e['id']=='browser'),{})
   vals=[]
   for k in ['startWidth','endWidth']:
    raw=snap.get(k,'');m=re.fullmatch(r'(-?[0-9.]+)px',raw)
    if m:vals.append(float(m.group(1)))
   offsets.extend(vals)
   if vals and browser.get('width') and max(vals)>=browser['width']-1:large+=1
  entry['directions'][data.get('requestedDirection',f.stem)]={'statesCollected':len(states),'statesPass':sum(s.get('productPass') is True for s in states),'roundsCollected':len(rounds),'viewportSizedOffsets':large,'uniqueOffsets':sorted(set(offsets)),'lateMotionOver1px':sum(q.get('lateHorizontalMotion',0)>1 for q in rounds),'panel400Rounds':sum(q.get('panel400') is True for q in rounds),'tabboxWithinViewportRounds':sum(q.get('tabboxWithinViewport') is True for q in rounds),'product':data.get('product'),'graphics':data.get('graphics'),'locale':data.get('initialDirection'),'states':[{'nativePositionStart':s.get('nativePositionStart'),'panelPositionAtEndPref':s.get('panelPositionAtEndPref'),'requestedPin':s.get('requestedNativePin'),'actualPin':s.get('actualNativePin'),'productPass':s.get('productPass'),'error':s.get('error'),'sideEffectsPass':s.get('sideEffects',{}).get('pass'),'panelWidthAfterSideEffects':s.get('panelWidthAfterSideEffects')} for s in states]}
 for n in ['runner-result.json','outer-result.json']:
  p=folder/n
  if p.exists():entry[n]=json.load(open(p))
 result['runs'][run]=entry
(root/'comparison.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({r:{d:{k:v for k,v in x.items() if k not in ['states','product','graphics','locale']} for d,x in data['directions'].items()} for r,data in result['runs'].items()},indent=2))
