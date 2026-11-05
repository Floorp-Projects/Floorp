import os,sys,time,json,subprocess,signal,socket,base64
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent; root=R.parents[1]; product=sys.argv[1];port=int(sys.argv[2]); display=':98';work=Path('/workspace/kan39-followup-run')/product;work.mkdir(parents=True,exist_ok=True)
env=os.environ.copy();env.update(DISPLAY=display,MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
profile=work/('profile-'+str(int(time.time())));profile.mkdir(exist_ok=True);(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\nuser_pref("floorp.tabstacks.enabled",true);\n')
processes=[];out={'product':product,'input':'OS XTest button4/5, one notch, synthetic not physical','cases':[]}
def launch(args,name):
 f=open(R/'logs'/name,'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);processes.append(p);return p
try:
 launch(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',display,'-screen','0','1600x1000x24','-nolisten','tcp'],'xvfb-'+product+'.log');time.sleep(1)
 p=launch(['dbus-run-session','--',str(root/'products'/product/product),'--no-remote','--profile',str(profile),'--marionette','about:blank'],product+'.log')
 for i in range(180):
  try:s=socket.create_connection(('localhost',port),.2);s.close();break
  except OSError:time.sleep(.5)
 m=Marionette('localhost',port=port,socket_timeout=90);m.start_session();m.set_context('chrome');js=m.execute_script
 out['environment']=js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS,dpr:devicePixelRatio,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level")}')
 js('window.qaTabs=Array.from({length:60},(_,i)=>gBrowser.addTab("about:blank",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));gBrowser.selectedTab=qaTabs[30];window.qaWheel=[];window.addEventListener("wheel",e=>{let r={deltaX:e.deltaX,deltaY:e.deltaY,mode:e.deltaMode,trusted:e.isTrusted,path:e.composedPath().slice(0,6).map(x=>x.id||x.localName)};qaWheel.push(r);setTimeout(()=>r.prevented=e.defaultPrevented,0)},true)');time.sleep(3)
 for kind in (['normal','stack'] if product=='floorp' else ['normal']):
  if kind=='stack':js('window.qaGroup=gBrowser.addTabGroup(qaTabs,{label:"QA wheel comparison"});gBrowser.selectedTab=qaTabs[30]');time.sleep(3)
  expr='document.querySelector("#floorp-stack-scroller")' if kind=='stack' else 'gBrowser.tabContainer.arrowScrollbox.scrollbox'
  measure='let e='+expr+';let r=e.getBoundingClientRect();return {left:e.scrollLeft,top:e.scrollTop,width:e.clientWidth,total:e.scrollWidth,x:r.x,y:r.y,height:r.height,screenX:mozInnerScreenX,screenY:mozInnerScreenY}'
  # Warm the upstream horizontal arrow-scrollbox axis filter with actual input.
  pre=js(measure)
  subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','mousemove','--sync',str(int(pre['screenX']+pre['x']+pre['width']/2)),str(int(pre['screenY']+pre['y']+pre['height']/2)),'click','--repeat','3','--delay','200','5'],env=env,check=True);time.sleep(1.2)
  out.setdefault('warmup',[]).append({'kind':kind,'events':js('return qaWheel')});js('qaWheel.length=0')
  for button in [4,5]:
   js('let e='+expr+';e.scrollTo({left:(e.scrollWidth-e.clientWidth)/2,behavior:"instant"})');time.sleep(1.2);before=js(measure);time.sleep(.3);stable=js(measure)
   js('qaWheel.length=0');x=int(before['screenX']+before['x']+before['width']/2);y=int(before['screenY']+before['y']+before['height']/2)
   subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','mousemove','--sync',str(x),str(y),'click',str(button)],env=env,check=True);time.sleep(1.2)
   after=js(measure);out['cases'].append({'kind':kind,'button':button,'before':before,'stable':stable,'after':after,'distance':after['left']-before['left'],'events':js('return qaWheel')})
   (R/f'{product}-{kind}-{button}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
 (R/(product+'.json')).write_text(json.dumps(out,indent=2));print(json.dumps(out))
finally:
 for p in reversed(processes):
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except (ProcessLookupError,subprocess.TimeoutExpired):pass
