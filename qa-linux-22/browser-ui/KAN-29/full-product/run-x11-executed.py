"""Prepared only until a reviewed actual package/native manifest is supplied.
python run.py --prepare actual-manifest.json
python run.py --execute actual-manifest.json
"""
import sys,os,json,hashlib,subprocess,time,socket,signal,traceback,zipfile
from pathlib import Path
R=Path(__file__).parent;mode=sys.argv[1];contractPath=Path(sys.argv[2]).resolve();c=json.loads(contractPath.read_text());root=Path(c['productRoot']);pack=Path(c['languagePack']['path']);port=int(c.get('port',30071));display=int(c.get('display',125));issues=[]
def digest(p):
 with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
for k in ['version','build','platformBuild','gre']:
 if not c.get('live',{}).get(k):issues.append('missing authoritative live '+k)
for k in ['floorp','libxul.so','omni.ja','browser/omni.ja']:
 if not c.get('payloadSha256',{}).get(k):issues.append('missing expected package digest '+k)
 elif not (root/k).is_file():issues.append('package file unavailable '+k)
 elif digest(root/k)!=c['payloadSha256'][k]:issues.append('package file digest mismatch '+k)
if c.get('live',{}).get('gre')!=str(root.resolve()):issues.append('expected live GreD must equal actual extracted root')
if not pack.is_file():issues.append('signed language pack unavailable')
elif digest(pack)!=c['languagePack'].get('sha256'):issues.append('signed language pack digest mismatch')
else:
 with zipfile.ZipFile(pack) as z:
  pm=json.loads(z.read('manifest.json'));compat=pm.get('browser_specific_settings',{}).get('gecko',{});major=c.get('live',{}).get('version','').split('.')[0]
  if major and compat.get('strict_min_version','').split('.')[0]!=major:issues.append('language pack minimum-major differs from actual native version')
  if major and compat.get('strict_max_version','').split('.')[0]!=major:issues.append('language pack maximum-major differs from actual native version')
  if not any(n.startswith('META-INF/') for n in z.namelist()):issues.append('language pack lacks signature assets (AddonManager must still verify)')
prepared={'mode':mode,'manifest':str(contractPath),'issues':issues,'ready':not issues,'browserStarted':False,'sourceSha':c.get('sourceSha'),'productRoot':str(root),'fixture':'Startup vertical/hover/panel prefs and native API side selection; actual pin/open/close/tab clicks. Signed locale installed normally then normal quit/restart. No dir fallback/protection override.'}
if mode=='--prepare':print(json.dumps(prepared,indent=2));sys.exit(0 if not issues else 2)
if mode not in ['--execute','--worker']:raise SystemExit('use --prepare or --execute')
if issues:print(json.dumps(prepared,indent=2));raise SystemExit(2)
if mode=='--execute':
 wrapper=subprocess.run(['dbus-run-session','--',sys.executable,str(Path(__file__).resolve()),'--worker',str(contractPath)]);(R/('outer-result-'+c['runId']+'.json')).write_text(json.dumps({'dbusWrapperExit':wrapper.returncode,'manifest':str(contractPath)},indent=2));raise SystemExit(wrapper.returncode)
sys.path.insert(0,'/workspace/qa-linux-22/browser-ui/python')
from marionette_driver.marionette import Marionette
runId=c['runId'];out=R/'runs'/runId;out.mkdir(parents=True,exist_ok=False);work=Path('/workspace/kan29-full-runtime')/runId;profile=Path(c['reuseProfile']) if c.get('reuseProfile') else work/'profile';profile.mkdir(parents=True,exist_ok=bool(c.get('reuseProfile')));runtime=work/'wayland';runtime.mkdir(parents=True);runtime.chmod(0o700)
def manifest():
 d={}
 for f in sorted(root.rglob('*')):
  key=str(f.relative_to(root))
  if f.is_symlink():d[key]={'type':'symlink','link':os.readlink(f)}
  elif f.is_file():d[key]={'type':'file','bytes':f.stat().st_size,'sha256':digest(f)}
 return d
