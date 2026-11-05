from pathlib import Path
from PIL import Image,ImageDraw
import json,zipfile
r=Path(__file__).parent/'theme';r.mkdir(exist_ok=True)
im=Image.new('RGB',(1800,160));d=ImageDraw.Draw(im)
colors=[(174,200,240),(220,175,230),(150,220,200),(240,200,150)]
for x in range(0,1800,90):d.rectangle((x,0,x+89,159),fill=colors[(x//90)%4])
im.save(r/'frame.png')
for images in [False,True]:
 name='image' if images else 'color'
 manifest={'manifest_version':2,'name':'QA Zen '+name+' Theme','version':'1.0','browser_specific_settings':{'gecko':{'id':'qa-zen-'+name+'@floorp.test'}},'theme':{'colors':{'frame':'rgba(174,200,240,0.35)','tab_background_text':'#142134','toolbar':'#edf1f7','toolbar_text':'#142134','tab_selected':'#edf1f7','tab_text':'#142134','sidebar':'#edf1f7','sidebar_text':'#142134'}}}
 if images:manifest['theme']['images']={'theme_frame':'frame.png'}
 (r/(name+'-manifest.json')).write_text(json.dumps(manifest,indent=2))
 with zipfile.ZipFile(r/(name+'.xpi'),'w') as z:
  z.writestr('manifest.json',json.dumps(manifest))
  if images:z.write(r/'frame.png','frame.png')
