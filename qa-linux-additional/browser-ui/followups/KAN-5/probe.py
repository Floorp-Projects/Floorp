"""UI stack creation + actual OS XTest drop. No adoption/group state APIs."""
import os,sys,time,json,subprocess,traceback,urllib.parse,base64
from pathlib import Path
if os.environ.get('QA_KAN5_START_FOLLOWUP')!='1':raise SystemExit('post-package gate closed')
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);phase=sys.argv[2];m=Marionette('localhost',port=port,socket_timeout=35);m.start_session();m.timeout.page_load=25;m.set_context('chrome')
X='/workspace/.floorp-cloud/sysroot/usr/bin/xdotool'
def js(code,args=None):return m.execute_script(code,script_args=args or [])
def xd(*args):return subprocess.check_output([X,*map(str,args)],env=os.environ,text=True).strip()
def wait(check,why,seconds=12):
 until=time.monotonic()+seconds
 while time.monotonic()<until:
  if check():return
  time.sleep(.1)
 raise RuntimeError('fixture precondition/timeout: '+why)
def point(expression,window='window'):
 return js('const w='+window+',e='+expression+';if(!e)throw Error("missing actual UI target");const r=e.getBoundingClientRect();return {x:w.mozInnerScreenX+r.x+r.width/2,y:w.mozInnerScreenY+r.y+r.height/2,width:r.width,height:r.height,label:e.label||e.textContent?.trim(),rect:r.toJSON()};')
def click(expression,button=1):
 p=point(expression);assert p['width']>=8 and p['height']>=8,p;xd('mousemove','--sync',int(p['x']),int(p['y']));xd('click',button);time.sleep(.3);return p
