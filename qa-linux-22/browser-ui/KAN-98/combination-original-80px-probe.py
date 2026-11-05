import sys,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
def js(s,args=[]):return m.execute_script(s,script_args=args)
def act(a):m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'combination','parameters':{'pointerType':'mouse'},'actions':a}]})
def move(x,y,d=0):return {'type':'pointerMove','origin':'viewport','x':x,'y':y,'duration':d}
r={'relativeTrail':[[0,0],[0,-160],[0,80]],'translatedTrail':[[450,500],[450,340],[450,580]],'input':'W3C synthetic pointer, 0ms held moves; not physical input','product':js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,name:Services.appinfo.name}'),'observations':[]}
try:
 js('Services.prefs.setBoolPref("floorp.mousegesture.enabled",true);let c=JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config","{}"));c.actions=[{pattern:["up","down"],action:"gecko-open-new-tab"},{pattern:["down","up"],action:"gecko-close-tab"}];Services.prefs.setStringPref("floorp.mousegesture.config",JSON.stringify(c));window.qaCombo={events:[],tabOpen:0,tabClose:0,popupShown:0};for(let n of ["mousedown","mousemove","mouseup","contextmenu"])window.addEventListener(n,e=>{if((e.buttons&2)||n!=="mousemove"){let o={type:n,x:e.clientX,y:e.clientY,buttons:e.buttons,trusted:e.isTrusted};qaCombo.events.push(o);setTimeout(()=>o.defaultPrevented=e.defaultPrevented,0)}},true);gBrowser.tabContainer.addEventListener("TabOpen",()=>qaCombo.tabOpen++);gBrowser.tabContainer.addEventListener("TabClose",()=>qaCombo.tabClose++);window.addEventListener("popupshown",e=>{if(e.target.id==="contentAreaContextMenu")qaCombo.popupShown++},true)');time.sleep(1)
 r['config']=js('return JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config"))')
 js('window.qaOriginal=gBrowser.addTab("data:text/html,<title>QA combined trail</title><h1>QA combined gesture</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});gBrowser.selectedTab=qaOriginal');time.sleep(.7)
 before=js('qaCombo.events=[];qaCombo.tabOpen=qaCombo.tabClose=qaCombo.popupShown=0;return gBrowser.tabs.length')
 act([move(450,500,100),{'type':'pointerDown','button':2},move(450,340),move(450,580)]);time.sleep(.15)
 held=js('return [...document.querySelectorAll("[id*=gesture],[class*=gesture]")].map(e=>({id:e.id,text:e.textContent?.slice(0,120)}))')
 act([{'type':'pointerUp','button':2}]);time.sleep(1)
 after=js('return {tabs:gBrowser.tabs.length,originalConnected:qaOriginal.isConnected,tabOpen:qaCombo.tabOpen,tabClose:qaCombo.tabClose,popupShown:qaCombo.popupShown,menuState:document.getElementById("contentAreaContextMenu").state,events:qaCombo.events}')
 r['observations'].append({'beforeTabs':before,'heldLabel':held,'after':after,'correctOnce':after['tabs']==before+1 and after['tabOpen']==1 and after['tabClose']==0 and after['originalConnected'],'noContextMenu':after['popupShown']==0 and after['menuState']=='closed'})
 (R/f'KAN-98/combination-{port}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
except Exception as e:r['error']=repr(e);r['traceback']=traceback.format_exc()
finally:
 (R/f'KAN-98/combination-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
