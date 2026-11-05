"""Single first-Arabic early signal capture after fresh normal English setup. Raw data private."""
import os,sys,time,json,subprocess,threading,signal,hashlib,gzip,traceback
from pathlib import Path
R=Path(__file__).resolve().parent;W=Path('/workspace/kan29-arabic-early');B=Path('/workspace/kan29-package/baseline/floorp');PORT=30131;DISPLAY=':131'
def save(n,v):(R/n).write_text(json.dumps(v,indent=2)+'\n')
def sha(p):
 with p.open('rb')as f:return hashlib.file_digest(f,'sha256').hexdigest()
if '--worker'not in sys.argv:raise SystemExit(subprocess.run(['dbus-run-session','--',sys.executable,str(Path(__file__).resolve()),'--worker']).returncode)
assert not W.exists();assert not Path('/tmp/.X11-unix/X131').exists()
W.mkdir(mode=0o700);P=W/'fresh-profile';P.mkdir();O=W/'private';O.mkdir(mode=0o700);rt=W/'runtime';rt.mkdir(mode=0o700)
contract=json.loads(Path('/workspace/qa-linux-22/browser-ui/KAN-29/full-product/runs/baseline-a629-x11-one-30076/contract.json').read_text())
for k in ['floorp','libxul.so','omni.ja','browser/omni.ja']:assert sha(B/k)==contract['payloadSha256'][k]
pack=Path(contract['languagePack']['path']);assert sha(pack)==contract['languagePack']['sha256']
prefs=json.loads(Path('/workspace/qa-linux-22/browser-ui/KAN-29/full-product/runs/candidate-f43-x11-30075/startup-prefs.json').read_text());prefs['marionette.port']=PORT
(P/'user.js').write_text(''.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');\n'for k,v in prefs.items()))
env=os.environ.copy();env.update(DISPLAY=DISPLAY,XDG_RUNTIME_DIR=str(rt),XDG_SESSION_TYPE='x11',XDG_CURRENT_DESKTOP='openbox',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');env.pop('WAYLAND_DISPLAY',None);assert 'MOZ_DISABLE_CONTENT_SANDBOX'not in env
prof={'MOZ_PROFILER_STARTUP':'1','MOZ_PROFILER_STARTUP_ENTRIES':'8MiB','MOZ_PROFILER_STARTUP_INTERVAL':'5','MOZ_PROFILER_STARTUP_FILTERS':'GeckoMain','MOZ_PROFILER_STARTUP_FEATURES':'js,stackwalk,processcpu,memory','MOZ_LOG':'prof:4'}
res={'sourceSha':'a6290b5811b8c5309b399ac340ec40b52f97b73c','runtimeSha':'c74fe1a545d275cdd710ab44701c1ee0dd6b0eae','freshProfile':True,'englishLaunches':0,'arabicLaunches':0,'phases':[],'strictArabicRTLQualified':False,'limits':{'ownedRssGiB':2,'cgroupGiB':6,'outputMiB':100,'englishWallSeconds':40,'arabicCaptureTargetSeconds':10,'arabicHardStopSeconds':15},'profilerEnvironment':prof}; browser=None;stop=threading.Event();rows=[];events=[];procs=[];phase='setup';began=0;guardFlag=threading.Event()

def sample():
 table={}
 for p in Path('/proc').iterdir():
  if not p.name.isdigit():continue
  try:
   st=(p/'stat').read_text();v=st[st.rfind(')')+2:].split();s=dict(l.split(':',1)for l in(p/'status').read_text().splitlines()if ':'in l);table[int(p.name)]={'ppid':int(v[1]),'rssKiB':int(s.get('VmRSS','0 kB').split()[0]),'cpuTicks':int(v[11])+int(v[12])}
  except(OSError,ValueError):pass
 ids={browser.pid}if browser and browser.poll()is None else set()
 while True:
  added={k for k,v in table.items()if v['ppid']in ids}-ids
  if not added:break
  ids|=added
 return {'phase':phase,'seconds':time.monotonic()-began,'pid':browser.pid if browser else None,'ownedRssKiB':sum(table[k]['rssKiB']for k in ids if k in table),'mainRssKiB':table.get(browser.pid,{}).get('rssKiB')if browser else None,'mainCpuTicks':table.get(browser.pid,{}).get('cpuTicks')if browser else None,'cgroupBytes':int(Path('/sys/fs/cgroup/memory.current').read_text()),'outputBytes':sum(p.stat().st_size for p in O.rglob('*')if p.is_file())}
def monitor():
 while not stop.wait(.2):
  try:
   x=sample();rows.append(x)
   if not browser or browser.poll()is not None:continue
   limit=15 if phase=='arabic' else 40
   if x['seconds']>limit or x['ownedRssKiB']>2*1024**2 or x['cgroupBytes']>6*1024**3 or x['outputBytes']>100*1024**2:
    res.setdefault('guards',[]).append(x);guardFlag.set();os.killpg(browser.pid,signal.SIGTERM);return
  except OSError:pass

def start(name):
 global browser,phase,began
 phase=name;began=time.monotonic();out=O/name;out.mkdir();e=env.copy();e.update(prof,MOZ_UPLOAD_DIR=str(out),TMPDIR=str(out));browser=subprocess.Popen([str(B/'floorp'),'--no-remote','--profile',str(P),'--marionette','about:blank'],env=e,stdout=open(out/'browser.log','w'),stderr=subprocess.STDOUT,start_new_session=True)
 row={'phase':name,'pid':browser.pid,'normalExit':False,'actualExeSha256':sha(Path('/proc')/str(browser.pid)/'exe')};res['phases'].append(row);save('ownership.json',res);return row

def sig(sig,label):
 s=dict(l.split(':',1)for l in(Path('/proc')/str(browser.pid)/'status').read_text().splitlines()if ':'in l);mask=int(s['SigCgt'].strip(),16);yes=bool(mask&(1<<(sig-1)));e={'phase':phase,'seconds':time.monotonic()-began,'pid':browser.pid,'signal':int(sig),'label':label,'registered':yes,'sent':False}
 if yes and browser.poll()is None:os.kill(browser.pid,sig);e['sent']=True
 events.append(e);save('signal-events.json',events)

try:
 xv=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',DISPLAY,'-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=open(O/'xvfb.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(xv);time.sleep(.7);assert xv.poll()is None
 we=env.copy();we.update(LD_LIBRARY_PATH='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu',XDG_DATA_DIRS='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share');wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable'],env=we,stdout=open(O/'wm.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(wm);time.sleep(.5)
 row=start('english');res['englishLaunches']=1;t=threading.Thread(target=monitor,daemon=True);t.start()
 sys.path.insert(0,'/workspace/qa-linux-22/browser-ui/python');from marionette_driver.marionette import Marionette
 m=Marionette('localhost',port=PORT,socket_timeout=10,startup_timeout=15);m.start_session(capabilities={'pageLoadStrategy':'none'});m.set_context('chrome')
 row['live']=m.execute_script('return {pid:Services.appinfo.processID,version:Services.appinfo.version,build:Services.appinfo.appBuildID,locale:Services.locale.appLocaleAsBCP47,rtl:Services.locale.isAppLocaleRTL,active:Services.profiler.IsActive(),config:Services.profiler.activeConfiguration,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),langpackSignature:Services.prefs.getBoolPref("extensions.langpacks.signatures.required")}');assert row['live']['version']=='157.0.1'and row['live']['build']=='20261007125927'and row['live']['active']and not row['live']['rtl']
 sig(signal.SIGUSR2,'English-registered-profiler-path-control');time.sleep(.5)
 row['localeInstall']=m.execute_async_script('''const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const {FileUtils}=ChromeUtils.importESModule('resource://gre/modules/FileUtils.sys.mjs');(async()=>{let i=await AddonManager.getInstallForFile(new FileUtils.File(arguments[0]));await i.install();let a=i.addon;if(a.signedState<=0||a.appDisabled)throw Error('unsigned/incompatible');Services.locale.requestedLocales=['ar'];Services.prefs.savePrefFile(null);done({id:a.id,version:a.version,signedState:a.signedState,appDisabled:a.appDisabled});})().catch(e=>done({error:String(e)}));''',script_args=[str(pack)],script_timeout=12000);assert not row['localeInstall'].get('error')
 m.execute_script('Services.prefs.savePrefFile(null);Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true');row['normalQuitRequested']=True;row['actualChildExit']=browser.wait(timeout=8);row['normalExit']=row['actualChildExit']==0;assert row['normalExit']and not guardFlag.is_set()
 try:m.client.close()
 except Exception:pass
 # No profile copy/migration; this fresh profile has only a normally-ended English launch.
 res['cleanBeforeArabic']={'prefsSha256':sha(P/'prefs.js'),'extensionsSha256':sha(P/'extensions.json'),'priorArabicLaunches':0,'previousNativeChildExit':row['actualChildExit']};save('ownership.json',res)
 row=start('arabic');res['arabicLaunches']=1;sentProf=sentMem=False
 while browser.poll()is None and time.monotonic()-began<10 and not guardFlag.is_set():
  dt=time.monotonic()-began
  if dt>=6 and not sentProf:sig(signal.SIGUSR2,'Arabic-early-profiler-once');sentProf=True
  if dt>=6.5 and not sentMem:sig(signal.SIGRTMIN,'Arabic-nonminimizing-memory-report-if-registered');sentMem=True
  time.sleep(.1)
 row['captureEndedSeconds']=time.monotonic()-began
 if browser.poll()is None and not guardFlag.is_set():
  q=subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','key','ctrl+q'],env=env,timeout=2,capture_output=True);row['ctrlQHelperExit']=q.returncode
  try:row['actualChildExit']=browser.wait(timeout=3);row['normalExit']=row['actualChildExit']==0
  except subprocess.TimeoutExpired:row['normalExit']=False;row['quitTimedOut']=True
 row['normalExitEstablished']=row.get('normalExit',False)
except Exception as e:res['error']=repr(e);(O/'runner-error.log').write_text(traceback.format_exc())
finally:
 if browser and browser.poll()is None:
  res['cleanupRequired']=True;os.killpg(browser.pid,signal.SIGTERM)
  try:browser.wait(timeout=2)
  except subprocess.TimeoutExpired:os.killpg(browser.pid,signal.SIGKILL);browser.wait()
 if browser and res['phases']:res['phases'][-1]['actualChildExit']=browser.returncode
 stop.set()
 for p in reversed(procs):
  if p.poll()is None:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=3)
 save('resources.json',rows);save('signal-events.json',events)
 res['profiles']={};res['memoryReports']={}
 for p in O.rglob('*.json'):
  try:
   ob=json.loads(p.read_text());ts=ob.get('threads',[]);res['profiles'][str(p.relative_to(O))]={'bytes':p.stat().st_size,'sha256':sha(p),'metaConfiguration':ob.get('meta',{}).get('configuration'),'threadPids':list({t.get('pid')for t in ts}),'samples':sum(len(t.get('samples',{}).get('data',[]))for t in ts),'hasStacks':any(t.get('stackTable',{}).get('data')for t in ts),'firstSampleMs':min((v[t['samples']['schema']['time']]for t in ts for v in t.get('samples',{}).get('data',[])),default=None),'lastSampleMs':max((v[t['samples']['schema']['time']]for t in ts for v in t.get('samples',{}).get('data',[])),default=None)}
  except Exception as e:res['profiles'][str(p.relative_to(O))]={'bytes':p.stat().st_size,'sha256':sha(p),'parseError':repr(e)}
 for p in O.rglob('*.json.gz'):
  try:ob=json.loads(gzip.decompress(p.read_bytes()));res['memoryReports'][str(p.relative_to(O))]={'bytes':p.stat().st_size,'sha256':sha(p),'reports':len(ob['reports'])}
  except Exception as e:res['memoryReports'][str(p.relative_to(O))]={'parseError':repr(e)}
 res['productUnchanged']={k:sha(B/k)==contract['payloadSha256'][k]for k in ['floorp','libxul.so','omni.ja','browser/omni.ja']}
 res['earlyArabicProfileObtained']=any(k.startswith('arabic/')and v.get('samples',0)>0 for k,v in res['profiles'].items());res['causeConfirmed']=False
 save('summary.json',res);print(json.dumps(res,indent=2));sys.exit(0 if res['earlyArabicProfileObtained'] else 1)
