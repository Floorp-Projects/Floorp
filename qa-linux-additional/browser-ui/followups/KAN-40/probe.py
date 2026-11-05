import os,sys,time,json,subprocess,base64,traceback,collections
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(os.environ['QA40_OUTPUT']);stage=sys.argv[2];m=Marionette('localhost',port=int(sys.argv[1]),socket_timeout=30);m.start_session();out={'stage':stage,'input':'OS XTest synthetic middle button2; not physical input','observations':[]}
def js(s,args=None):m.set_context('chrome');return m.execute_script(s,script_args=args or [])
def content(s):m.set_context('content');m.switch_to_frame(None);return m.execute_script(s)
def xd(*args):subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],check=True,timeout=12)
def shot(name):
 from PIL import ImageGrab
 ImageGrab.grab(xdisplay=os.environ['DISPLAY']).save(R/(stage+'-'+name+'.png'))
def state():return js('return [...gBrowser.tabs].map(t=>({id:t.id,url:t.linkedBrowser.currentURI.spec,selected:t.selected,group:t.group?.id||null}))')
def rect(expression):return js('let e='+expression+';if(!e)throw Error("target absent");let r=e.getBoundingClientRect(),hit=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);return {x:mozInnerScreenX+r.x+r.width/2,y:mozInnerScreenY+r.y+r.height/2,width:r.width,height:r.height,outer:e.outerHTML,hit:hit?.outerHTML,focused:document.hasFocus(),nativeScreen:{x:mozInnerScreenX,y:mozInnerScreenY}}')
try:
 out['metadata']=js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,platformBuild:Services.appinfo.platformBuildID,gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode")}')
 assert out['metadata']['gre']=='/workspace/kan97-runtime/full-package/floorp';assert out['metadata']['build']=='20261103153232'
 if stage=='setup':
  m.set_context('content');m.navigate('about:hub#/features/design');deadline=time.monotonic()+15
  while time.monotonic()<deadline:
   found=content('return !!document.querySelector("#enable-tab-stacks")')
   if found:break
   time.sleep(.3)
  out['settingBefore']=content('let e=document.querySelector("#enable-tab-stacks");e.scrollIntoView({block:"center"});let r=e.getBoundingClientRect();return {checked:e.checked,disabled:e.disabled,x:r.x+r.width/2,y:r.y+r.height/2,html:e.outerHTML}')
  e=out['settingBefore'];assert not e['disabled']
  if not e['checked']:
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'settings','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':int(e['x']),'y':int(e['y']),'duration':0},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]});time.sleep(.7)
  out['settingEnabled']=js('return Services.prefs.getBoolPref("floorp.tabstacks.enabled")');assert out['settingEnabled'];shot('enabled-needs-restart')
 else:
  assert js('return Services.prefs.getBoolPref("floorp.tabstacks.enabled")')
  js('window.qa40Events=[];for(let n of ["mousedown","mouseup","auxclick","click"])window.addEventListener(n,e=>{if(e.button!==1)return;let row={type:e.type,button:e.button,trusted:e.isTrusted,target:e.target.id||e.target.className,composedPath:e.composedPath().slice(0,5).map(x=>x.id||x.className||x.nodeName),screenX:e.screenX,screenY:e.screenY};qa40Events.push(row);setTimeout(()=>row.prevented=e.defaultPrevented,0)},true);window.qa40Tabs=Array.from({length:5},(_,i)=>gBrowser.addTab("data:text/html,<title>QA40 synthetic "+i+"</title>QA40 "+i,{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));window.qa40Group=gBrowser.addTabGroup(qa40Tabs.slice(2),{label:"QA40 whole stack"});gBrowser.selectedTab=qa40Tabs[0]');time.sleep(1.2)
  out['fixture']='Real tabs and a native tab group constructed with gBrowser.addTab/addTabGroup for controlled URLs. Stack feature enabled through real settings UI then normal restart. No close handler or event replaced.'
  for kind,expression in ([('whole-stack-chip','qa40Group.querySelector(".tab-group-label-container")')] if os.environ.get('QA40_CHIP_ONLY')=='1' else [('ordinary','qa40Tabs[0]'),('whole-stack-chip','qa40Group.querySelector(".tab-group-label-container")')]):
   if kind=='whole-stack-chip':
    # Activate via actual OS primary click on chip so the visible stack has real members.
    p=rect(expression);xd('mousemove',int(p['x']),int(p['y']),'click',1);time.sleep(.4)
   p=rect(expression);assert p['width']>5 and p['height']>5 and p['focused'];before=state();shot(kind+'-before');js('qa40Events=[]')
   xd('mousemove',int(p['x']),int(p['y']));out['mouseBeforeMiddle']=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','getmouselocation','--shell'],text=True);out['targetImmediatelyBeforeMiddle']=rect(expression);xd('click',2);time.sleep(1)
   after=state();events=js('return qa40Events');closed=[url for (url,group) in (collections.Counter((t['url'],t['group']) for t in before)-collections.Counter((t['url'],t['group']) for t in after)).elements()];expected=[t['url'] for t in before if (t['url']=='data:text/html,<title>QA40 synthetic 0</title>QA40 0' if kind=='ordinary' else t['group']==js('return qa40Group.id'))]
   row={'kind':kind,'clickTarget':p,'before':before,'after':after,'closedUrls':closed,'expectedFromOriginalReport':expected,'remainingCount':len(after),'events':events,'trustedMiddleObserved':any(e['trusted'] and e['button']==1 for e in events),'expectedClosedSetMatches':collections.Counter(closed)==collections.Counter(expected)};out['observations'].append(row);shot(kind+'-after');(R/(stage+'.json')).write_text(json.dumps(out,indent=2));print(kind,'closed',closed,'expected',expected,flush=True)
  out['allExpected']=all(x['expectedClosedSetMatches'] and x['trustedMiddleObserved'] for x in out['observations'])
except Exception as e:out['error']=repr(e);out['traceback']=traceback.format_exc();traceback.print_exc()
finally:
 try:out['normalQuitRequested']=js('Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true')
 except Exception as e:out['quitError']=repr(e)
 (R/(stage+'.json')).write_text(json.dumps(out,indent=2))
 try:m.client.close()
 except Exception:pass
if out.get('error'):sys.exit(1)