before=manifest();(out/'product-before.json').write_text(json.dumps(before,indent=2));(out/'contract.json').write_text(json.dumps(c,indent=2));design=json.loads((R/'design-fixture.json').read_text());prefs={'marionette.port':port,'focusmanager.testmode':False,'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,'floorp.tabstacks.enabled':True,'browser.tabs.splitView.enabled':True,'sidebar.revamp':True,'sidebar.verticalTabs':True,'sidebar.visibility':'expand-on-hover','floorp.design.configs':json.dumps(design),'floorp.panelSidebar.config':json.dumps({'globalWidth':400,'autoUnload':False,'position_start':True,'displayed':True,'webExtensionRunningEnabled':False,'overlay':False,'openOnHover':False})};(profile/'user.js').write_text(''.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');\n' for k,v in prefs.items()));(out/'startup-prefs.json').write_text(json.dumps(prefs,indent=2))
qa=Path('/workspace/qa-linux-22/browser-ui');env=os.environ.copy();env.update(XDG_CURRENT_DESKTOP='KDE',XDG_SESSION_TYPE='wayland',PYTHONPATH='/workspace/qa-linux-22/browser-ui/python',DISPLAY=':'+str(display),XDG_RUNTIME_DIR=str(runtime),XDG_CACHE_HOME=str(work/'cache'),XDG_CONFIG_HOME=str(work/'config'),XDG_DATA_DIRS=str(qa/'sysroot/usr/share')+':/usr/local/share:/usr/share',LD_LIBRARY_PATH=str(qa/'sysroot/usr/lib/x86_64-linux-gnu')+':'+str(qa/'sysroot/usr/lib/x86_64-linux-gnu/weston'),QT_PLUGIN_PATH=str(qa/'sysroot/usr/lib/x86_64-linux-gnu/qt6/plugins')+':/usr/lib/x86_64-linux-gnu/qt6/plugins',WAYLAND_DISPLAY='qa-kan29-full-'+str(port),MOZ_ENABLE_WAYLAND='1',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',QA29_OUTPUT=str(out),QA29_MANIFEST=str(contractPath));assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
if c.get('backend')=='x11':
 env.update(XDG_CURRENT_DESKTOP='openbox',XDG_SESSION_TYPE='x11',MOZ_ENABLE_WAYLAND='0',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');env.pop('WAYLAND_DISPLAY',None)
procs=[];browser=None;result={'sourceSha':c['sourceSha'],'profile':str(profile),'phases':[],'normalExitRequired':True,'runnerPid':os.getpid(),'exitSemantics':'actualFloorpChildExit is direct Popen.wait on executable child inside DBus; dbusWrapperExit and runnerExit are separate. No child status inferred from wrapper.'}
from resource_monitor import Monitor,ResourceBoundExceeded
def on_resource_bound(signum,frame):raise ResourceBoundExceeded('resource bound reached; no product PASS')
signal.signal(signal.SIGUSR1,on_resource_bound)
monitor=Monitor(out/'resources.jsonl',lambda:browser.pid if browser and browser.poll() is None else None,lambda:[p.pid for kind,p in procs if kind!='browser']);monitor.start()
def start(stage):
 global browser
 browser=subprocess.Popen([str(root/'floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=open(out/(stage+'-browser.log'),'w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('browser',browser));deadline=time.monotonic()+45
 while time.monotonic()<deadline:
  if browser.poll() is not None:raise RuntimeError('early browser exit '+str(browser.returncode))
  try:s=socket.create_connection(('127.0.0.1',port),.2);s.close();break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('Marionette socket timeout')
 row={'stage':stage,'actualFloorpChildPid':browser.pid,'actualFloorpExecutable':os.readlink('/proc/'+str(browser.pid)+'/exe'),'normalQuitRequested':False,'sessionCapabilities':{'pageLoadStrategy':'none'},'sessionTimeoutSeconds':90};result['phases'].append(row);(out/'ownership.json').write_text(json.dumps(result,indent=2));m=Marionette('localhost',port=port,socket_timeout=90);m.start_session(capabilities={'pageLoadStrategy':'none'});m.set_context('chrome');actual=m.execute_script('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,platformBuild:Services.appinfo.platformBuildID,gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,signature:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),langpackSignature:Services.prefs.getBoolPref("extensions.langpacks.signatures.required"),langpackSignatureUserSet:Services.prefs.prefHasUserValue("extensions.langpacks.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode")}');assert all(actual[k]==v for k,v in c['live'].items()),(actual,c['live']);row['live']=actual;(out/'ownership.json').write_text(json.dumps(result,indent=2));return m,row
def quit(m,row):
 m.execute_script('Services.prefs.savePrefFile(null);Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true');row['normalQuitRequested']=True;row['actualFloorpChildExit']=browser.wait(timeout=40);row['exitMeasurement']='direct Popen.wait on actual executable child, not DBus wrapper';(out/(row['stage']+'-floorp-child-exit.json')).write_text(json.dumps({'actualFloorpChildPid':browser.pid,'actualFloorpExecutable':row['actualFloorpExecutable'],'actualFloorpChildExit':row['actualFloorpChildExit'],'normalQuitRequested':True,'measurement':row['exitMeasurement']},indent=2));assert row['actualFloorpChildExit']==0
 try:m.client.close()
 except Exception:pass
try:
 assert not Path('/tmp/.X11-unix/X'+str(display)).exists(),'private display occupied'
 xv=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',':'+str(display),'-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=open(out/'xvfb.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('xvfb',xv));time.sleep(1);assert xv.poll() is None
 if c.get('backend')=='x11':
  we=env.copy();we['LD_LIBRARY_PATH']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu';we['XDG_DATA_DIRS']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share'
  wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable'],env=we,stdout=open(out/'openbox.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('openbox',wm));time.sleep(1);assert wm.poll() is None
 else:
  kw=subprocess.Popen([str(qa/'sysroot/usr/bin/kwin_wayland'),'--x11-display',':'+str(display),'--width','1366','--height','1000','--socket',env['WAYLAND_DISPLAY']],env=env,stdout=open(out/'kwin.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('kwin',kw));deadline=time.monotonic()+15
  while not (runtime/env['WAYLAND_DISPLAY']).exists():
   assert kw.poll() is None,'KWin exited'
   if time.monotonic()>deadline:raise RuntimeError('private Wayland socket timeout')
   time.sleep(.2)
 # LTR and RTL are separate native browser launches on the same controlled profile.
 if 'ltr' in c.get('stages',['ltr','locale-install','rtl']):
  m,row=start('ltr');locale=m.execute_script('return {app:Services.locale.appLocalesAsBCP47,rtl:Services.locale.isAppLocaleRTL,RTL_UI:window.RTL_UI,dir:getComputedStyle(document.documentElement).direction}');row['locale']=locale;assert not locale['rtl'] and not locale['RTL_UI'] and locale['dir']=='ltr';m.delete_session();env['QA_DIR']='ltr';env['QA29_SINGLE_STATE']='0,1,0' if c.get('singleLtrState') else '';measured=subprocess.run([sys.executable,str(R/'measure.py'),'panel',str(port)],env=env,timeout=240,stdout=open(out/'ltr-measure.log','w'),stderr=subprocess.STDOUT);row['probeExit']=measured.returncode;row['probeKind']='separate measure.py process';assessment=json.loads((out/('measure-'+env['QA_DIR']+'-'+str(port)+'.json')).read_text());row['collectionComplete']=assessment.get('collectionComplete',False);row['productPass']=assessment.get('productPass',False);row['hoverRoundsCollected']=assessment.get('hoverRoundsCollected',0);m=Marionette('localhost',port=port,socket_timeout=45);m.start_session(capabilities={'pageLoadStrategy':'none'});m.set_context('chrome');quit(m,row);assert measured.returncode==0
 if 'locale-install' in c.get('stages',['ltr','locale-install','rtl']):
  m,row=start('locale-install');row['install']=m.execute_async_script('''const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const {FileUtils}=ChromeUtils.importESModule('resource://gre/modules/FileUtils.sys.mjs');(async()=>{let i=await AddonManager.getInstallForFile(new FileUtils.File(arguments[0]));await i.install();let a=i.addon;if(a.signedState<=0||a.appDisabled)throw Error('unsigned/incompatible locale');Services.locale.requestedLocales=['ar'];Services.prefs.savePrefFile(null);done({id:a.id,version:a.version,signedState:a.signedState,appDisabled:a.appDisabled});})().catch(e=>done({error:String(e)}));''',script_args=[str(pack)],script_timeout=60000);quit(m,row);assert not row['install'].get('error'),row
 m,row=start('rtl');row['locale']=m.execute_script('return {app:Services.locale.appLocalesAsBCP47,rtl:Services.locale.isAppLocaleRTL,RTL_UI:window.RTL_UI,dir:getComputedStyle(document.documentElement).direction}');assert row['locale']['rtl'] and row['locale']['RTL_UI'] and row['locale']['dir']=='rtl';m.delete_session();env.update(QA_DIR='rtl',QA_REQUIRE_RTL='1',QA29_SINGLE_STATE='');measured=subprocess.run([sys.executable,str(R/'measure.py'),'panel',str(port)],env=env,timeout=240,stdout=open(out/'rtl-measure.log','w'),stderr=subprocess.STDOUT);row['probeExit']=measured.returncode;row['probeKind']='separate measure.py process';assessment=json.loads((out/('measure-'+env['QA_DIR']+'-'+str(port)+'.json')).read_text());row['collectionComplete']=assessment.get('collectionComplete',False);row['productPass']=assessment.get('productPass',False);row['hoverRoundsCollected']=assessment.get('hoverRoundsCollected',0);m=Marionette('localhost',port=port,socket_timeout=45);m.start_session(capabilities={'pageLoadStrategy':'none'});m.set_context('chrome');quit(m,row);assert measured.returncode==0;result['collectionComplete']=True;result['productPass']=all(x.get('productPass',False) for x in result['phases'] if x['stage'] in ['ltr','rtl'])
except Exception as e:result['error']=repr(e);result['traceback']=traceback.format_exc()
finally:
 if monitor.exceeded:
  result['resourceBound']=monitor.exceeded;result['productPass']=False;result['resourceClassification']='QA safety bound reached; not proof of original host inadequacy/product cause, never PASS'
  if browser and browser.poll() is None:
   result['resourceBoundNormalUiQuitAttempt']='OS CtrlQ on private nested display; no claim of normal quit until direct child wait observed'
   try:
    uiEnv=env.copy();uiEnv['LD_LIBRARY_PATH']='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu';uiQuit=subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','key','ctrl+q'],env=uiEnv,timeout=4,capture_output=True,text=True);result['resourceBoundUiCommand']={'exit':uiQuit.returncode,'stderr':uiQuit.stderr};uiQuit.check_returncode();result['resourceBoundUiChildExit']=browser.wait(timeout=8)
   except Exception as e:result['resourceBoundUiQuitError']=repr(e)
 if browser and browser.poll() is None and not monitor.exceeded:
  try:
   m=Marionette('localhost',port=port,socket_timeout=10,startup_timeout=10);m.start_session();m.set_context('chrome');m.execute_script('Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true');result['recoveryActualFloorpChildExit']=browser.wait(timeout=20);result['recoveryActualFloorpChildPid']=browser.pid
  except Exception as e:result['recoveryError']=repr(e)
 for kind,p in reversed(procs):
  if p.poll() is None:
   if kind=='browser':result['abnormalProductCleanup']=True
   try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=8)
   except (ProcessLookupError,subprocess.TimeoutExpired):pass
 for phase in result['phases']:
  child=next((p for kind,p in procs if kind=='browser' and p.pid==phase['actualFloorpChildPid']),None)
  if child is not None:
   try:
    phase['actualFloorpChildExit']=child.wait(timeout=1);(out/(phase['stage']+'-floorp-child-exit.json')).write_text(json.dumps({'actualFloorpChildPid':child.pid,'actualFloorpExecutable':phase['actualFloorpExecutable'],'actualFloorpChildExit':phase['actualFloorpChildExit'],'normalQuitRequested':phase.get('normalQuitRequested',False),'measurement':'direct Popen.wait, including failed startup; abnormal cleanup separately recorded'},indent=2))
   except subprocess.TimeoutExpired:phase['actualFloorpChildExit']=None
 monitor.stop()
 after=manifest();(out/'product-after.json').write_text(json.dumps(after,indent=2));result['productUnmodified']=before==after;result['changedEntries']=[k for k in before if before[k]!=after.get(k)];result['addedEntries']=[k for k in after if k not in before];result['runnerExit']=1 if result.get('error') or not result['productUnmodified'] else 0;(out/'runner-result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if result.get('error') or not result['productUnmodified']:raise SystemExit(1)
