"""One bounded fresh English native157 instrumentation preflight. Raw outputs private."""
import os,sys,time,json,subprocess,threading,signal,hashlib,gzip,traceback
from pathlib import Path
R=Path(__file__).resolve().parent; W=Path('/workspace/kan29-english-preflight'); B=Path('/workspace/kan29-package/baseline/floorp'); PORT=30130; DISPLAY=':130'
def save(n,v):(R/n).write_text(json.dumps(v,indent=2)+'\n')
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
if '--worker' not in sys.argv:raise SystemExit(subprocess.run(['dbus-run-session','--',sys.executable,str(Path(__file__).resolve()),'--worker']).returncode)
assert not W.exists();assert not Path('/tmp/.X11-unix/X130').exists()
W.mkdir(mode=0o700); P=W/'fresh-profile';P.mkdir();O=W/'private';O.mkdir(mode=0o700); rt=W/'runtime';rt.mkdir(mode=0o700)
prefs=json.loads(Path('/workspace/qa-linux-22/browser-ui/KAN-29/full-product/runs/candidate-f43-x11-30075/startup-prefs.json').read_text());prefs['marionette.port']=PORT
(P/'user.js').write_text(''.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');\n' for k,v in prefs.items()))
env=os.environ.copy();env.update(DISPLAY=DISPLAY,XDG_RUNTIME_DIR=str(rt),XDG_SESSION_TYPE='x11',XDG_CURRENT_DESKTOP='openbox',MOZ_ENABLE_WAYLAND='0',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',MOZ_PROFILER_STARTUP='1',MOZ_PROFILER_STARTUP_ENTRIES='8MiB',MOZ_PROFILER_STARTUP_INTERVAL='5',MOZ_PROFILER_STARTUP_FILTERS='GeckoMain',MOZ_PROFILER_STARTUP_FEATURES='js,stackwalk,processcpu,memory',MOZ_UPLOAD_DIR=str(O),TMPDIR=str(O));env.pop('WAYLAND_DISPLAY',None)
assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
res={'sourceSha':'a6290b5811b8c5309b399ac340ec40b52f97b73c','runtimeSha':'c74fe1a545d275cdd710ab44701c1ee0dd6b0eae','englishLaunches':0,'arabicLaunches':0,'freshProfile':True,'limits':{'wallSeconds':45,'ownedRssGiB':2,'cgroupGiB':6,'outputMiB':100},'requestedFeatures':['js','stackwalk','processcpu','memory'],'normalExit':False}; procs=[];browser=None;stop=threading.Event();samples=[];start=None

def guard():
 while not stop.wait(.2):
  try:
   rows={}
   for p in Path('/proc').iterdir():
    if not p.name.isdigit():continue
    try:
     st=(p/'stat').read_text();v=st[st.rfind(')')+2:].split();s=dict(l.split(':',1)for l in(p/'status').read_text().splitlines()if ':'in l);rows[int(p.name)]=(int(v[1]),int(s.get('VmRSS','0 kB').split()[0]))
    except(OSError,ValueError):pass
   ids={browser.pid} if browser and browser.poll()is None else set()
   while True:
    added={k for k,v in rows.items()if v[0]in ids}-ids
    if not added:break
    ids|=added
   rss=sum(rows[k][1]for k in ids if k in rows); cg=int(Path('/sys/fs/cgroup/memory.current').read_text());size=sum(p.stat().st_size for p in O.rglob('*')if p.is_file()); t=time.monotonic()-start
   samples.append({'seconds':t,'ownedRssKiB':rss,'cgroupBytes':cg,'outputBytes':size})
   if t>45 or rss>2*1024**2 or cg>6*1024**3 or size>100*1024**2:
    res['guardTriggered']=samples[-1];os.killpg(browser.pid,signal.SIGTERM);return
  except OSError:pass
try:
 xv=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',DISPLAY,'-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=open(O/'xvfb.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(xv);time.sleep(.7);assert xv.poll()is None
 we=env.copy();we.update(LD_LIBRARY_PATH='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu',XDG_DATA_DIRS='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share');wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable'],env=we,stdout=open(O/'wm.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(wm);time.sleep(.5)
 start=time.monotonic();browser=subprocess.Popen([str(B/'floorp'),'--no-remote','--profile',str(P),'--marionette','about:blank'],env=env,stdout=open(O/'browser.log','w'),stderr=subprocess.STDOUT,start_new_session=True);res.update(englishLaunches=1,pid=browser.pid,exeSha256=sha(B/'floorp'),libxulSha256=sha(B/'libxul.so'));thread=threading.Thread(target=guard,daemon=True);thread.start()
 sys.path.insert(0,'/workspace/qa-linux-22/browser-ui/python');from marionette_driver.marionette import Marionette
 m=Marionette('localhost',port=PORT,socket_timeout=12,startup_timeout=15);m.start_session(capabilities={'pageLoadStrategy':'none'});m.set_context('chrome')
 res['live']=m.execute_script('return {pid:Services.appinfo.processID,version:Services.appinfo.version,build:Services.appinfo.appBuildID,locales:Services.locale.appLocalesAsBCP47,rtl:Services.locale.isAppLocaleRTL,active:Services.profiler.IsActive(),available:Services.profiler.GetFeatures(),config:Services.profiler.activeConfiguration,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),langpackSignature:Services.prefs.getBoolPref("extensions.langpacks.signatures.required")}');assert res['live']['pid']==browser.pid and res['live']['active'] and not res['live']['rtl'];assert all(f in res['live']['available']for f in res['requestedFeatures'])
 res['memoryDump']=m.execute_async_script('''const done=arguments[arguments.length-1];try {Cc['@mozilla.org/memory-reporter-manager;1'].getService(Ci.nsIMemoryReporterManager);const d=Cc['@mozilla.org/memory-info-dumper;1'].getService(Ci.nsIMemoryInfoDumper);d.dumpMemoryReportsToNamedFile(arguments[0],()=>done({completed:true,pid:Services.appinfo.processID,minimizeMemoryUsage:false,anonymize:true}),null,true,false);}catch(e){done({error:String(e)});}''',script_args=[str(O/'memory.json.gz')],script_timeout=12000);assert res['memoryDump'].get('completed')
 res['profilerDump']=m.execute_async_script('''const done=arguments[arguments.length-1];const path=arguments[0];(async()=>{await Services.profiler.waitOnePeriodicSampling();const p=await Services.profiler.getProfileDataAsync();await IOUtils.writeUTF8(path,JSON.stringify(p));done({written:true,pid:Services.appinfo.processID});})().catch(e=>done({error:String(e)}));''',script_args=[str(O/'api-profile.json')],script_timeout=12000);assert res['profilerDump'].get('written')
 # Signal path only after registration checked; no unregistered signals or minimizing report.
 status=dict(l.split(':',1)for l in(Path('/proc')/str(browser.pid)/'status').read_text().splitlines()if ':'in l);mask=int(status['SigCgt'].strip(),16);res['registeredSignals']={str(int(s)):bool(mask&(1<<(int(s)-1)))for s in [signal.SIGUSR2,signal.SIGRTMIN]}
 if res['registeredSignals'][str(int(signal.SIGUSR2))]:os.kill(browser.pid,signal.SIGUSR2);res['usr2Sent']=True;time.sleep(1)
 m.execute_script('Services.tm.dispatchToMainThread(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit));return true');res['normalQuitRequested']=True;res['actualChildExit']=browser.wait(timeout=8);res['normalExit']=res['actualChildExit']==0
except Exception as e:res['error']=repr(e);(O/'runner-error.log').write_text(traceback.format_exc())
finally:
 if browser and browser.poll()is None:
  res['cleanupRequired']=True;os.killpg(browser.pid,signal.SIGTERM)
  try:browser.wait(timeout=3)
  except subprocess.TimeoutExpired:os.killpg(browser.pid,signal.SIGKILL);browser.wait()
 if browser:res['actualChildExit']=browser.returncode
 stop.set()
 for p in reversed(procs):
  if p.poll()is None:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=3)
 save('resources.json',samples)
 # Only aggregate output metadata; never publish raw profiler or memory report.
 def stats(p):
  obj=json.loads(p.read_text());threads=obj.get('threads',[]);return {'bytes':p.stat().st_size,'sha256':sha(p),'pidSet':sorted(set(t.get('pid')for t in threads if t.get('pid')is not None),key=str),'threads':len(threads),'sampleCount':sum(len(t.get('samples',{}).get('data',[]))for t in threads),'hasStacks':any(t.get('stackTable',{}).get('data')for t in threads)}
 res['profiles']={}
 for p in O.glob('*.json'):
  try:res['profiles'][p.name]=stats(p)
  except Exception as e:res['profiles'][p.name]={'parseError':str(e)}
 p=O/'memory.json.gz'
 if p.exists():
  try:
   obj=json.loads(gzip.decompress(p.read_bytes()));rs=obj['reports'];res['memoryReport']={'bytes':p.stat().st_size,'sha256':sha(p),'version':obj['version'],'reportCount':len(rs),'mainPidReports':sum('pid '+str(browser.pid)+')'in r['process']for r in rs),'hasHeapAllocated':any(r['path']=='heap-allocated'for r in rs),'hasExplicitJS':any(r['path'].startswith('explicit/js')for r in rs)}
  except Exception as e:res['memoryReport']={'parseError':str(e)}
 res['success']=bool(res.get('normalExit')and res.get('memoryReport',{}).get('mainPidReports',0)>0 and any(v.get('sampleCount',0)>0 and v.get('hasStacks')for v in res['profiles'].values())and not res.get('guardTriggered'))
 save('summary.json',res);print(json.dumps(res,indent=2));sys.exit(0 if res['success']else 1)
