from marionette_driver.marionette import Marionette
from marionette_driver.by import By
from pathlib import Path
import os,sys,time,json,base64,subprocess,traceback
R=Path(os.environ['QA97_OUTPUT']);product='full-product';port=int(sys.argv[1]);m=Marionette('localhost',port=port,socket_timeout=60);m.start_session();nested_mode=False;out={'productKind':product,'input':'OS XTest synthetic mouse/keyboard; W3C settings toggle; not physical hardware','observations':[]}
def chrome(s):m.set_context('chrome');return m.execute_script(s)
def content(s):m.set_context('content');m.switch_to_frame(None);return m.execute_script(s)
def click(x,y):
 m.set_context('content');m.switch_to_frame(None);m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'ui','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','duration':0,'origin':'viewport','x':int(x),'y':int(y)},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]})
def xd(*args):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],env=os.environ.copy(),check=True)
def middlejs(s):
 m.set_context('content');m.switch_to_frame(None);m.switch_to_frame(m.find_element(By.ID,'frame'));v=m.execute_script(s);m.switch_to_frame(None);return v
def framejs(s):
 m.set_context('content');m.switch_to_frame(None);m.switch_to_frame(m.find_element(By.ID,'frame'))
 if nested_mode:m.switch_to_frame(m.find_element(By.ID,'leaf'))
 v=m.execute_script(s);m.switch_to_frame(None);return v
def measure():
 a=content('return {root:{top:scrollY,max:document.scrollingElement.scrollHeight-innerHeight},overflow:{top:document.querySelector("#box").scrollTop,max:document.querySelector("#box").scrollHeight-document.querySelector("#box").clientHeight},active:document.activeElement.id||document.activeElement.localName,visible:document.visibilityState}')
 a['frame']=framejs('return {top:scrollY,max:document.scrollingElement.scrollHeight-innerHeight,active:document.activeElement.localName}')
 if nested_mode:a['ancestorFrame']=middlejs('return {top:scrollY,max:document.scrollingElement.scrollHeight-innerHeight,active:document.activeElement.id||document.activeElement.localName}')
 a['focus']=chrome('let s=Services.focus;return {contentContext:s.focusedContentBrowsingContext?.id,focusedTop:s.focusedContentBrowsingContext?.top?.id,context:s.focusedBrowsingContext?.id,top:gBrowser.selectedBrowser.browsingContext.id,frames:gBrowser.selectedBrowser.browsingContext.children.map(b=>({id:b.id,uri:b.currentURI?.spec,children:b.children.map(c=>({id:c.id,uri:c.currentURI?.spec}))}))}')
 return a
