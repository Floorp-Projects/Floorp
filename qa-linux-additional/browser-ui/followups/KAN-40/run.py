import os,sys,time,json,hashlib,subprocess,signal,traceback
from pathlib import Path
R=Path(__file__).parent;port=int(sys.argv[1]);out=R/('run-'+str(port));out.mkdir(exist_ok=False);profile=Path(os.environ['QA40_REUSE_PROFILE']) if os.environ.get('QA40_REUSE_PROFILE') else Path('/workspace/kan40-runtime')/str(port)/'profile';profile.mkdir(parents=True,exist_ok=bool(os.environ.get('QA40_REUSE_PROFILE')));product=Path('/workspace/kan97-runtime/full-package/floorp');exe=product/'floorp'
(profile/'user.js').write_text('user_pref("marionette.port",'+str(port)+');\nuser_pref("focusmanager.testmode",false);\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\n')
def manifest():
 d={}
 for f in sorted(product.rglob('*')):
  r=str(f.relative_to(product))
  if f.is_symlink():d[r]={'type':'symlink','link':os.readlink(f)}
  elif f.is_file():
   with f.open('rb') as h:d[r]={'type':'file','bytes':f.stat().st_size,'sha256':hashlib.file_digest(h,'sha256').hexdigest()}
 return d
before=manifest();(out/'product-before.json').write_text(json.dumps(before,indent=2));env=os.environ.copy();env.update(DISPLAY=':124',XDG_CURRENT_DESKTOP='openbox',XDG_SESSION_TYPE='x11',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',PYTHONPATH='/workspace/qa-linux-22/browser-ui/python',QA40_OUTPUT=str(out));assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env;processes=[];result={'sourceSha':'d6bd2fc457774f55ab1f36a2b01c941f908f1ab0','candidateKind':'complete unmodified production package with unrelated KAN97 focused-scroll fix; not a middleclick candidate','profile':str(profile),'phases':[]}
try:
 assert not Path('/tmp/.X11-unix/X124').exists()
 display=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',':124','-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=open(out/'display.log','w'),stderr=subprocess.STDOUT,start_new_session=True);processes.append(display);time.sleep(1)
 we=env.copy();we['LD_LIBRARY_PATH']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu';we['XDG_DATA_DIRS']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share'
 wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable'],env=we,stdout=open(out/'openbox.log','w'),stderr=subprocess.STDOUT,start_new_session=True);processes.append(wm);time.sleep(1);assert display.poll() is None and wm.poll() is None
 result['ownedDisplayPid']=display.pid;result['ownedWmPid']=wm.pid
 for stage in (['interactions'] if os.environ.get('QA40_CHIP_ONLY')=='1' else ['setup','interactions']):
  browser=subprocess.Popen(['dbus-run-session','--',str(exe),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=open(out/(stage+'-browser.log'),'w'),stderr=subprocess.STDOUT,start_new_session=True);processes.append(browser);time.sleep(3)
  row={'stage':stage,'dbusPid':browser.pid};result['phases'].append(row);(out/'ownership.json').write_text(json.dumps(result,indent=2))
  test=subprocess.run([sys.executable,str(R/'probe.py'),str(port),stage],env=env,timeout=100,stdout=open(out/(stage+'-probe.log'),'w'),stderr=subprocess.STDOUT);row['probeExit']=test.returncode
  row['productDbusExit']=browser.wait(timeout=25);row['normalExit']=row['productDbusExit']==0;assert row['normalExit'] and test.returncode==0,row
 result['complete']=True
except Exception as e:result['error']=repr(e);result['traceback']=traceback.format_exc()
finally:
 for proc in reversed(processes):
  if proc.poll() is None:
   if proc not in [display,wm]:result['abnormalProductCleanup']=True
   try:os.killpg(proc.pid,signal.SIGTERM);proc.wait(timeout=8)
   except (ProcessLookupError,subprocess.TimeoutExpired):pass
 after=manifest();(out/'product-after.json').write_text(json.dumps(after,indent=2));result['productUnmodified']=before==after;result['displayRemoved']=not Path('/tmp/.X11-unix/X124').exists();(out/'runner-result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
if result.get('error'):sys.exit(1)