def shot(name):
 try:
  from PIL import ImageGrab
  ImageGrab.grab(xdisplay=os.environ['DISPLAY']).save(R/'screenshots'/f'{port}-{name}.png')
 except Exception:
  (R/'screenshots'/f'{port}-{name}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
r={'key':'KAN-5','canonicalPr':2763,'phase':phase,'fixture':'actual settings/group UI; data-URL pages; native window rectangles; OS XTest pointer/drag (synthetic, not physical)','apiAdoptionUsed':False,'observations':[]}
normalQuit=False
try:
 r['product']=js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUser:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode"),stacksEnabled:Services.prefs.getBoolPref("floorp.tabstacks.enabled",false)}')
 assert Path(r['product']['gre']).resolve()==Path(os.environ['QA_EXPECTED_PRODUCT']).parent.resolve(),'wrong GreD; not a valid product control'
 assert not r['product']['focusTestMode'],'native focus precondition not satisfied'
 if phase=='enable':
  m.set_context('content');m.navigate('about:hub#/features/design');time.sleep(1)
  p=js('let e=document.querySelector("label[for=enable-tab-stacks]");if(!e)throw Error("missing enable-stack label");e.scrollIntoView({block:"center"});let r=e.getBoundingClientRect();return{x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,height:r.height,checked:document.getElementById("enable-tab-stacks").checked,text:e.innerText};');assert p['width']>20 and p['height']>10
  if not p['checked']:
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'enable','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':int(p['x']),'y':int(p['y'])},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]});time.sleep(.5)
  r['enableUi']=p;m.set_context('chrome');assert js('return Services.prefs.getBoolPref("floorp.tabstacks.enabled",false)');r['normalRestartRequired']=True;r['normalQuitRequested']=True
  (R/f'setup-{port}.json').write_text(json.dumps(r,indent=2));r['normalQuitRequested']=True;normalQuit=True;js('Services.prefs.savePrefFile(null);Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));')
 elif phase=='drop':
  assert r['product']['stacksEnabled'],'normal UI setting did not survive restart'
  titles=['QA KAN5 source A','QA KAN5 source B','QA KAN5 source C','QA KAN5 ordinary control'];uris=['data:text/html;charset=utf-8,'+urllib.parse.quote('<title>'+t+'</title><h1>'+t+'</h1>') for t in titles];handles=[]
  for i,uri in enumerate(uris):
   if i:
    old=set(m.window_handles);xd('key','ctrl+t');wait(lambda:len(set(m.window_handles)-old)==1,'new-tab keyboard UI');m.switch_to_window((set(m.window_handles)-old).pop())
   m.set_context('content');m.navigate(uri);handles.append(m.current_window_handle);m.set_context('chrome');wait(lambda:js('return gBrowser.selectedBrowser.contentTitle')==titles[i],'loaded source page');uris[i]=js('return gBrowser.selectedBrowser.currentURI.spec')
  m.set_window_rect(x=20,y=80,width=1580,height=760);time.sleep(.4);sourceXid=xd('getactivewindow')
  old=set(m.window_handles);xd('key','ctrl+n');wait(lambda:len(set(m.window_handles)-old)==1,'new-window keyboard UI');targetHandle=(set(m.window_handles)-old).pop();m.switch_to_window(targetHandle);m.set_context('content');targetUri='data:text/html,<title>QA KAN5 destination</title><h1>Existing target</h1>';m.navigate(targetUri);m.set_context('chrome');targetUri=js('return gBrowser.selectedBrowser.currentURI.spec');m.set_window_rect(x=860,y=80,width=750,height=760);time.sleep(.5);targetXid=xd('getactivewindow');assert sourceXid!=targetXid
  m.switch_to_window(handles[2]);m.set_context('chrome');xd('windowactivate','--sync',sourceXid)
  js('window.qaKAN5Source=window;window.qaKAN5Target=[...Services.wm.getEnumerator("navigator:browser")].find(w=>[...w.gBrowser.tabs].some(t=>t.linkedBrowser.currentURI.spec===arguments[0]));if(!qaKAN5Target)throw Error("target missing");qaKAN5Source.qaKAN5Role="source";qaKAN5Target.qaKAN5Role="target";',[targetUri])
  def tabexpr(uri):return '[...gBrowser.tabs].find(t=>t.linkedBrowser.currentURI.spec==='+json.dumps(uri)+')'
  click(tabexpr(uris[0]));xd('keydown','ctrl')
  try:click(tabexpr(uris[1]));click(tabexpr(uris[2]))
  finally:xd('keyup','ctrl')
  r['selectedBeforeGrouping']=js('return {selected:[...gBrowser.selectedTabs].map(t=>({title:t.linkedBrowser.contentTitle,rect:t.getBoundingClientRect().toJSON()})),all:[...gBrowser.tabs].map(t=>({title:t.linkedBrowser.contentTitle,rect:t.getBoundingClientRect().toJSON()}))}');assert len(r['selectedBeforeGrouping']['selected'])==3,'real UI multiselection failed'
  click(tabexpr(uris[2]),3)
  r['contextMenu']=js('return [...document.querySelectorAll("#tabContextMenu menuitem,#tabContextMenu menu")].filter(e=>!e.hidden&&e.getBoundingClientRect().width>5).map(e=>({id:e.id,label:e.label}))');shot('group-context')
  js('window.qaGroupCommands=[];window.addEventListener("command",e=>qaGroupCommands.push({id:e.target.id,label:e.target.label,trusted:e.isTrusted}),true)')
  r['groupBeforeMenuCommand']=js('return {groups:gBrowser.tabGroups.map(g=>({id:g.id,titles:g.tabs.map(t=>t.linkedBrowser.contentTitle)})),proxies:document.querySelectorAll(".floorp-stack-tab").length}')
  menuIds=[item['id'] for item in r['contextMenu']];index=menuIds.index('context_moveTabToNewGroup');assert index==1,'unexpected native menu layout; do not guess command'
  xd('key','Home');time.sleep(.15);xd('key','Down');time.sleep(.15)
  r['keyboardMenuSelection']=js('let p=document.getElementById("tabContextMenu");return {activeChild:p.activeChild?.id,marked:[...p.querySelectorAll("menuitem")].filter(e=>e.getAttribute("_moz-menuactive")==="true"||e.getAttribute("menuactive")==="true").map(e=>e.id)}')
  active=r['keyboardMenuSelection'];assert active.get('activeChild')=='context_moveTabToNewGroup' or 'context_moveTabToNewGroup' in active['marked'],'native keyboard item selection unproven'
  xd('key','Return');time.sleep(.5)
  r['groupAfterMenuCommand']=js('return {commands:qaGroupCommands,groups:gBrowser.tabGroups.map(g=>({id:g.id,titles:g.tabs.map(t=>t.linkedBrowser.contentTitle)})),proxies:document.querySelectorAll(".floorp-stack-tab").length,openPopups:[...document.querySelectorAll("panel,menupopup")].filter(e=>e.state==="open").map(e=>e.id)}');shot('group-command')
  r['groupEditorFocus']=js('let e=document.activeElement;while(e?.shadowRoot?.activeElement)e=e.shadowRoot.activeElement;return {tag:e?.localName,id:e?.id,type:e?.type,role:e?.getAttribute("role"),value:e?.value}')
  focus=r['groupEditorFocus'];assert focus['tag'] in ['input','textbox'] and focus.get('type') in ['text',None,''],'expected native group name textbox focus'
  xd('key','ctrl+a');xd('type','--clearmodifiers','--delay','35','QA KAN5');xd('key','Return');time.sleep(.5)
  r['groupAfterPopupClose']=js('return {groups:gBrowser.tabGroups.map(g=>({id:g.id,titles:g.tabs.map(t=>t.linkedBrowser.contentTitle)})),proxies:document.querySelectorAll(".floorp-stack-tab").length}')
  wait(lambda:js('return gBrowser.tabGroups.some(g=>g.tabs.length===3&&g.tabs.every(t=>arguments[0].includes(t.linkedBrowser.currentURI.spec)))',[uris[:3]]),'UI-created three-tab stack')
  click('document.querySelector("tab-group .tab-group-label-container")');wait(lambda:js('return document.querySelectorAll("#floorp-stack-items .floorp-stack-tab").length')==3,'actual second-row proxies')
  r['uiStackCreated']=True;m.set_window_rect(x=20,y=80,width=750,height=760);time.sleep(.5)
  js('for(let w of [qaKAN5Source,qaKAN5Target]){w.qaKAN5Events=[];for(let type of ["dragstart","dragover","drop","dragend"]){w.addEventListener(type,e=>{if(w.qaKAN5Events.length<100||e.type!=="dragover")w.qaKAN5Events.push({type:e.type,trusted:e.isTrusted,x:e.screenX,y:e.screenY,target:e.target.id||e.target.className,types:[...(e.dataTransfer?.types||[])]})},true)}}')
  def snapshot():return js('return [...Services.wm.getEnumerator("navigator:browser")].map(w=>({role:w.qaKAN5Role||"unexpected",screen:{x:w.screenX,y:w.screenY,width:w.outerWidth,height:w.outerHeight,state:w.windowState,sizemode:w.document.documentElement.getAttribute("sizemode")},selected:w.gBrowser.selectedTab.linkedBrowser.contentTitle,tabs:[...w.gBrowser.tabs].map(t=>({uri:t.linkedBrowser.currentURI.spec,title:t.linkedBrowser.contentTitle,pending:t.hasAttribute("pending"),group:t.group?.id,dragData:!!t._dragData})),events:w.qaKAN5Events||[]}))')
  def osmeta(xid):return subprocess.check_output(['xprop','-id',xid,'_NET_WM_STATE','_NET_FRAME_EXTENTS','_NET_WM_PID'],env=os.environ,text=True)
  r['windowOwnership']={'sourceXid':sourceXid,'targetXid':targetXid,'sourcePid':xd('getwindowpid',sourceXid),'targetPid':xd('getwindowpid',targetXid),'sourceEwmh':osmeta(sourceXid),'targetEwmh':osmeta(targetXid)};assert r['windowOwnership']['sourcePid']==r['windowOwnership']['targetPid']
  assert all(w['screen']['sizemode']=='normal' for w in snapshot()),'both windows must be normal'
  def drop_case(name,expression,expectedUri):
   before=snapshot();start=point(expression);target=js('const w=qaKAN5Target,e=[...w.gBrowser.tabs].at(-1),r=e.getBoundingClientRect();return{x:w.mozInnerScreenX+r.right-3,y:w.mozInnerScreenY+r.y+r.height/2,rect:r.toJSON()};');assert start['width']>10 and start['height']>10
   xd('windowactivate','--sync',sourceXid);xd('mousemove','--sync',int(start['x']),int(start['y']));xd('mousedown',1);time.sleep(.15)
   try:
    xd('mousemove','--sync',int(start['x']+25),int(start['y']+5));time.sleep(.2)
    for n in range(1,13):
     xd('mousemove','--sync',int(start['x']+(target['x']-start['x'])*n/12),int(start['y']+(target['y']-start['y'])*n/12));time.sleep(.04)
    time.sleep(.1)
   finally:xd('mouseup',1)
   time.sleep(1);after=snapshot();shot(name);time.sleep(2);late=snapshot();old={w['role']:w for w in before};new={w['role']:w for w in late}
   valid=set(new)=={'source','target'} and len(new['source']['tabs'])==len(old['source']['tabs'])-1 and len(new['target']['tabs'])==len(old['target']['tabs'])+1
   valid=valid and not any(t['uri']==expectedUri for t in new.get('source',{}).get('tabs',[])) and sum(t['uri']==expectedUri for t in new.get('target',{}).get('tabs',[]))==1
   eventDelta={role:new[role]['events'][len(old[role]['events']):] for role in new if role in old};nativeStart=any(e['type']=='dragstart' and e['trusted'] for e in eventDelta.get('source',[]));nativeDrop=any(e['type']=='drop' and e['trusted'] for e in eventDelta.get('target',[]));valid=valid and nativeStart and nativeDrop
   oldPayload=sorted((t['uri'],t['title']) for w in before for t in w['tabs']);newPayload=sorted((t['uri'],t['title']) for w in late for t in w['tabs']);knownEmptyBlanks={t['uri'] for w in before for t in w['tabs'] if t['uri'] in ['about:blank','about:newtab'] and not t['title']};valid=valid and oldPayload==newPayload and all((t['title'] or t['uri'] in knownEmptyBlanks) and not t['pending'] for w in late for t in w['tabs'])
   observation={'case':name,'input':'OS XTest native mouse path; no dispatchEvent/DataTransfer/adoption API','expectedUri':expectedUri,'start':start,'target':target,'before':before,'after':after,'late':late,'nativeDragStartSeen':nativeStart,'trustedTargetDropSeen':nativeDrop,'eventDelta':eventDelta,'singleTabPayloadPreserved':valid};r['observations'].append(observation);(R/f'drop-{port}.json').write_text(json.dumps(r,indent=2));return valid
  shot('before')
  assert js('return gBrowser.tabs.at(-1).linkedBrowser.currentURI.spec')==uris[3],'positive-control tab must be last before native keyboard selection'
  xd('key','alt+9');wait(lambda:js('return gBrowser.selectedBrowser.currentURI.spec')==uris[3],'native UI selects/scolls ordinary control into view')
  positive=drop_case('ordinary-positive-control',tabexpr(uris[3]),uris[3]);r['ordinaryPositiveControl']=positive
  if not positive:raise RuntimeError('ordinary actual OS DnD positive control failed; stack result would be uninterpretable')
  xd('windowactivate','--sync',sourceXid);click('document.querySelector("tab-group .tab-group-label-container")');click('[...document.querySelectorAll("#floorp-stack-items .floorp-stack-tab")].find(e=>e.textContent.includes('+json.dumps(titles[0])+'))')
  r['proxyExistingWindow']=drop_case('individual-stack-proxy-existing-window','[...document.querySelectorAll("#floorp-stack-items .floorp-stack-tab")].find(e=>e.textContent.includes('+json.dumps(titles[1])+'))',uris[1])
  r['completed']=True;r['originalEnvironmentSatisfied']=False