try:
 out['sourceSha']=os.environ['QA97_SOURCE_SHA']
 out['scope']='guards-only; primary24 retained in full-29981' if os.environ.get('QA97_GUARDS_ONLY')=='1' else 'complete matrix and guards'
 out['candidateKind']='unmodified extracted full Linux product; no diagnostic asset replacement'
 out['metadata']=chrome('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,platformBuild:Services.appinfo.platformBuildID,platformVersion:Services.appinfo.platformVersion,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode"),documentFocused:document.hasFocus()}')
 out['resourceLocation']=chrome('let c=Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry);return {gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,executable:Services.dirsvc.get("XREExeF",Ci.nsIFile).path,module:c.convertChromeURL(Services.io.newURI("chrome://noraneko/content/assets/js/gestures.js")).spec}')
 assert out['resourceLocation']['gre']==os.environ['QA97_EXPECT_GRE'],out['resourceLocation']
 assert out['metadata']['version']=='159.0.1',out['metadata']
 assert out['metadata']['build']=='20261103153232' and out['metadata']['platformBuild']=='20261103153232',out['metadata']
 out['liveRuntimeGate']='passed native App+Platform BuildID 20261103153232; packaged INI recorded separately'
 if product!='firefox':
  m.set_context('content');m.navigate('about:hub#/features/gesture');time.sleep(1)
  setting=content('let e=document.querySelector("[data-setting=mouse-gesture-enabled]");let r=e.getBoundingClientRect();return {html:e.outerHTML,x:r.x+r.width/2,y:r.y+r.height/2,checked:e.checked,aria:e.getAttribute("aria-checked")}')
  out['settingsBefore']=setting
  if setting['checked']!=True and setting['aria']!='true':click(setting['x'],setting['y']);time.sleep(1)
  (R/(product+'-settings.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')))
  out['config']=chrome('return {enabled:Services.prefs.getBoolPref("floorp.mousegesture.enabled"),config:JSON.parse(Services.prefs.getStringPref("floorp.mousegesture.config"))}')
  assert out['config']['enabled'];assert any(x['action']=='gecko-scroll-to-top' and x['pattern']==['up'] for x in out['config']['config']['actions'])
 chrome('window.qa97={events:[],popups:0};for(let n of ["mousedown","mouseup","contextmenu"])window.addEventListener(n,e=>{let r={type:n,trusted:e.isTrusted,x:e.clientX,y:e.clientY};qa97.events.push(r);setTimeout(()=>r.prevented=e.defaultPrevented,0)},true);window.addEventListener("popupshown",e=>{if(e.target.id==="contentAreaContextMenu")qa97.popups++},true)')
 for target in ([] if os.environ.get('QA97_GUARDS_ONLY')=='1' else ['root','overflow','same-frame','cross-frame','nested-same-frame','nested-cross-frame']):
  nested_mode=target.startswith('nested-')
  m.set_context('content');m.switch_to_frame(None);m.navigate('http://127.0.0.1:29971/'+('nested-cross' if target=='nested-cross-frame' else 'nested-same' if target=='nested-same-frame' else 'cross' if target=='cross-frame' else 'same')+'?token=matrix-'+target);time.sleep(.4)
  for mode in (['gesture','keyboard'] if product!='firefox' else ['keyboard']):
   for direction in ['top','bottom']:
    content('scrollTo({top:1800,behavior:"instant"});document.querySelector("#box").scrollTop=1800')
    if nested_mode:middlejs('scrollTo({top:1800,behavior:"instant"})')
    framejs('scrollTo({top:1800,behavior:"instant"})');time.sleep(.1)
    m.set_context('content');m.switch_to_frame(None);pos=m.execute_script('let t=arguments[0],e=document.querySelector(t==="overflow"?"#box":"#frame"),r=e.getBoundingClientRect();return {x:t==="root"?innerWidth-70:r.x+r.width/2,y:t==="root"?300:r.y+r.height/2,sx:mozInnerScreenX,sy:mozInnerScreenY}',script_args=[target])
    xd('mousemove','--sync',int(pos['sx']+pos['x']),int(pos['sy']+pos['y']),'click','1');time.sleep(.1);before=measure();chrome('qa97.events=[];qa97.popups=0')
    held=[]
    if mode=='gesture':
     xd('mousedown','3');time.sleep(.06)
     try:
      sign=-1 if direction=='top' else 1
      for j in range(1,9):xd('mousemove','--sync',int(pos['sx']+pos['x']),int(pos['sy']+pos['y']+sign*j*18));time.sleep(.02)
      time.sleep(.1);held=chrome('return [...document.querySelectorAll("[id*=gesture],[class*=gesture]")].filter(e=>e.textContent).map(e=>({id:e.id,text:e.textContent.slice(0,150)}))')
     finally:xd('mouseup','3')
    else:xd('key','ctrl+Home' if direction=='top' else 'ctrl+End')
    time.sleep(1.1);after=measure();events=chrome('return qa97');expectedKey='frame' if 'frame' in target else target;wanted=0 if direction=='top' else before[expectedKey]['max'];actual=after[expectedKey]['top']
    checkedKeys=[k for k in ['root','overflow','frame','ancestorFrame'] if k in before]
    untouched=all(abs(before[k]['top']-after[k]['top'])<=1 for k in checkedKeys if k!=expectedKey)
    expectedContext=before['focus']['top']
    if 'frame' in target:
     node=before['focus']['frames'][0]
     if nested_mode:node=node['children'][0]
     expectedContext=node['id']
    focusQualified=before['focus']['contentContext']==expectedContext and before['focus']['focusedTop']==before['focus']['top']
    row={'target':target,'mode':mode,'direction':direction,'before':before,'heldLabel':held,'after':after,'events':events,'expectedTarget':expectedKey,'expectedTop':wanted,'actualTop':actual,'atExpectedEndpoint':abs(actual-wanted)<=1,'nonTargetsUnchanged':untouched,'actualFocusedContextQualified':focusQualified,'expectedFocusedContext':expectedContext};out['observations'].append(row);print(product,target,mode,direction,row['atExpectedEndpoint'],before[expectedKey]['top'],actual,flush=True)
    if mode=='gesture':(R/(product+'-'+target+'-'+direction+'.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')))
 nested_mode=False
 from guards import run as run_guards
 out['guards']=run_guards(m,xd,R)
except Exception as e:
 out['error']=repr(e);out['traceback']=traceback.format_exc();print(out['traceback'],flush=True)
 if (R/'guards.json').exists():out['guards']=json.loads((R/'guards.json').read_text())
finally:
 out['normalQuitRequested']=True
 try:
  m.set_context('chrome');m.execute_script('Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true;')
 except Exception as e:out['normalQuitTransportResult']=repr(e)
 (R/'probe.json').write_text(json.dumps(out,indent=2));print(json.dumps({'source':out.get('sourceSha'),'matrixCases':len(out['observations']),'error':out.get('error'),'guards':out.get('guards'),'normalQuitRequested':out['normalQuitRequested']}),flush=True)
 try:m.client.close()
 except Exception:pass
 if out.get('error') or len(out['observations'])!=(0 if os.environ.get('QA97_GUARDS_ONLY')=='1' else 24) or not all(r['atExpectedEndpoint'] and r['nonTargetsUnchanged'] and r['actualFocusedContextQualified'] and r['events']['popups']==0 for r in out['observations']) or not out.get('guards',{}).get('allRequiredPass'):sys.exit(1)
