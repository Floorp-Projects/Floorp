import sys,os,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
ROOT=Path(__file__).parent
scenario=sys.argv[1]; port=int(sys.argv[2]) if len(sys.argv)>2 else 29282
m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
def js(s,args=None): return m.execute_script(s,script_args=args or [])
def shot(name): (ROOT/(name+'.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')))
def click(x,y,button=0):
 m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'qa-mouse','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':int(x),'y':int(y),'duration':100},{'type':'pointerDown','button':button},{'type':'pointerUp','button':button}]}]})
def rect(sel): return js('let r=document.querySelector(arguments[0]).getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,w:r.width,h:r.height}',[sel])
r={'scenario':scenario,'product':js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,name:Services.appinfo.name,os:Services.appinfo.OS}'),'observations':[]}
try:
 r['graphics']=m.execute_async_script('const done=arguments[arguments.length-1];const {Troubleshoot}=ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs");Troubleshoot.snapshot().then(s=>done({windowProtocol:s.graphics.windowProtocol,compositing:s.graphics.windowLayerManagerType,keys:Object.keys(s.graphics)})).catch(e=>done({error:String(e)}))');
 if scenario=='split':
  js('window.qaOrigin=window;window.qaSets=[];for(let j=0;j<2;j++){let ts=Array.from({length:2},(_,i)=>gBrowser.addTab("data:text/html,<title>QA split "+j+"-"+i+"</title><h1>QA split "+j+"-"+i+"</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));let gr=gBrowser.addTabGroup(ts,{label:"QA split "+j});gBrowser.addTabSplitView(ts,{insertBefore:ts[0]});qaSets.push({ts,gr});}gBrowser.selectedTab=qaSets[1].ts[0]');time.sleep(3)
  snap='return qaSets.map(s=>s.ts.map(t=>({connected:t.isConnected,url:t.linkedBrowser?.currentURI.spec,title:t.linkedBrowser?.contentTitle,pending:t.hasAttribute("pending"),panel:t.linkedPanel,split:t.getAttribute("floorpSplitViewGroupId")})))'
  r['observations'].append({'phase':'before','tabs':js(snap)});shot('KAN-3/before')
  js('window.qaNew=gBrowser.replaceGroupWithWindow(qaSets[1].gr)');time.sleep(3)
  r['observations'].append({'phase':'new-window','tabs':js('return Array.from(qaNew.gBrowser.tabs).map(t=>({url:t.linkedBrowser.currentURI.spec,title:t.linkedBrowser.contentTitle,pending:t.hasAttribute("pending")}))')});shot('KAN-3/moved')
  js('window.qaMovedGroup=qaNew.gBrowser.tabGroups[0];gBrowser.adoptTabGroup(qaMovedGroup,{tabIndex:gBrowser.tabs.length})');time.sleep(3)
  r['observations'].append({'phase':'returned','tabs':js('return Array.from(gBrowser.tabs).filter(t=>t.linkedBrowser.currentURI.spec.includes("QA split")).map(t=>({url:t.linkedBrowser.currentURI.spec,title:t.linkedBrowser.contentTitle}))')});shot('KAN-3/returned')
 if scenario=='detach':
  js('window.qaTabs=Array.from({length:3},(_,i)=>gBrowser.addTab("data:text/html,<title>QA detach "+i+"</title>QA "+i,{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));window.qaGroup=gBrowser.addTabGroup(qaTabs,{label:"QA detach"});gBrowser.selectedTab=qaTabs[0]');time.sleep(2)
  js('window.qaNew=gBrowser.replaceTabWithWindow(qaTabs[1])');time.sleep(2)
  r['observations'].append(js('return {source:qaTabs.map(t=>({connected:t.isConnected,url:t.linkedBrowser?.currentURI.spec})),destination:Array.from(qaNew.gBrowser.tabs).map(t=>({url:t.linkedBrowser.currentURI.spec,title:t.linkedBrowser.contentTitle}))}'));shot('KAN-5/detach')
  js('gBrowser.adoptTab(qaNew.gBrowser.tabs[0],gBrowser.tabs.length,true)');time.sleep(1)
  r['observations'].append(js('return Array.from(gBrowser.tabs).filter(t=>t.linkedBrowser.currentURI.spec.includes("QA detach")).map(t=>({url:t.linkedBrowser.currentURI.spec,title:t.linkedBrowser.contentTitle}))'))
 if scenario=='visual':
  js('gBrowser.selectedTab=gBrowser.addTab("data:text/html,<body style=background:repeating-linear-gradient(45deg,red,blue%2020px,yellow%2040px)><h1>QA opaque backdrop</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})');time.sleep(2)
  style='return ["#navigator-toolbox","#TabsToolbar","#nav-bar","#PanelUI-menu-button","#urlbar-input"].map(s=>{let e=document.querySelector(s);let c=e&&getComputedStyle(e);return {selector:s,color:c?.color,background:c?.backgroundColor,opacity:c?.opacity,rect:e&&{width:e.getBoundingClientRect().width,height:e.getBoundingClientRect().height}}})'
  for dark in [0,1]:
   js('Services.prefs.setIntPref("ui.systemUsesDarkTheme",arguments[0])',[dark]);time.sleep(1)
   r['observations'].append({'dark':dark,'normal':js(style)});shot('KAN-93/'+r['product']['name']+'-chrome-'+str(dark))
   if not js('return !!window.__floorpZenModeController'):continue
   js('window.__floorpZenModeController.setEnabledFromUser(true)');time.sleep(1)
   # reveal through pointer movement to upper edge
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'reveal','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':500,'y':2,'duration':100}]}]});time.sleep(.5)
   r['observations'].append({'dark':dark,'zen':js(style)});shot('KAN-87/zen-'+str(dark))
   js('window.__floorpZenModeController.setEnabledFromUser(false)')
 if scenario=='gesture':
  js('Services.prefs.setBoolPref("floorp.mousegesture.enabled",true);let c=JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config","{}"));c.actions=[{pattern:["up"],action:"gecko-open-new-tab"},{pattern:["up","down"],action:"gecko-open-new-tab"}];Services.prefs.setStringPref("floorp.mousegesture.config",JSON.stringify(c))');time.sleep(2)
  for points in [[(600,650),(600,450)],[(600,650),(600,450),(600,650)]]:
   js('gBrowser.selectedTab=gBrowser.addTab("data:text/html,<h1>QA gesture input</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})');time.sleep(.5)
   before=js('return gBrowser.tabs.length')
   acts=[{'type':'pointerMove','origin':'viewport','x':points[0][0],'y':points[0][1],'duration':100},{'type':'pointerDown','button':2}]
   for x,y in points[1:]:acts.append({'type':'pointerMove','origin':'viewport','x':x,'y':y,'duration':500})
   acts.append({'type':'pointerUp','button':2})
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'gesture','parameters':{'pointerType':'mouse'},'actions':acts}]});time.sleep(1)
   r['observations'].append({'points':points,'beforeTabs':before,'afterTabs':js('return gBrowser.tabs.length')});shot('KAN-98/gesture-'+str(len(points)))
 if scenario=='controls':
  for style in ['horizontal','multirow']:
   js('let c=JSON.parse(Services.prefs.getStringPref("floorp.design.configs"));c.tabbar.tabbarStyle=arguments[0];Services.prefs.setStringPref("floorp.design.configs",JSON.stringify(c));setToolbarVisibility(document.getElementById("toolbar-menubar"),true)',[style]);time.sleep(1)
   for title in [0,1]:
    js('Services.prefs.setIntPref("browser.tabs.inTitlebar",arguments[0])',[title]);time.sleep(1)
    r['observations'].append({'style':style,'inTitlebar':title,'controls':js('return [...document.querySelectorAll(".titlebar-buttonbox-container")].map(e=>({parent:e.parentElement.id,visible:e.checkVisibility(),rect:{x:e.getBoundingClientRect().x,y:e.getBoundingClientRect().y,w:e.getBoundingClientRect().width,h:e.getBoundingClientRect().height}}))')});shot('KAN-54/controls-'+style+'-'+str(title))
 if scenario=='layout':
  js('let c=JSON.parse(Services.prefs.getStringPref("floorp.design.configs"));c.tabbar.tabbarStyle="vertical";Services.prefs.setStringPref("floorp.design.configs",JSON.stringify(c));Services.prefs.setBoolPref("sidebar.revamp",true);Services.prefs.setBoolPref("sidebar.verticalTabs",true);Services.prefs.setStringPref("sidebar.visibility","expand-on-hover")');time.sleep(2)
  for native in [True,False]:
   for panel in [True,False]:
    js('Services.prefs.setBoolPref("sidebar.position_start",arguments[0]);SidebarController.setPosition();let c=JSON.parse(Services.prefs.getStringPref("floorp.panelSidebar.config"));c.position_start=arguments[1];Services.prefs.setStringPref("floorp.panelSidebar.config",JSON.stringify(c))',[native,panel]);time.sleep(.8)
    dims='return {dir:getComputedStyle(document.documentElement).direction,position:Services.prefs.getBoolPref("sidebar.position_start"),elements:["sidebar-main","sidebar-container","panel-sidebar-select-box","panel-sidebar-box","tabbrowser-tabbox"].map(id=>{let e=document.getElementById(id);let r=e?.getBoundingClientRect();return {id,rect:r&&{x:r.x,y:r.y,w:r.width,h:r.height},expanded:e?.getAttribute("expanded"),order:e&&getComputedStyle(e).order}})}'
    before=js(dims)
    el=next((e for e in before['elements'] if e['id']=='sidebar-container' and e['rect'] and e['rect']['w']>0),None)
    if el:
     rr=el['rect']
     m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'hover','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':int(rr['x']+min(20,rr['w']/2)),'y':int(rr['y']+200),'duration':100}]}]})
    samples=[]
    for i in range(5):time.sleep(.3);samples.append(js(dims))
    r['observations'].append({'nativeStart':native,'panelAtRight':panel,'before':before,'hoverSamples':samples});shot('KAN-29/layout-'+str(native)+'-'+str(panel))
 if scenario=='desktop-drag':
  m._send_message('WebDriver:SetWindowRect',{'x':50,'y':50,'width':850,'height':700})
  js('window.qaTabs=Array.from({length:3},(_,i)=>gBrowser.addTab("data:text/html,<title>QA desktop drag "+i+"</title>QA "+i,{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));window.qaGroup=gBrowser.addTabGroup(qaTabs,{label:"QA drag"});gBrowser.selectedTab=qaTabs[0];window.qaEvents=[];for(let type of ["dragstart","dragend","drop"]){window.addEventListener(type,e=>qaEvents.push({type:e.type,trusted:e.isTrusted,x:e.screenX,y:e.screenY,target:e.target.id||e.target.className}),true)}');time.sleep(2)
  p=rect('#floorp-stack-items .floorp-stack-tab:nth-child(2)');pos=js('return {x:window.mozInnerScreenX,y:window.mozInnerScreenY,outerX:screenX,outerY:screenY,width:outerWidth,height:outerHeight}')
  import subprocess
  env=os.environ.copy();env['DISPLAY']=':92';env['LD_LIBRARY_PATH']='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu'
  def xd(*args):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],env=env,check=True)
  x=int(pos['x']+p['x']);y=int(pos['y']+p['y']);shot('KAN-5/desktop-before')
  xd('mousemove','--sync',x,y);xd('mousedown',1);time.sleep(.3);xd('mousemove','--sync',x+30,y+5);time.sleep(.5);xd('mousemove','--sync',1250,850);time.sleep(.5);xd('mouseup',1);time.sleep(3)
  r['observations'].append({'input':'OS XTest synthetic drag; not physical mouse','start':{'proxy':p,'window':pos},'events':js('return qaEvents'),'windows':js('return [...Services.wm.getEnumerator("navigator:browser")].map(w=>({x:w.screenX,y:w.screenY,tabs:[...w.gBrowser.tabs].map(t=>({url:t.linkedBrowser.currentURI.spec,title:t.linkedBrowser.contentTitle}))}))')});shot('KAN-5/desktop-after')
 if scenario=='wheel-os':
  import subprocess
  env=os.environ.copy();env['DISPLAY']=':92';env['LD_LIBRARY_PATH']='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu'
  def xd(*args):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],env=env,check=True)
  js('window.qaWheel=[];window.addEventListener("wheel",e=>qaWheel.push({target:e.target.id||e.target.className,deltaX:e.deltaX,deltaY:e.deltaY,mode:e.deltaMode,trusted:e.isTrusted}),true);window.qaTabs=Array.from({length:30},(_,i)=>gBrowser.addTab("about:blank",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));window.qaGroup=gBrowser.addTabGroup(qaTabs,{label:"QA wheel OS"});gBrowser.selectedTab=qaTabs[0]');time.sleep(2)
  for kind in ['stack','normal']:
   if kind=='normal':js('gBrowser.ungroupTab(qaTabs[0]);for(let t of qaTabs.slice(1))gBrowser.ungroupTab(t);gBrowser.selectedTab=qaTabs[0]');time.sleep(2)
   selector='#floorp-stack-scroller' if kind=='stack' else '#tabbrowser-tabs'
   p=rect(selector);pos=js('return {x:mozInnerScreenX,y:mozInnerScreenY}')
   measure='let e='+('document.querySelector("#floorp-stack-scroller")' if kind=='stack' else 'gBrowser.tabContainer.arrowScrollbox.scrollbox')+';return {left:e.scrollLeft,top:e.scrollTop,width:e.clientWidth,total:e.scrollWidth}'
   js(('document.querySelector("#floorp-stack-scroller")' if kind=='stack' else 'gBrowser.tabContainer.arrowScrollbox.scrollbox')+'.scrollTo({left:99999,behavior:"instant"})');time.sleep(.8);before=js(measure);js('qaWheel.length=0');xd('mousemove','--sync',int(pos['x']+p['x']),int(pos['y']+p['y']));xd('click',4);time.sleep(1)
   r['observations'].append({'kind':kind,'input':'OS XTest wheel button4 one notch; not physical','before':before,'after':js(measure),'events':js('return qaWheel')});shot('KAN-39/os-'+kind)
 if scenario=='inspect':
  r['observations'].append(js('return {floorpKeys:Object.keys(gFloorp),design:Services.prefs.getStringPref("floorp.design.configs"),zen:Object.getOwnPropertyNames(Object.getPrototypeOf(window.__floorpZenModeController))}'));shot('environment/floorp-first')
 if scenario=='middle':
  js('window.qaTabs=Array.from({length:3},(_,i)=>gBrowser.addTab("data:text/html,<title>QA middle "+i+"</title>QA "+i,{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));window.qaGroup=gBrowser.addTabGroup(qaTabs,{label:"QA middle"});gBrowser.selectedTab=qaTabs[0];');time.sleep(2)
  r['observations'].append(js('return {proxies:document.querySelectorAll(".floorp-stack-tab").length,tabs:qaTabs.map(t=>({connected:t.isConnected,selected:t.selected,url:t.linkedBrowser.currentURI.spec}))}'))
  shot('KAN-40/before')
  for index in [1,0]:
   p=rect('#floorp-stack-items .floorp-stack-tab'+(':nth-child(2)' if index==1 else ':first-child'))
   click(p['x'],p['y'],1);time.sleep(.8)
   r['observations'].append(js('return qaTabs.map(t=>({connected:t.isConnected,selected:t.selected}))'))
  shot('KAN-40/after')
 if scenario=='wheel':
  js('window.qaTabs=Array.from({length:30},(_,i)=>gBrowser.addTab("about:blank",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));window.qaGroup=gBrowser.addTabGroup(qaTabs,{label:"QA wheel"});gBrowser.selectedTab=qaTabs[0];');time.sleep(2)
  for mode,delta in [(0,120),(1,1),(2,1)]:
   js('document.querySelector("#floorp-stack-scroller").scrollLeft=0');time.sleep(.5)
   a=js('let e=document.querySelector("#floorp-stack-scroller");return {left:e.scrollLeft,width:e.clientWidth,total:e.scrollWidth}')
   js('document.querySelector("#floorp-stack-scroller").dispatchEvent(new WheelEvent("wheel",{bubbles:true,cancelable:true,deltaY:arguments[1],deltaMode:arguments[0]}))',[mode,delta]);time.sleep(.8)
   r['observations'].append({'input':'constructed WheelEvent','mode':mode,'delta':delta,'before':a,'after':js('return document.querySelector("#floorp-stack-scroller").scrollLeft')})
  shot('KAN-39/stack')
 if scenario=='search':
  m.set_context('content');m.navigate('about:preferences');time.sleep(2)
  from marionette_driver.by import By
  from marionette_driver.keys import Keys
  field=js('return document.querySelector("#searchInput").shadowRoot.querySelector("input")');field.send_keys('proxy');time.sleep(3)
  s='''function text(root){let out=[];for(let e of root.children||[]){if(e.checkVisibility?.()){if(e.childElementCount===0)out.push(e.textContent);if(e.shadowRoot)out.push(...text(e.shadowRoot));out.push(...text(e));}}return out;}return {url:location.href,input:document.querySelector("#searchInput").value,visible:text(document).join("\\n")};'''
  r['observations'].append(js(s));shot('KAN-31/'+str(port)+'-first')
  m.navigate('about:preferences#privacy');time.sleep(1);m.navigate('about:preferences#connectionSecurity');time.sleep(2)
  r['observations'].append(js(s));field=js('return document.querySelector("#searchInput").shadowRoot.querySelector("input")');field.send_keys('proxy');time.sleep(3);r['observations'].append(js(s));shot('KAN-31/'+str(port)+'-after')
except Exception as e:r['error']=repr(e);r['traceback']=traceback.format_exc()
finally:
 (ROOT/(scenario+'-'+str(port)+'.json')).write_text(json.dumps(r,ensure_ascii=False,indent=2));print(json.dumps(r,ensure_ascii=False)[:7000]);m.delete_session()
