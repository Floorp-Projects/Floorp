from marionette_driver.marionette import Marionette
from pathlib import Path
from PIL import Image
import json,time,base64,sys,hashlib,io
R=Path(__file__).parent;m=Marionette('localhost',port=29837);m.start_session();m.set_context('chrome');env=m.execute_script('let c=JSON.parse(Services.prefs.getStringPref("floorp.design.configs"));return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,design:c.globalConfigs.userInterface,disableFloorpStart:c.uiCustomization.disableFloorpStart,signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),proxyType:Services.prefs.getIntPref("network.proxy.type"),selfLoading:Services.prefs.getBoolPref("browser.newtabpage.activity-stream.selfLoading.enabled"),selfLoadingUserSet:Services.prefs.prefHasUserValue("browser.newtabpage.activity-stream.selfLoading.enabled")}')
if 'clear' in sys.argv[1]:m.execute_script('Services.cache2.clear();')
m.set_context('content')
if m.get_url()!='about:newtab':m.navigate('about:newtab')
if 'reload' in sys.argv[1] or 'clear' in sys.argv[1]:m.refresh()
time.sleep(3)
state=m.execute_script('return {uri:location.href,title:document.title,visible:document.visibilityState,images:[...document.querySelectorAll("a img")].map(i=>{let r=i.getBoundingClientRect(),s=getComputedStyle(i);return {title:i.alt,src:i.src,naturalWidth:i.naturalWidth,naturalHeight:i.naturalHeight,complete:i.complete,rect:{x:r.x,y:r.y,width:r.width,height:r.height},css:{display:s.display,visibility:s.visibility,opacity:s.opacity,filter:s.filter}}})}')
png=base64.b64decode(m.screenshot(format='base64'));(R/(sys.argv[1]+'.png')).write_bytes(png);im=Image.open(io.BytesIO(png)).convert('RGB')
for i in state['images']:
 r=i['rect'];i['paintSamples']=[list(im.getpixel((int(r['x']+r['width']*x),int(r['y']+r['height']*y)))) for x,y in [(.25,.25),(.75,.25),(.25,.75),(.75,.75)]]
 if i['src'].startswith('data:'):i['srcSha256']=hashlib.sha256(i['src'].encode()).hexdigest();i['src']='data:image/png;base64,[local fixture]'
state['environment']=env;(R/(sys.argv[1]+'.json')).write_text(json.dumps(state,indent=2));print(json.dumps({'condition':sys.argv[1],'environment':env,'images':[(i['title'],i['naturalWidth'],i['paintSamples']) for i in state['images']]}));m.delete_session()
