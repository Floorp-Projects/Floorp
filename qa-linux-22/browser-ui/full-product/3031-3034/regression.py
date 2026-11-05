import sys,os,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=90,startup_timeout=90);m.start_session();m.set_context('chrome')
def js(s,args=[]):return m.execute_script(s,script_args=args)
def act(a):m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'combination','parameters':{'pointerType':'mouse'},'actions':a}]})
def move(x,y,d=0):return {'type':'pointerMove','origin':'viewport','x':x,'y':y,'duration':d}
r={'relativeTrail':[[0,0],[0,-160],[0,16]],'translatedTrail':[[450,500],[450,340],[450,516]],'minDistance':20,'input':'W3C synthetic pointer, 0ms held moves; not physical input','product':js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,name:Services.appinfo.name,os:Services.appinfo.OS}'),'observations':[]}
try:
 r['productionStartup']=js('return {gFloorp:!!window.gFloorp,scripts:[...document.querySelectorAll("script[src]")].map(e=>e.src),signaturesRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),contentSandboxLevel:Services.prefs.getIntPref("security.sandbox.content.level"),sandboxUserSet:Services.prefs.prefHasUserValue("security.sandbox.content.level"),defaultSignatureRequired:Services.prefs.getDefaultBranch("").getBoolPref("xpinstall.signatures.required")}')
 js('Services.prefs.setBoolPref("floorp.mousegesture.enabled",true);let c=JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config","{}"));c.contextMenu.minDistance=20;c.actions=[{pattern:["up","down"],action:"gecko-open-new-tab"},{pattern:["down","up"],action:"gecko-close-tab"}];Services.prefs.setStringPref("floorp.mousegesture.config",JSON.stringify(c));window.qaCombo={events:[],tabOpen:0,tabClose:0,popupShown:0};for(let n of ["mousedown","mousemove","mouseup","contextmenu"])window.addEventListener(n,e=>{if((e.buttons&2)||n!=="mousemove"){let o={type:n,x:e.clientX,y:e.clientY,buttons:e.buttons,trusted:e.isTrusted};qaCombo.events.push(o);setTimeout(()=>o.defaultPrevented=e.defaultPrevented,0)}},true);gBrowser.tabContainer.addEventListener("TabOpen",()=>qaCombo.tabOpen++);gBrowser.tabContainer.addEventListener("TabClose",()=>qaCombo.tabClose++);window.addEventListener("popupshown",e=>{if(e.target.id==="contentAreaContextMenu")qaCombo.popupShown++},true)');time.sleep(1)
 r['config']=js('return {enabledPref:Services.prefs.getBoolPref("floorp.mousegesture.enabled"),gesture:JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config")),afterMouseup:Services.prefs.getBoolPref("ui.context_menus.after_mouseup",false),timingLocked:Services.prefs.prefIsLocked("ui.context_menus.after_mouseup")}')
 js('window.qaOriginal=gBrowser.addTab("data:text/html,<title>QA combined trail</title><h1>QA combined gesture</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});gBrowser.selectedTab=qaOriginal');time.sleep(.7)
 before=js('qaCombo.events=[];qaCombo.tabOpen=qaCombo.tabClose=qaCombo.popupShown=0;return gBrowser.tabs.length')
 import os,subprocess
 native=os.environ.get('QA_COMBO_XTEST')=='1'
 if native:
  r['input']='OS XTest synthetic mouse; same 3-point trail; not physical input'
  pos=js('return {x:mozInnerScreenX,y:mozInnerScreenY}')
  env=os.environ.copy();env['DISPLAY']=os.environ['DISPLAY'];env['LD_LIBRARY_PATH']='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu'
  def xd(*args):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],env=env,check=True)
  xd('mousemove','--sync',int(pos['x']+450),int(pos['y']+500));xd('mousedown',3);time.sleep(.1)
  xd('mousemove','--sync',int(pos['x']+450),int(pos['y']+340));time.sleep(.1)
  xd('mousemove','--sync',int(pos['x']+450),int(pos['y']+516));time.sleep(.15)
 else:act([move(450,500,100),{'type':'pointerDown','button':2},move(450,340),move(450,516)]);time.sleep(.15)
 held=js('return [...document.querySelectorAll("[id*=gesture],[class*=gesture]")].map(e=>({id:e.id,text:e.textContent?.slice(0,120)}))')
 xd('mouseup',3) if native else act([{'type':'pointerUp','button':2}]);time.sleep(1)
 after=js('return {tabs:gBrowser.tabs.length,originalConnected:qaOriginal.isConnected,tabOpen:qaCombo.tabOpen,tabClose:qaCombo.tabClose,popupShown:qaCombo.popupShown,menuState:document.getElementById("contentAreaContextMenu").state,events:qaCombo.events}')
 r['observations'].append({'beforeTabs':before,'heldLabel':held,'after':after,'correctOnce':after['tabs']==before+1 and after['tabOpen']==1 and after['tabClose']==0 and after['originalConnected'],'noContextMenu':after['popupShown']==0 and after['menuState']=='closed'})
 (R/f'returning-{port}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
except Exception as e:r['error']=repr(e);r['traceback']=traceback.format_exc()
finally:
 (R/f'returning-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
 if os.environ.get('QA_COMBO_EXPECT_PASS')=='1' and (r.get('error') or not all(o['correctOnce'] and o['noContextMenu'] for o in r['observations'])):sys.exit(1)
