import os,sys,json,time,subprocess,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);m=Marionette('localhost',port=port,socket_timeout=90);m.start_session();m.set_context('chrome')
def js(s,a=[]):return m.execute_script(s,script_args=a)
def xd(*a):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,a)],env={**os.environ,'LD_LIBRARY_PATH':'/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu:'+os.environ.get('LD_LIBRARY_PATH','')},check=True)
def move(x,y):
 o=js('return [mozInnerScreenX,mozInnerScreenY]');xd('mousemove','--sync',int(o[0]+x),int(o[1]+y))
def snap():return js('''let e=document.getElementById('sidebar-box'),c=getComputedStyle(e);return {time:performance.now(),documentFocused:document.hasFocus(),isOpen:SidebarController.isOpen,currentID:SidebarController.currentID,hidden:e.hidden,opacity:c.opacity,visibility:c.visibility,box:e.getBoundingClientRect().toJSON(),hover:e.matches(':hover'),expanded:e.hasAttribute('data-floorp-sidebar-expanded'),trigger:document.getElementById('floorp-firefox-sidebar-edge-toggle')?.getBoundingClientRect().toJSON(),active:document.activeElement?.id,selected:gBrowser.selectedTab.label};''')
r={'sourceSha':'29a3eb23145da90706d8a94441373278521cc737','productCopy':os.environ.get('QA_PRODUCT','official unchanged'),'fixture':'startup normal design preference selects overlap+hover; OS XTest synthetic input, not physical','observations':[]}
try:
 r['product']=js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode",false),focusTestModeUser:Services.prefs.prefHasUserValue("focusmanager.testmode")}')
 # A bare Xvfb has no window manager to activate clicked top-level windows.
 # Establish real X11 input focus without changing browser implementation state.
 pid=js('return Services.appinfo.processID')
 toolenv={**os.environ,'LD_LIBRARY_PATH':'/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu:'+os.environ.get('LD_LIBRARY_PATH','')}
 ids=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','search','--onlyvisible','--pid',str(pid)],env=toolenv,text=True).split()
 if not ids:raise RuntimeError('No visible owned native product window')
 xd('windowfocus','--sync',ids[0]);time.sleep(.3)
 r['nativeFocusFixture']={'method':'xdotool windowfocus on owned product native window; compensates missing X11 window manager','window':ids[0],'documentFocused':js('return document.hasFocus()')}
 r['windowManager']=os.environ.get('QA_WM','none')
 r['ewmhSupported']=subprocess.run(['xprop','-root','_NET_SUPPORTING_WM_CHECK','_NET_ACTIVE_WINDOW'],env=os.environ.copy(),text=True,capture_output=True).stdout
 r['resourceLocation']=js('return {gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,chrome:Cc["@mozilla.org/chrome/chrome-registry;1"].getService(Ci.nsIChromeRegistry).convertChromeURL(Services.io.newURI("chrome://noraneko/content/assets/js/index9.js")).spec}')
 if 'candidate-pr2934' in r['productCopy'] and 'candidate-pr2934' not in r['resourceLocation']['gre']:raise RuntimeError('Candidate executable resources did not resolve to candidate directory')

 if sys.argv[2].startswith('sidebery') and not os.environ.get('QA_EXTENSION_PREINSTALLED'):
  r['addonInstall']=m.execute_async_script('''const done=arguments[arguments.length-1];const{AddonManager}=ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");const{FileUtils}=ChromeUtils.importESModule("resource://gre/modules/FileUtils.sys.mjs");(async()=>{let i=await AddonManager.getInstallForFile(new FileUtils.File(arguments[0]));await i.install();done({id:i.addon.id,active:i.addon.isActive,signedState:i.addon.signedState,temporary:i.addon.temporarilyInstalled});})().catch(e=>done({error:String(e)}));''',script_args=[str(R/'sidebery.xpi')],script_timeout=30000)
  time.sleep(2)
  r['sidebarMap']=js('return Array.from(SidebarController.sidebars.entries()).map(([id,s])=>({id,title:s.title,extensionId:s.extensionId,visible:s.visible}))')
  r['sidebarUI']=js('return [...document.querySelectorAll("#sidebar-container *,#sidebar-switcher-panel *,#viewSidebarMenu *")].filter(e=>e.localName.includes("button")||e.localName==="menuitem").map(e=>({tag:e.localName,id:e.id,title:e.getAttribute("title"),label:e.getAttribute("label"),aria:e.getAttribute("aria-label"),view:e.getAttribute("view"),rect:e.getBoundingClientRect().toJSON()}))')
  if sys.argv[2]=='sidebery-install-quit':
   js('Services.prefs.savePrefFile(null);Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));');time.sleep(2);r['normalQuitRequested']=True;sys.exit(0)
  if sys.argv[2]=='sidebery-inspect':
   (R/f'{port}-sidebery-install.png').write_bytes(base64.b64decode(m.screenshot(format='base64')));sys.exit(0)
 js("window.qaTabs=[gBrowser.addTab('data:text/html,<title>QA A</title><body style=background:rgb(255,120,120)>A',{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}),gBrowser.addTab('data:text/html,<title>QA B</title><body style=background:rgb(80,220,220)>B',{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})];gBrowser.selectedTab=qaTabs[0]")
 time.sleep(1)
 if sys.argv[2].startswith('sidebery'):
  move(650,550);xd('click',1);time.sleep(.3)
  a=js('let e=document.getElementById("_3c078156-979c-498b-8990-85f7987dd929_-browser-action");return e?e.getBoundingClientRect().toJSON():null');r['sideberyToolbarButton']=a
  if not a or a['width']<=0:raise RuntimeError('Sidebery toolbar action not visible')
  js("window.qaClicks=[];window.addEventListener('click',e=>qaClicks.push({id:e.target.id,tag:e.target.localName,trusted:e.isTrusted,path:e.composedPath().slice(0,4).map(x=>x.id||x.localName)}),true)")
  m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'sidebery-toolbar-ui','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':int(a['x']+a['width']/2),'y':int(a['y']+a['height']/2),'duration':100},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]})
  # Follow W3C attempt with actual OS XTest click only if normal entry still closed.
  time.sleep(2)
  if '3c078156'not in snap()['currentID']:
   move(a['x']+a['width']/2,a['y']+a['height']/2);xd('click',1);time.sleep(4)
  r['toolbarClickEvents']=js('return qaClicks')
  r['afterSideberyToolbar']=snap()
  r['extensionState']=js('let x=WebExtensionPolicy.getByID("{3c078156-979c-498b-8990-85f7987dd929}")?.extension;return {backgroundState:x?.backgroundState,hasShutdownPromise:!!x?.shutdownPromise,active:WebExtensionPolicy.getByID("{3c078156-979c-498b-8990-85f7987dd929}")?.active}')
  r['sideberySelectionMethod']='W3C trusted click of normal Sidebery toolbar action; subsequent pointer uses OS XTest'
  if '3c078156'not in snap()['currentID']:
   r['ordinarySideberySelectionQualified']=False
   if sys.argv[2]=='sidebery-row':raise RuntimeError('normal Sidebery entry failed; no API fallback permitted for row test')
   r['sideberyApiSetup']=m.execute_async_script('''const done=arguments[arguments.length-1];SidebarController.showInitially("_3c078156-979c-498b-8990-85f7987dd929_-sidebar-action").then(v=>done({result:v,isOpen:SidebarController.isOpen,id:SidebarController.currentID})).catch(e=>done({error:String(e)}));''',script_timeout=20000)
   r['sideberySelectionMethod']+='; explicit native showInitially panel-selection fixture required (not qualified as normal UI entry)'
   time.sleep(1)
   if '3c078156'not in snap()['currentID']:raise RuntimeError('Sidebery native selection fixture also failed')
  else:r['ordinarySideberySelectionQualified']=True
  t=snap()['trigger'];move(t['x']+t['width']/2,t['y']+t['height']/2);time.sleep(.3)
  if js('return document.getElementById("floorp-firefox-sidebar-edge-toggle").getAttribute("aria-pressed")')=='true':xd('click',1)
  b=snap()['box'];move(b['x']+100,b['y']+180);time.sleep(.7)
  r['sideberyOpen']=snap();(R/f'{port}-sidebery-open.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
  if sys.argv[2]=='sidebery-menu':sys.exit(0)
 js('''window.qaEvents=[];for(let type of ['TabSelect','mouseenter','mouseleave','blur','focus','focusin','focusout','activate','deactivate'])window.addEventListener(type,e=>{if(['mouseenter','mouseleave'].includes(type)&& !['sidebar-box','floorp-firefox-sidebar-edge-toggle','sidebar-container','webext-panels-browser'].includes(e.target.id))return;let b=document.getElementById('sidebar-box');qaEvents.push({time:performance.now(),type,target:e.target?.id||e.target?.localName||'window',related:e.relatedTarget?.id||e.relatedTarget?.localName||null,trusted:e.isTrusted,phase:e.eventPhase,bubbles:e.bubbles,sameWindow:e.target===window,boxHover:b.matches(':hover'),expanded:b.hasAttribute('data-floorp-sidebar-expanded'),opacity:getComputedStyle(b).opacity,active:document.activeElement?.id,selected:gBrowser.selectedTab.label})},true);new MutationObserver(rs=>{for(let a of rs){let b=document.getElementById('sidebar-box');qaEvents.push({time:performance.now(),type:'mutation',attribute:a.attributeName,target:a.target.id,opacity:getComputedStyle(b).opacity,hidden:b.hidden,expanded:b.hasAttribute('data-floorp-sidebar-expanded')})}}).observe(document.getElementById('sidebar-box'),{attributes:true});''')
 if sys.argv[2]=='sidebery-row':
  # Last of three actual painted rows: New Tab, QA A, QA B.
  # Text paint is blank in this setup; selection label must independently verify QA B.
  js('qaEvents.length=0');before=snap();b=before['box'];move(b['x']+110,b['y']+174);xd('click',1)
  samples=[]
  for i in range(6):time.sleep(.15);samples.append(snap())
  r['observations'].append({'case':'normal-sidebery-row-click','rowCoordinatesFromPaint':[b['x']+110,b['y']+174],'before':before,'samples':samples,'events':js('return qaEvents'),'qualifiedSelection':before['selected']=='QA A'and samples[-1]['selected']=='QA B'})
  (R/f'{port}-sidebery-after-click.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
  sys.exit(0)
 def open_body():
  # Clear focus using a real click in the webpage before the next hover.
  move(650,550);xd('click',1);time.sleep(.8)
  t=snap()['trigger'];move(t['x']+max(1,t['width']/2),t['y']+t['height']/2);time.sleep(.8)
  b=snap()['box'];move(b['x']+min(100,b['width']/2),b['y']+150);time.sleep(.6)
 for focused in [False,True]:
  open_body()
  if focused:xd('click',1);time.sleep(.3)
  js('qaEvents.length=0');before=snap();pointer=None
  xd('key','ctrl+Tab');samples=[]
  for i in range(6):time.sleep(.15);samples.append(snap())
  r['observations'].append({'case':'ctrl-tab-focused-body'if focused else'ctrl-tab-hover-only','before':before,'samples':samples,'events':js('return qaEvents')})
  (R/f'{port}-ctrl-{int(focused)}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
 open_body();js('qaEvents.length=0');before=snap();move(650,550);time.sleep(.8);r['observations'].append({'case':'pointer-leave','before':before,'after':snap(),'events':js('return qaEvents')})

 if sys.argv[2]=='cases-focus':
  open_body();js('qaEvents.length=0');before=snap();focusBefore=js('return document.hasFocus()');focuslog=open(R/f'logs/focus-control-{port}.log','w')
  other=subprocess.Popen(['/usr/bin/xmessage','-title','KAN30 focus control','-geometry','200x100+1100+980','QA focus control'],env=os.environ.copy(),stdout=focuslog,stderr=focuslog,start_new_session=True)
  try:
   for attempt in range(30):
    time.sleep(.2)
    found=subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','search','--onlyvisible','--name','^KAN30 focus control$'],env=toolenv,text=True,capture_output=True)
    if found.returncode==0 and found.stdout.strip():break
   else:raise RuntimeError('owned xmessage not visible after6s; desktop focus control unqualified')
   win=found.stdout.split()[0]
   xd('windowactivate'if os.environ.get('QA_WM')=='openbox'else'windowfocus','--sync',win);time.sleep(.8)
   focusAfter=js('return document.hasFocus()')
   ewmhAfter=subprocess.run(['xprop','-root','_NET_ACTIVE_WINDOW'],env=os.environ.copy(),text=True,capture_output=True).stdout
   r['observations'].append({'case':'real-native-window-focus-loss','fixture':'EWMH windowactivate under Openbox to owned mapped xmessage, stationary pointer'if os.environ.get('QA_WM')=='openbox'else'X11 windowfocus (no EWMH); not physical input','documentFocusedBefore':focusBefore,'documentFocusedAfter':focusAfter,'ewmhActiveAfter':ewmhAfter,'before':before,'after':snap(),'events':js('return qaEvents')})
   (R/f'{port}-window-focus-loss.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
  finally:
   other.terminate();other.wait(timeout=5)

except Exception as e:
 r.update(error=repr(e),traceback=traceback.format_exc())
 try:r['errorSnapshot']=snap();(R/f'{port}-error.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
 except Exception:pass
finally:
 (R/f'probe-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));
 try:m.delete_session()
 except Exception:pass
