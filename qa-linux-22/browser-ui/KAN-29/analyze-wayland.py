from pathlib import Path
import json
from PIL import Image
R=Path(__file__).parents[1];out={'key':'KAN-29','baselineSource':'984509d2053be7bae9027133bda7259cd706b208','environment':'Debian13.6, nested KWin/Wayland, Software WebRender','fixture':'UI opens panel and pins/unpins native launcher. Side prefs/setPosition and widget-placement API are fixtures. RTL uses root dir with RTL_UI=false, not a localized RTL session.','states':[]}
for port in [29603,29604]:
 d=json.load(open(R/f'KAN-29/wayland-panel-{port}.json'))
 for o in d['observations']:
  states=[]
  for q in o['rounds']:
   if q.get('unreachableAnchor'):states.append({'unreachableAnchor':True});continue
   last=q['samples'][-1];prev=q['samples'][-2];by={e['id']:e for e in last['elements']};old={e['id']:e for e in prev['elements']};a=by['sidebar-container']['rect'];b=by['panel-sidebar-box']['rect']
   states.append({'nativeX':a['x'],'nativeWidth':a['width'],'nativeExpanded':by['sidebar-container']['expanded'],'lateXDelta':abs(a['x']-old['sidebar-container']['rect']['x']),'panelWidth':b['width'],'hoverEnabled':last['hover'],'offsetStart':last['startWidth'],'offsetEnd':last['endWidth']})
  img=Image.open(R/o['screenshot']).convert('RGB');last=next((q['samples'][-1] for q in reversed(o['rounds']) if 'samples'in q),None);pixels={}
  if last:
   for e in last['elements']:
    if e['id'] not in ['panel-sidebar-box','sidebar-container']:continue
    rr=e['rect'];x=int(rr['x']+rr['width']/2);y=int(rr['y']+min(650,rr['height']/2))
    if 0<=x<img.width and 0<=y<img.height:pixels[e['id']]={'point':[x,y],'rgb':img.getpixel((x,y))}
  out['states'].append({'port':port,'direction':d['requestedDirection'],'nativeStartPref':o['nativePositionStart'],'panelEndPref':o['panelPositionAtEndPref'],'nativePinned':o['requestedNativePin'],'attempts':states,'screenshot':o['screenshot'],'paintSamples':pixels})
(R/'KAN-29/wayland-summary.json').write_text(json.dumps(out,indent=2))
for direction in ['ltr','rtl']:
 rows=[a for s in out['states']if s['direction']==direction for a in s['attempts']]
 print(direction,'attempts',len(rows),'unreachable',sum(a.get('unreachableAnchor',False)for a in rows),'lateChangesAbove1px',sum(a.get('lateXDelta',0)>1 for a in rows),'panels400px',sum(a.get('panelWidth')==400 for a in rows))