except Exception as e:
 r.update(error=str(e),traceback=traceback.format_exc(),classification='fixture/infrastructure or observed behavior pending review; not automatically a product defect')
 try:
  m.set_context('chrome');shot('fixture-error');r['failureUi']=js('return {url:gBrowser.selectedBrowser.currentURI.spec,stackEnabled:Services.prefs.getBoolPref("floorp.tabstacks.enabled",false),proxyCount:document.querySelectorAll(".floorp-stack-tab").length,menus:[...document.querySelectorAll("menupopup[open] menuitem")].filter(e=>!e.hidden).map(e=>({id:e.id,label:e.label}))}')
 except Exception:pass
finally:
 if phase=='drop':(R/f'drop-{port}.json').write_text(json.dumps(r,indent=2))
 elif not normalQuit:(R/f'setup-{port}.json').write_text(json.dumps(r,indent=2))
 print(json.dumps({'phase':phase,'completed':r.get('completed'),'normalQuit':normalQuit,'error':r.get('error'),'proxyExistingWindow':r.get('proxyExistingWindow')}))
 if not normalQuit:
  try:
   m.set_context('chrome');r['normalQuitRequested']=True;js('Services.prefs.savePrefFile(null);Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));');normalQuit=True
  except Exception:pass
if r.get('error'):raise SystemExit(1)
