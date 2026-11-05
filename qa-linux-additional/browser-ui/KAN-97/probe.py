from marionette_driver.marionette import Marionette
from marionette_driver.by import By
from pathlib import Path
import os,sys,time,json,base64,subprocess,traceback
R=Path(__file__).parent;product=sys.argv[1];port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=60);m.start_session();out={'productKind':product,'input':'OS XTest synthetic mouse/keyboard; W3C settings toggle; not physical hardware','observations':[]}
def chrome(s):m.set_context('chrome');return m.execute_script(s)
def content(s):m.set_context('content');m.switch_to_frame(None);return m.execute_script(s)
def click(x,y):
 m.set_context('content');m.switch_to_frame(None);m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'ui','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','duration':0,'origin':'viewport','x':int(x),'y':int(y)},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]})
def xd(*args):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],env=os.environ.copy(),check=True)
def framejs(s):
 m.set_context('content');m.switch_to_frame(None);m.switch_to_frame(m.find_element(By.ID,'frame'));v=m.execute_script(s);m.switch_to_frame(None);return v
def measure():
 a=content('return {root:{top:scrollY,max:document.scrollingElement.scrollHeight-innerHeight},overflow:{top:document.querySelector("#box").scrollTop,max:document.querySelector("#box").scrollHeight-document.querySelector("#box").clientHeight},active:document.activeElement.id||document.activeElement.localName,visible:document.visibilityState}')
 a['frame']=framejs('return {top:scrollY,max:document.scrollingElement.scrollHeight-innerHeight,active:document.activeElement.localName}')
 a['focus']=chrome('let s=Services.focus;return {contentContext:s.focusedContentBrowsingContext?.id,context:s.focusedBrowsingContext?.id,top:gBrowser.selectedBrowser.browsingContext.id,frames:gBrowser.selectedBrowser.browsingContext.children.map(b=>({id:b.id,uri:b.currentURI?.spec}))}')
 return a
try:
 if product=='patched':
  m.set_context('chrome')
  for stage in ['before','after']:
   bundle=(R/'unit'/(stage+'.js')).read_text()
   result=m.execute_async_script(bundle+'\nconst done=arguments[arguments.length-1];qaScrollTests.runAllTests().then(()=>done({passed:true,progress:globalThis.__NORA_TEST_PROGRESS__}),e=>done({passed:false,error:String(e),progress:globalThis.__NORA_TEST_PROGRESS__}));')
   (R/'unit'/(stage+'-browser.json')).write_text(json.dumps(result,indent=2));out.setdefault('unit',{})[stage]=result
 out['metadata']=chrome('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode"),documentFocused:document.hasFocus()}')
 if product!='firefox':
  m.set_context('content');m.navigate('about:hub#/features/gesture');time.sleep(1)
  setting=content('let e=document.querySelector("[data-setting=mouse-gesture-enabled]");let r=e.getBoundingClientRect();return {html:e.outerHTML,x:r.x+r.width/2,y:r.y+r.height/2,checked:e.checked,aria:e.getAttribute("aria-checked")}')
  out['settingsBefore']=setting
  if setting['checked']!=True and setting['aria']!='true':click(setting['x'],setting['y']);time.sleep(1)
  (R/(product+'-settings.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')))
  out['config']=chrome('return {enabled:Services.prefs.getBoolPref("floorp.mousegesture.enabled"),config:JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config"))}')
  assert out['config']['enabled'];assert any(x['action']=='gecko-scroll-to-top' and x['pattern']==['up'] for x in out['config']['config']['actions'])
 chrome('window.qa97={events:[],popups:0};for(let n of ["mousedown","mouseup","contextmenu"])window.addEventListener(n,e=>{let r={type:n,trusted:e.isTrusted,x:e.clientX,y:e.clientY};qa97.events.push(r);setTimeout(()=>r.prevented=e.defaultPrevented,0)},true);window.addEventListener("popupshown",e=>{if(e.target.id==="contentAreaContextMenu")qa97.popups++},true)')
 for target in ['root','overflow','same-frame','cross-frame']:
  m.set_context('content');m.switch_to_frame(None);m.navigate('http://127.0.0.1:29971/'+('cross' if target=='cross-frame' else 'same'));time.sleep(.4)
  for mode in (['gesture','keyboard'] if product!='firefox' else ['keyboard']):
   for direction in ['top','bottom']:
    content('scrollTo({top:1800,behavior:"instant"});document.querySelector("#box").scrollTop=1800');framejs('scrollTo({top:1800,behavior:"instant"})');time.sleep(.1)
    pos=content('let target=arguments[0];let e=target==="root"?document.querySelector("#rootFocus"):document.querySelector(target==="overflow"?"#box":"#frame");let r=e.getBoundingClientRect();return {x:target==="root"?innerWidth-70:r.x+r.width/2,y:target==="root"?300:r.y+r.height/2,sx:mozInnerScreenX,sy:mozInnerScreenY}',) if False else None
    m.set_context('content');m.switch_to_frame(None);pos=m.execute_script('let t=arguments[0],e=document.querySelector(t==="overflow"?"#box":"#frame"),r=e.getBoundingClientRect();return {x:t==="root"?innerWidth-70:r.x+r.width/2,y:t==="root"?300:r.y+r.height/2,sx:mozInnerScreenX,sy:mozInnerScreenY}',script_args=[target])
    xd('mousemove','--sync',int(pos['sx']+pos['x']),int(pos['sy']+pos['y']),'click','1');time.sleep(.1);before=measure();chrome('qa97.events=[];qa97.popups=0')
    held=[]
    if mode=='gesture':
     xd('mousedown','3');time.sleep(.06)
     sign=-1 if direction=='top' else 1
     for j in range(1,9):xd('mousemove','--sync',int(pos['sx']+pos['x']),int(pos['sy']+pos['y']+sign*j*18));time.sleep(.02)
     time.sleep(.1);held=chrome('return [...document.querySelectorAll("[id*=gesture],[class*=gesture]")].filter(e=>e.textContent).map(e=>({id:e.id,text:e.textContent.slice(0,150)}))');xd('mouseup','3')
    else:xd('key','ctrl+Home' if direction=='top' else 'ctrl+End')
    time.sleep(1.1);after=measure();events=chrome('return qa97');expectedKey='frame' if 'frame' in target else target;wanted=0 if direction=='top' else before[expectedKey]['max'];actual=after[expectedKey]['top']
    row={'target':target,'mode':mode,'direction':direction,'before':before,'heldLabel':held,'after':after,'events':events,'expectedTarget':expectedKey,'expectedTop':wanted,'actualTop':actual,'atExpectedEndpoint':abs(actual-wanted)<=1};out['observations'].append(row);print(product,target,mode,direction,row['atExpectedEndpoint'],before[expectedKey]['top'],actual,flush=True)
    if mode=='gesture':(R/(product+'-'+target+'-'+direction+'.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')))
except Exception as e:out['error']=repr(e);out['traceback']=traceback.format_exc();print(out['traceback'],flush=True)
finally:
 (R/(product+'.json')).write_text(json.dumps(out,indent=2));m.delete_session()
 if out.get('error'):sys.exit(1)
