"""Prepared single experiment: fresh normal locale bootstrap then one bounded Arabic capture.
Raw profiler/memory reports are PRIVATE. No automatic raw upload/publication.
"""
import os,sys,json,time,signal,subprocess,threading,hashlib,shutil,traceback
from pathlib import Path
R=Path(__file__).resolve().parent;W=Path('/workspace/kan29-memory-capture');P=W/'profile';PRODUCT=Path('/workspace/kan29-package/baseline/floorp');BASE=Path('/workspace/qa-linux-22/browser-ui/KAN-29/full-product');PORT=30120;DISPLAY=':129';REF='c74fe1a545d275cdd710ab44701c1ee0dd6b0eae'
def save(n,d):(R/n).write_text(json.dumps(d,indent=2)+'\n')
def sha(f):
 with f.open('rb')as s:return hashlib.file_digest(s,'sha256').hexdigest()
def tree(root):return {str(f.relative_to(root)):({'link':os.readlink(f)}if f.is_symlink()else{'bytes':f.stat().st_size,'sha256':sha(f)})for f in sorted(root.rglob('*'))if f.is_file()or f.is_symlink()}
contract=json.loads((BASE/'runs/baseline-a629-x11-one-30076/contract.json').read_text());prefs=json.loads((BASE/'runs/candidate-f43-x11-30075/startup-prefs.json').read_text());prefs['marionette.port']=PORT
allowed={'marionette.port','focusmanager.testmode','browser.shell.checkDefaultBrowser','browser.aboutwelcome.enabled','floorp.tabstacks.enabled','browser.tabs.splitView.enabled','sidebar.revamp','sidebar.verticalTabs','sidebar.visibility','floorp.design.configs','floorp.panelSidebar.config'};assert set(prefs)<=allowed
profenv={'MOZ_PROFILER_STARTUP':'1','MOZ_PROFILER_STARTUP_ENTRIES':'32MiB','MOZ_PROFILER_STARTUP_INTERVAL':'5','MOZ_PROFILER_STARTUP_FILTERS':'GeckoMain','MOZ_PROFILER_STARTUP_FEATURES':'js,stackwalk,cpu,memory','MOZ_UPLOAD_DIR':str(W/'private-captures'),'MOZ_PROFILER_SHUTDOWN':str(W/'private-captures/shutdown.json'),'TMPDIR':str(W/'private-captures/tmp')}
if '--execute'not in sys.argv:
 save('prepared.json',{'preparedOnly':True,'browserStarted':False,'product':str(PRODUCT),'sourceSha':contract['sourceSha'],'nativeVersionExpected':'157.0.1','buildExpected':'20261007125927','port':PORT,'display':DISPLAY,'profileFresh':True,'noOldProfileCopy':True,'startupPrefs':prefs,'profilingEnvironment':profenv,'captureLimitSeconds':20,'rssGuardGiB':3,'cgroupGuardGiB':6,'rawCapturesPrivate':True,'gate':'QA_KAN29_CAPTURE_START=1 --execute','captureRTLPass':False});print('PREPARED ONLY; no browser started');sys.exit()
