from pathlib import Path
import sys,base64,json
sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages')
from PIL import Image,ImageDraw
from control import send
r=Path(__file__).parent;im=Image.new('RGB',(64,64));d=ImageDraw.Draw(im)
for box,col in [((0,0,31,31),(255,0,0)),((32,0,63,31),(0,255,0)),((0,32,31,63),(0,0,255)),((32,32,63,63),(255,255,0))]:d.rectangle(box,fill=col)
im.save(r/'fixture-favicon.png');uri='data:image/png;base64,'+base64.b64encode((r/'fixture-favicon.png').read_bytes()).decode()
script='''const done=arguments[arguments.length-1];const {NewTabUtils:N}=ChromeUtils.importESModule('resource://gre/modules/NewTabUtils.sys.mjs');const {AboutNewTab:A}=ChromeUtils.importESModule('resource:///modules/AboutNewTab.sys.mjs');if(!A.activityStream?.store?.feeds?.get('feeds.system.topsites'))throw Error('native feed not naturally initialized');N.pinnedLinks.pin({url:'http://127.0.0.1:19192/native-local',title:'KAN37 native fixture',favicon:ICON,faviconSize:64},0);const feed=A.activityStream.store.feeds.get('feeds.system.topsites');feed.pinnedCache.expire();feed.refresh({broadcast:true}).then(()=>done({pinned:true,topSiteCount:A.getTopSites().length}),e=>done({error:String(e)}));'''.replace('ICON',json.dumps(uri))
print(send(sys.argv[1],'native-pin-fixture',script=script,**{'async':True}))
