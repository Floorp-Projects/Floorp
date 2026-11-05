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
   r['observations'].append({'dark':dark,'normal':js(style)});shot('KAN-93/chrome-'+str(dark))
   js('window.__floorpZenModeController.setEnabledFromUser(true)');time.sleep(1)
   # reveal through pointer movement to upper edge
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'reveal','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':500,'y':2,'duration':100}]}]});time.sleep(.5)
   r['observations'].append({'dark':dark,'zen':js(style)});shot('KAN-87/zen-'+str(dark))
   js('window.__floorpZenModeController.setEnabledFromUser(false)')
 if scenario=='gesture':
  js('Services.prefs.setBoolPref("floorp.mousegesture.enabled",true);let c=JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config","{}"));c.actions=[{pattern:["up"],action:"gecko-open-new-tab"},{pattern:["up","down"],action:"gecko-open-new-tab"},{pattern:["down","up"],action:"gecko-open-new-tab"},{pattern:["right","down"],action:"gecko-open-new-tab"}];Services.prefs.setStringPref("floorp.mousegesture.config",JSON.stringify(c))');time.sleep(2)
  r['config']=js('return Services.prefs.getStringPref("floorp.mousegesture.config")')
  cases=[('up',[(600,650),(600,450)]),('updown',[(600,650),(600,450),(600,650)]),('denseupdown',[(600,y) for y in list(range(650,449,-10))+list(range(460,651,10))]),('downup',[(600,450),(600,650),(600,450)]),('rightdown',[(450,450),(650,450),(650,650)])]
  for name,points in cases:
   js('gBrowser.selectedTab=gBrowser.addTab("data:text/html,<h1>QA gesture input</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})');time.sleep(.5)
   before=js('return gBrowser.tabs.length')
   js('window.qaGestureEvents=[];for(let n of ["mousedown","mousemove","mouseup","contextmenu"])window.addEventListener(n,e=>{if(qaGestureEvents.length<200)qaGestureEvents.push({type:e.type,x:e.clientX,y:e.clientY,buttons:e.buttons,trusted:e.isTrusted})},{capture:true,once:false})')
   acts=[{'type':'pointerMove','origin':'viewport','x':points[0][0],'y':points[0][1],'duration':100},{'type':'pointerDown','button':2}]
   for x,y in points[1:]:acts.append({'type':'pointerMove','origin':'viewport','x':x,'y':y,'duration':100})
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'gesture','parameters':{'pointerType':'mouse'},'actions':acts}]});time.sleep(.2)
   label=js('return [...document.querySelectorAll("[id*=gesture],[class*=gesture]")].map(e=>({id:e.id,cls:e.className,text:e.textContent?.slice(0,100)}))');shot('KAN-98/'+name+'-held')
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'gesture','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerUp','button':2}]}]});time.sleep(1)
   r['observations'].append({'name':name,'points':points,'beforeTabs':before,'afterTabs':js('return gBrowser.tabs.length'),'label':label,'events':js('return window.qaGestureEvents')});shot('KAN-98/'+name+'-released')
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
  m.navigate('about:preferences#privacy');time.sleep(1);m.navigate('about:preferences#connection');time.sleep(2)
  r['observations'].append(js(s));field=js('return document.querySelector("#searchInput").shadowRoot.querySelector("input")');field.send_keys('proxy');time.sleep(3);r['observations'].append(js(s));shot('KAN-31/'+str(port)+'-after')
except Exception as e:r['error']=repr(e);r['traceback']=traceback.format_exc()
finally:
 (ROOT/(scenario+'-'+str(port)+'.json')).write_text(json.dumps(r,ensure_ascii=False,indent=2));print(json.dumps(r,ensure_ascii=False)[:7000]);m.delete_session()
