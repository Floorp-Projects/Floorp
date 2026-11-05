from pathlib import Path
import json
from PIL import Image
R=Path(__file__).parents[1]
out={'baselineSource':'984509d2053be7bae9027133bda7259cd706b208','candidatePrHead':'f43b22d34c0728e0ed800b8016d81b3212190954','environment':'Debian13.6 / KWin6.3.6 nested Wayland / Software WebRender','input':'W3C synthetic UI mouse; no physical input','runs':[]}
for port,variant in [(29665,'original'),(29666,'PR2877 diagnostic copy')]:
 d=json.loads((R/f'KAN-29/wayland-panel-{port}.json').read_text());setup=json.loads((R/f'KAN-29/locale/setup-{port}.json').read_text())
 assert d['initialDirection']['rtlUI'] is True and d['initialDirection']['appLocaleRTL'] is True
 assert not any('fallback' in s.lower() and 'if needed' not in s.lower() for s in d['fixtures'])
 row={'port':port,'variant':variant,'graphics':d['graphics'],'initialDirection':d['initialDirection'],'setup':setup,'error':d.get('error'),'states':[]}
 for o in d['observations']:
  qrow={'nativeStartPref':o['nativePositionStart'],'panelEndPref':o['panelPositionAtEndPref'],'nativePinned':o['requestedNativePin'],'screenshot':o['screenshot'],'rounds':[]}
  for q in o['rounds']:
   if q.get('unreachableAnchor'):qrow['rounds'].append({'unreachableAnchor':True});continue
   samples=q['samples'];last=samples[-1];prev=samples[-2];by={x['id']:x for x in last['elements']};old={x['id']:x for x in prev['elements']}
   a=by['sidebar-container']['rect'];b=by['panel-sidebar-box']['rect']
   qrow['rounds'].append({'nativeX':a['x'],'nativeWidth':a['width'],'nativeExpanded':by['sidebar-container']['expanded'],'lateXDelta':abs(a['x']-old['sidebar-container']['rect']['x']),'panelX':b['x'],'panelWidth':b['width'],'offsetStart':last['startWidth'],'offsetEnd':last['endWidth'],'hoverEnabled':last['hover']})
  image=Image.open(R/o['screenshot']).convert('RGB');pixels={}
  last=next((q['samples'][-1] for q in reversed(o['rounds'])if 'samples'in q),None)
  if last:
   for e in last['elements']:
    if e['id']not in ['sidebar-container','panel-sidebar-box']:continue
    rect=e['rect'];x=int(rect['x']+rect['width']/2);y=int(rect['y']+min(180,rect['height']/2))
    if 0<=x<image.width and 0<=y<image.height:
     crop=image.crop((max(0,x-4),max(0,y-4),min(image.width,x+5),min(image.height,y+5)))
     pixels[e['id']]={'point':[x,y],'rgb':image.getpixel((x,y)),'cropUniqueColors':len(set(crop.getdata()))}
  qrow['paintSamples']=pixels;row['states'].append(qrow)
 rounds=[x for s in row['states']for x in s['rounds']]
 row['counts']={'states':len(row['states']),'attempts':len(rounds),'unreachable':sum(x.get('unreachableAnchor',False)for x in rounds),'lateChangesAbove1px':sum(x.get('lateXDelta',0)>1 for x in rounds),'panel400px':sum(x.get('panelWidth')==400 for x in rounds),'viewportSizedOffsets':sum(any(float(str(x.get(k,'0')).replace('px','')or 0)>=1300 for k in ['offsetStart','offsetEnd'])for x in rounds)}
 out['runs'].append(row)
(R/'KAN-29/localized-comparison.json').write_text(json.dumps(out,indent=2))
for row in out['runs']:print(row['variant'],row['initialDirection'],row['counts'])
