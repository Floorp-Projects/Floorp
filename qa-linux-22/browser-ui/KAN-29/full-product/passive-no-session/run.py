"""Single-factor diagnostic. No socket connection, RPC, Marionette import or NewSession."""
import os,sys,json,time,signal,subprocess,shutil,hashlib,traceback
from pathlib import Path
R=Path(__file__).resolve().parent
SRC=Path('/workspace/kan29-full-runtime/candidate-f43-x11-30075/profile')
PRODUCT=Path('/workspace/kan29-package/candidate/floorp')
WORK=Path('/workspace/kan29-passive-no-session');PROFILE=WORK/'profile'
PORT=30078;DISPLAY=':127'
def refs():
 found=[]
 for p in Path('/proc').iterdir():
  if not p.name.isdigit():continue
  try:
   args=(p/'cmdline').read_bytes().replace(b'\0',b' ').decode(errors='replace')
   if str(SRC) in args and 'floorp' in args and not any(x in args for x in ['python','bash','exec_command']):found.append(int(p.name))
   for fd in (p/'fd').iterdir():
    try:
     if os.readlink(fd).startswith(str(SRC)+'/'):found.append(int(p.name));break
    except OSError:pass
  except OSError:pass
 return sorted(set(found))
def manifest():
 d={}
 for f in sorted(PRODUCT.rglob('*')):
  if f.is_symlink():d[str(f.relative_to(PRODUCT))]={'link':os.readlink(f)}
  elif f.is_file():
   with f.open('rb') as stream:d[str(f.relative_to(PRODUCT))]={'size':f.stat().st_size,'sha256':hashlib.file_digest(stream,'sha256').hexdigest()}
 return d
if '--execute' not in sys.argv:
 print(json.dumps({'prepared':True,'browserStarted':False,'sourceProfileRefs':refs(),'displayOccupied':Path('/tmp/.X11-unix/X127').exists(),'profileBytes':sum(f.stat().st_size for f in SRC.rglob('*') if f.is_file()),'gate':'QA_KAN29_PASSIVE_START=1 and --execute required','sourceProfile':str(SRC),'dedicatedProfile':str(PROFILE),'port':PORT,'display':DISPLAY,'durationSeconds':60,'noRpcOrTcpConnection':True,'strictLocaleNotMeasured':True},indent=2));sys.exit()
assert os.environ.get('QA_KAN29_PASSIVE_START')=='1','root start gate required'
if '--worker' not in sys.argv:
 raise SystemExit(subprocess.run(['dbus-run-session','--',sys.executable,str(Path(__file__).resolve()),'--execute','--worker']).returncode)
assert not refs(),'source profile still referenced'
assert not Path('/tmp/.X11-unix/X127').exists(),'private display occupied'
WORK.mkdir(exist_ok=False);shutil.copytree(SRC,PROFILE,ignore=shutil.ignore_patterns('lock','.parentlock'))
# Only port changes in copied user.js; all locale/protection/render prefs preserved.
u=PROFILE/'user.js';original=u.read_text();import re
changed=re.sub(r'user_pref\("marionette.port",\s*\d+\);','user_pref("marionette.port", '+str(PORT)+');',original);assert changed!=original;u.write_text(changed)
runtime=WORK/'runtime';runtime.mkdir(mode=0o700)
env=os.environ.copy();env.update(DISPLAY=DISPLAY,XDG_CURRENT_DESKTOP='openbox',XDG_SESSION_TYPE='x11',MOZ_ENABLE_WAYLAND='0',XDG_RUNTIME_DIR=str(runtime),LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');env.pop('WAYLAND_DISPLAY',None)
assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
sys.path.insert(0,str(R.parent));from resource_monitor import Monitor,ResourceBoundExceeded
procs=[];browser=None;result={'singleFactor':'No Marionette NewSession; no RPC/TCP connections at all; CLI --marionette retained','profileCopy':str(PROFILE),'sourceProfile':str(SRC),'port':PORT,'display':DISPLAY,'strictRTL_UI':'NOT MEASURED','KAN29Pass':False,'durationRequestedSeconds':60}
before=manifest();(R/'product-before.json').write_text(json.dumps(before,indent=2))
def bound(s,f):raise ResourceBoundExceeded('passive resource bound')
signal.signal(signal.SIGUSR1,bound)
monitor=Monitor(R/'resources.jsonl',lambda:browser.pid if browser and browser.poll() is None else None,lambda:[p.pid for k,p in procs if k!='browser']);monitor.start()
try:
 xv=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',DISPLAY,'-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=open(R/'xvfb.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('xvfb',xv));time.sleep(1);assert xv.poll() is None
 we=env.copy();we.update(LD_LIBRARY_PATH='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu',XDG_DATA_DIRS='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share')
 wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable'],env=we,stdout=open(R/'openbox.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('openbox',wm));time.sleep(1);assert wm.poll() is None
 began=time.monotonic();browser=subprocess.Popen([str(PRODUCT/'floorp'),'--no-remote','--profile',str(PROFILE),'--marionette','about:blank'],env=env,stdout=open(R/'browser.log','w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(('browser',browser));result['pid']=browser.pid;result['actualExecutable']=os.readlink('/proc/'+str(browser.pid)+'/exe')
 for second in range(60):
  time.sleep(1)
  if browser.poll() is not None:raise RuntimeError('early child exit '+str(browser.returncode))
  if second in [14,59]:
   s=subprocess.run(['/usr/bin/import','-window','root',str(R/('screen-'+str(second+1)+'s.png'))],env=env,timeout=5,capture_output=True);result['screenshot'+str(second+1)]={'exit':s.returncode}
 result['passiveSeconds']=time.monotonic()-began
except Exception as e:result['error']=repr(e)
finally:
 result['resourceBound']=monitor.exceeded
 if browser and browser.poll() is None:
  try:
   q=subprocess.run(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','key','ctrl+q'],env=env,timeout=4,capture_output=True);result['osCtrlQExit']=q.returncode;result['actualNativeChildExit']=browser.wait(timeout=12)
  except Exception as e:result['quitError']=repr(e)
 for kind,p in reversed(procs):
  if p.poll() is None:
   if kind=='browser':result['abnormalCleanup']=True
   try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=5)
   except (ProcessLookupError,subprocess.TimeoutExpired):pass
 result['actualNativeChildExit']=browser.poll() if browser else None
 monitor.stop();after=manifest();(R/'product-after.json').write_text(json.dumps(after,indent=2));result['fullPayloadUnchanged']=before==after;(R/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
