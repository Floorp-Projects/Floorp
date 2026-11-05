import sys,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
def js(s,args=[]):return m.execute_script(s,script_args=args)
def actions(seq):m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'qa-rd','parameters':{'pointerType':'mouse'},'actions':seq}]})
def move(x,y,duration):return {'type':'pointerMove','origin':'viewport','x':x,'y':y,'duration':duration}
r={'product':js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,name:Services.appinfo.name,os:Services.appinfo.OS}'),'observations':[]}
try:
 js('Services.prefs.setBoolPref("floorp.mousegesture.enabled",true);let c=JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config","{}"));c.actions=[{pattern:["up"],action:"gecko-open-new-tab"},{pattern:["right","down"],action:"gecko-open-new-tab"}];Services.prefs.setStringPref("floorp.mousegesture.config",JSON.stringify(c))');time.sleep(1)
 r['config']=js('return {enabled:Services.prefs.getBoolPref("floorp.mousegesture.enabled"),config:JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config"))}')
 cases=[('recorded-three-points',[(450,450),(650,450),(650,650)],0),('recorded-five-points',[(450,450),(550,450),(650,450),(650,483),(650,650)],0),('dense',[(x,450) for x in range(450,651,10)]+[(650,y) for y in range(460,651,10)],40)]
 for repeat in range(3):
  for name,pts,duration in cases:
   js('gBrowser.selectedTab=gBrowser.addTab("data:text/html,<h1>QA right-down input</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})');time.sleep(.4)
   before=js('return gBrowser.tabs.length')
   js('window.qaRDEvents=[];window.qaRDHandler=e=>{if(qaRDEvents.length<400)qaRDEvents.push({type:e.type,x:e.clientX,y:e.clientY,screenX:e.screenX,screenY:e.screenY,buttons:e.buttons,trusted:e.isTrusted,time:e.timeStamp})};for(let n of ["mousedown","mousemove","mouseup","contextmenu"])window.addEventListener(n,qaRDHandler,{capture:true})')
   seq=[move(*pts[0],100),{'type':'pointerDown','button':2}]+[move(x,y,duration) for x,y in pts[1:]]
   actions(seq);time.sleep(.15)
   label=js('return document.querySelector("#mouse-gesture-display-container")?.textContent')
   if repeat==0:(R/'KAN-98'/f'sparse-product-{port}-{name}-held.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
   actions([{'type':'pointerUp','button':2}]);time.sleep(.4)
   r['observations'].append({'case':name,'repeat':repeat,'points':pts,'durationPerMoveMs':duration,'beforeTabs':before,'afterTabs':js('return gBrowser.tabs.length'),'label':label,'events':js('return qaRDEvents')})
   js('for(let n of ["mousedown","mousemove","mouseup","contextmenu"])window.removeEventListener(n,qaRDHandler,{capture:true})')
except Exception as e:r.update(error=repr(e),traceback=traceback.format_exc())
finally:
 (R/f'KAN-98/sparse-product-{port}.json').write_text(json.dumps(r,ensure_ascii=False,indent=2));print(json.dumps({'error':r.get('error'),'config':r.get('config'),'results':[{k:o[k] for k in ['case','repeat','beforeTabs','afterTabs','label']} for o in r['observations']]},ensure_ascii=False));m.delete_session()