assert os.environ.get('QA_KAN29_CAPTURE_START')=='1','root execute gate required'
if '--worker'not in sys.argv:raise SystemExit(subprocess.run(['dbus-run-session','--',sys.executable,str(Path(__file__).resolve()),'--execute','--worker']).returncode)
assert not W.exists(),'single-run work root already exists'
assert not Path('/tmp/.X11-unix/X129').exists(),'private display occupied'
for key in ['floorp','libxul.so','omni.ja','browser/omni.ja']:assert sha(PRODUCT/key)==contract['payloadSha256'][key]
pack=Path(contract['languagePack']['path']);assert sha(pack)==contract['languagePack']['sha256']
W.mkdir();W.chmod(0o700);P.mkdir();private=W/'private-captures';private.mkdir(mode=0o700);(private/'tmp').mkdir(mode=0o700);rt=W/'runtime';rt.mkdir(mode=0o700)
(P/'user.js').write_text(''.join('user_pref('+json.dumps(k)+', '+json.dumps(v)+');\n'for k,v in prefs.items()))
env=os.environ.copy();env.update(DISPLAY=DISPLAY,XDG_CURRENT_DESKTOP='openbox',XDG_SESSION_TYPE='x11',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',XDG_RUNTIME_DIR=str(rt),LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');env.pop('WAYLAND_DISPLAY',None);assert 'MOZ_DISABLE_CONTENT_SANDBOX'not in env
procs=[];browser=None;stop=threading.Event();pssStop=threading.Event();latest=[];cache={};events=[];bound=threading.Event();phase='setup';latestRow={};result={'sourceSha':contract['sourceSha'],'runtime':REF,'nativeVersionExpected':'157.0.1','buildExpected':'20261007125927','captureRTLPass':False,'freshProfile':True,'noMigrationOrRestoredUserPages':True,'profilingEnvironment':profenv,'phases':[],'runnerPid':os.getpid(),'rawCaptureDirectory':str(private),'rawCapturePrivacy':'private; only sanitized aggregates/digests may publish'}
class LowResourceBound(RuntimeError):pass
def interrupt_guard(signum,frame):raise LowResourceBound('fast0.2s resource guard interrupted current operation')
signal.signal(signal.SIGUSR1,interrupt_guard)
lock=threading.Lock()
def write(row):
 with lock:
  with (R/'resources-private.jsonl').open('a')as f:f.write(json.dumps(row)+'\n')
def sample():
 global latest,latestRow
 rows={}
 for p in Path('/proc').iterdir():
  if not p.name.isdigit():continue
  try:
   st=(p/'stat').read_text();parts=st[st.rfind(')')+2:].split();status=dict(l.split(':',1)for l in(p/'status').read_text().splitlines()if':'in l);rows[int(p.name)]={'pid':int(p.name),'ppid':int(parts[1]),'rssKiB':int(status.get('VmRSS','0 kB').split()[0]),'cpuTicks':int(parts[11])+int(parts[12])}
  except(OSError,ValueError):pass
 ids={browser.pid}if browser and browser.poll()is None else set()
 while True:
  added={p for p,x in rows.items()if x['ppid']in ids}-ids
  if not added:break
  ids|=added
 latest=list(ids);t=time.monotonic();owned=[]
 for p in ids:
  if p in rows:owned.append(dict(rows[p],pss=cache.get(p)))
 cg=Path('/sys/fs/cgroup');d={'t':t,'phase':phase,'actualChildPid':browser.pid if browser else None,'owned':owned,'rssKiB':sum(x['rssKiB']for x in owned),'cgroupBytes':int((cg/'memory.current').read_text()),'memoryEvents':dict(l.split()for l in(cg/'memory.events').read_text().splitlines()),'cpuTicksPerSecond':os.sysconf('SC_CLK_TCK')};latestRow=d;return d
def fast():
 while not stop.is_set():
  start=time.monotonic()
  try:
   d=sample();d['samplerDuration']=time.monotonic()-start;write(d)
   if (d['rssKiB']>=3*1024**2 or d['cgroupBytes']>=6*1024**3) and not bound.is_set():
    bound.set();os.kill(os.getpid(),signal.SIGUSR1)
  except Exception as e:write({'sampleError':repr(e)})
  stop.wait(.2)
def slow():
 while not pssStop.is_set():
  for pid in list(latest):
   begin=time.monotonic()
   try:
    v=dict(l.split(':',1)for l in(Path('/proc')/str(pid)/'smaps_rollup').read_text().splitlines()if':'in l);cache[pid]={'KiB':int(v['Pss'].split()[0]),'started':begin,'completed':time.monotonic()}
   except(OSError,ValueError):pass
  pssStop.wait(1)
def check():
 if bound.is_set():raise RuntimeError('low resource bound reached')
