from marionette_driver.marionette import Marionette
from PIL import Image,ImageDraw
from pathlib import Path
import base64,json
R=Path(__file__).parent;im=Image.new('RGB',(64,64));d=ImageDraw.Draw(im)
for box,col in [((0,0,31,31),(255,0,0)),((32,0,63,31),(0,255,0)),((0,32,31,63),(0,0,255)),((32,32,63,63),(255,255,0))]:d.rectangle(box,fill=col)
im.save(R/'fixture-favicon.png');uri='data:image/png;base64,'+base64.b64encode((R/'fixture-favicon.png').read_bytes()).decode()
m=Marionette('localhost',port=29837);m.start_session();m.set_context('chrome');print(m.execute_async_script('''let done=arguments[arguments.length-1];let {NewTabUtils:N}=ChromeUtils.importESModule("resource://gre/modules/NewTabUtils.sys.mjs");let {AboutNewTab:A}=ChromeUtils.importESModule("resource:///modules/AboutNewTab.sys.mjs");N.pinnedLinks.pin({url:"http://127.0.0.1:29871/qa-local",title:"QA local favicon",favicon:arguments[0],faviconSize:64},0);A.activityStream.store.feeds.get("feeds.system.topsites").pinnedCache.expire();A.activityStream.store.feeds.get("feeds.system.topsites").refresh({broadcast:true}).then(()=>done(A.getTopSites().map(x=>({url:x.url,title:x.title,icon:x.favicon?.slice(0,30),size:x.faviconSize}))),e=>done({error:String(e)}));''',script_args=[uri]));m.delete_session()
