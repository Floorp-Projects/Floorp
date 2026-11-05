from pathlib import Path
from PIL import Image,ImageChops
from collections import Counter
import json,sys
r=Path(__file__).parent;port=sys.argv[1] if len(sys.argv)>1 else '29653';out={}
points=[(300,15),(500,15),(800,15),(1000,15),(1100,15),(900,50),(1100,50),(1100,400),(400,700)]
regions={'tabbar_frame':(300,5,1000,30),'overlap_above_right_panel':(940,5,1050,30),'toolbar':(400,40,1000,68),'panel_body':(960,300,1250,700),'page_backdrop':(300,400,800,900)}
for theme in ['default','color','image']:
 ims=[Image.open(r/'screenshots'/f'{port}-{theme}-{b}-simultaneous.png').convert('RGB') for b in ['red','cyan']]
 result={'size':ims[0].size,'samples':[{'point':p,'red':ims[0].getpixel(p),'cyan':ims[1].getpixel(p)} for p in points],'regions':{}}
 for n,box in regions.items():
  a,b=[im.crop(box) for im in ims];diff=ImageChops.difference(a,b);values=list(diff.getdata());result['regions'][n]={'box':box,'pixels':len(values),'changedPixels':sum(v!=(0,0,0) for v in values),'maxChannelDelta':max(max(v) for v in values),'redDominantColors':Counter(a.getdata()).most_common(5)}
 out[theme]=result
(r/f'{port}-pixels.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2))