def start(stage,extra=None):
 global browser,phase
 phase=stage;ee=env.copy();ee.update(extra or{});browser=subprocess.Popen([str(PRODUCT/'floorp'),'--no-remote','--profile',str(P),'--marionette','about:blank'],env=ee,stdout=open(private/(stage+'-browser.log'),'w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('browser',browser));row={'stage':stage,'pid':browser.pid,'actualExe':os.readlink('/proc/'+str(browser.pid)+'/exe'),'actualExeSha256':sha(Path('/proc')/str(browser.pid)/'exe'),'began':time.monotonic()};result['phases'].append(row);save('ownership.json',result);return row
def normal(row):
 q=subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','key','ctrl+q'],env=env,timeout=3,capture_output=True);row['osCtrlQExit']=q.returncode
 try:row['actualChildExit']=browser.wait(timeout=5);row['normalExit']=row['actualChildExit']==0
 except subprocess.TimeoutExpired:row['normalExit']=False;row['quitTimedOut']=True
 return row.get('normalExit',False)
def caught(sig):
 try:
  status=dict(l.split(':',1)for l in(Path('/proc')/str(browser.pid)/'status').read_text().splitlines()if':'in l);mask=int(status['SigCgt'].strip(),16);return bool(mask&(1<<(sig-1))),hex(mask)
 except(OSError,KeyError):return False,None
def send(sig,label):
 yes,mask=caught(sig);e={'t':time.monotonic(),'signal':int(sig),'label':label,'caughtMask':mask,'registered':yes,'sent':False}
 if yes and browser.poll()is None:os.kill(browser.pid,sig);e['sent']=True
 events.append(e);save('signal-events.json',events)
before=tree(PRODUCT);save('product-before.json',before);threads=[threading.Thread(target=fast,daemon=True),threading.Thread(target=slow,daemon=True)]
try:
 xv=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',DISPLAY,'-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=open(private/'xvfb.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('xvfb',xv));time.sleep(1);assert xv.poll()is None
 we=env.copy();we.update(LD_LIBRARY_PATH='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu',XDG_DATA_DIRS='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share');wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable'],env=we,stdout=open(private/'openbox.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('openbox',wm));time.sleep(1);assert wm.poll()is None
 for t in threads:t.start()
 row=start('english-locale-bootstrap');sys.path.insert(0,'/workspace/qa-linux-22/browser-ui/python');from marionette_driver.marionette import Marionette
 m=Marionette('localhost',port=PORT,socket_timeout=15,startup_timeout=30);m.start_session(capabilities={'pageLoadStrategy':'none'});m.set_context('chrome');check()
 actual=m.execute_script('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,app:Services.locale.appLocalesAsBCP47,rtl:Services.locale.isAppLocaleRTL,signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),signatureUserSet:Services.prefs.prefHasUserValue("xpinstall.signatures.required"),langpackSignatureRequired:Services.prefs.getBoolPref("extensions.langpacks.signatures.required"),langpackSignatureUserSet:Services.prefs.prefHasUserValue("extensions.langpacks.signatures.required"),sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),focusTestMode:Services.prefs.getBoolPref("focusmanager.testmode")}');row['live']=actual;row['bootstrapContext']=m.execute_script('return {tabs:gBrowser.tabs.map(t=>({uri:t.linkedBrowser.currentURI.spec,remote:t.linkedBrowser.isRemoteBrowser,remoteType:t.linkedBrowser.remoteType,systemPrincipal:t.linkedBrowser.contentPrincipal?.isSystemPrincipal})),startupPage:Services.prefs.getIntPref("browser.startup.page",-1),resumeOnce:Services.prefs.getBoolPref("browser.sessionstore.resume_session_once",false)}');assert actual['version']=='157.0.1'and actual['build']=='20261007125927'and actual['gre']==str(PRODUCT)and not actual['rtl']
 row['localeInstall']=m.execute_async_script('''const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const {FileUtils}=ChromeUtils.importESModule('resource://gre/modules/FileUtils.sys.mjs');(async()=>{let i=await AddonManager.getInstallForFile(new FileUtils.File(arguments[0]));await i.install();let a=i.addon;if(a.signedState<=0||a.appDisabled)throw Error('unsigned/incompatible');Services.locale.requestedLocales=['ar'];Services.prefs.savePrefFile(null);done({id:a.id,version:a.version,signedState:a.signedState,appDisabled:a.appDisabled});})().catch(e=>done({error:String(e)}));''',script_args=[str(pack)],script_timeout=20000);check();assert not row['localeInstall'].get('error')
 m.execute_script('Services.prefs.savePrefFile(null);Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true')
 row['normalQuitRequested']=True;row['quitMethod']='normal Services.startup.quit(eAttemptQuit) during connected English bootstrap';row['actualChildExit']=browser.wait(timeout=15);row['normalExit']=row['actualChildExit']==0;assert row['normalExit'],'bootstrap must normally exit0 before capture'
 try:m.client.close()
 except Exception:pass
 # Snapshot only hashes; do not read session contents. No imported old profile.
 snapshot=tree(P);save('clean-preRTL-profile-hashes-private.json',snapshot);shutil.copytree(P,W/'clean-preRTL-profile');result['cleanPreRTLSnapshot']={'nativeChildExit':row['actualChildExit'],'entryCount':len(snapshot),'snapshotSha256':hashlib.sha256(json.dumps(snapshot,sort_keys=True).encode()).hexdigest(),'normalQuitBeforeFirstArabic':True};check()
 row=start('sole-arabic-capture',profenv);started=row['began'];sent3=sent7=dumped=False;profileStable={};completed=[]
 # No Marionette connection or RPC in the Arabic capture phase.
 while browser.poll()is None:
  elapsed=time.monotonic()-started
  if elapsed>=3 and not sent3:send(signal.SIGRTMIN,'nonminimizing-memory-report-3s');sent3=True
  if elapsed>=7 and not sent7:send(signal.SIGRTMIN,'nonminimizing-memory-report-7s');sent7=True
  if not dumped and(elapsed>=8 or latestRow.get('rssKiB',0)>=1.0*1024**2):send(signal.SIGUSR2,'async-profiler-dump-and-stop');dumped=True
  for f in private.glob('profile_*.json'):
   stat=f.stat();prev=profileStable.get(f.name);profileStable[f.name]={'bytes':stat.st_size,'observed':time.monotonic(),'stableForSeconds':(time.monotonic()-prev['unchangedSince'])if prev and prev['bytes']==stat.st_size else 0,'unchangedSince':prev['unchangedSince']if prev and prev['bytes']==stat.st_size else time.monotonic()}
  row['boundedProfileCompletionObservation']=profileStable
  if bound.is_set()or elapsed>=20:break
  time.sleep(.1)
 row['captureSeconds']=time.monotonic()-started;row['boundReached']=bound.is_set();row['lastFastSample']=latestRow
 # At resource guard stop immediately; do not let normal-quit grace extend memory growth.
 if not bound.is_set():normal(row)
 result['captureSessionCreated']=False;result['strictRTL_UI']='NOT MEASURED';result['signals']=events
except Exception as e:result['error']=repr(e);result['tracebackPrivate']=traceback.format_exc()
finally:
 if browser and browser.poll()is None and not bound.is_set() and 'osCtrlQExit' not in result['phases'][-1]:
  try:normal(result['phases'][-1])
  except Exception as e:result['recoveryUiError']=repr(e)
 for kind,p in reversed(procs):
  if p.poll()is None:
   if kind=='browser':result['abnormalCleanup']=True
   try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=3)
   except(ProcessLookupError,subprocess.TimeoutExpired):
    try:os.killpg(p.pid,signal.SIGKILL);p.wait(timeout=2)
    except(ProcessLookupError,subprocess.TimeoutExpired):pass
 for row in result['phases']:
  proc=next((p for k,p in procs if k=='browser'and p.pid==row['pid']),None);row['actualChildExit']=proc.poll()if proc else None
 stop.set();pssStop.set()
 for t in threads:
  if t.is_alive():t.join(timeout=1)
 result['finalSample']=sample();after=tree(PRODUCT);save('product-after.json',after);result['fullPayloadUnchanged']=before==after
 # Bounded validation after child termination, not an extra browser or capture.
 inventory=[]
 for f in private.rglob('*'):
  if not f.is_file():continue
  x={'name':str(f.relative_to(private)),'bytes':f.stat().st_size,'sha256':sha(f),'private':True}
  if f.suffix=='.json':
   try:
    with f.open()as stream:j=json.load(stream)
    x['jsonComplete']=True;x['topLevelKeys']=list(j)[:12]if isinstance(j,dict)else[]
   except Exception:x['jsonComplete']=False
  inventory.append(x)
 result['captureInventory']=inventory;result['pssCaveat']='slow PSS reads separate thread, timestamps/age in fast rows; missing or delayed readings are not zero';save('result-private.json',result)
 print(json.dumps({'finished':True,'captureRTLPass':False,'error':result.get('error'),'productUnmodified':result['fullPayloadUnchanged'],'phases':[{k:r.get(k)for k in ['stage','pid','actualChildExit','normalExit','boundReached','captureSeconds']}for r in result['phases']]}))
