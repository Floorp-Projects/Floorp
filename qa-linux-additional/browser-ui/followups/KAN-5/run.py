"""No browser starts until the parent explicitly opens the post-package gate."""
import os,sys,time,socket,signal,subprocess,json,hashlib,stat
from pathlib import Path
R=Path(__file__).parent
if os.environ.get('QA_KAN5_START_FOLLOWUP')!='1':
 print('PREPARED ONLY: package-complete/start-followup gate is closed; no display/browser launched.')
 raise SystemExit(2)
port=int(sys.argv[1]);display=os.environ.get('QA_DISPLAY',':123');B=Path('/workspace/qa-linux-22/browser-ui');product=Path(os.environ.get('QA_PRODUCT',str(B/'products/floorp/floorp'))).resolve()
assert not Path('/tmp/.X11-unix/X'+display.lstrip(':')).exists(),'display already owned'
s=socket.socket();assert s.connect_ex(('127.0.0.1',port))!=0,'port already owned';s.close()
profile=R/'runtime'/str(port);profile.mkdir(parents=True,exist_ok=False);(R/'logs').mkdir(exist_ok=True);(R/'screenshots').mkdir(exist_ok=True)
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\nuser_pref("focusmanager.testmode",false);\n')
deps=Path('/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root');env=os.environ.copy();env.update(DISPLAY=display,XDG_CURRENT_DESKTOP='openbox',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',PYTHONPATH=str(B/'python'),QA_EXPECTED_PRODUCT=str(product),XDG_CONFIG_HOME=str(R/'runtime/config'),XDG_CACHE_HOME=str(R/'runtime/cache'),XDG_DATA_DIRS=str(deps/'usr/share')+':/usr/local/share:/usr/share',LD_LIBRARY_PATH=str(deps/'usr/lib/x86_64-linux-gnu')+':/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
def payload():
 result={}
 for f in sorted(product.parent.rglob('*')):
  item={'mode':stat.S_IMODE(f.lstat().st_mode),'type':'link' if f.is_symlink() else 'dir' if f.is_dir() else 'file'}
  if f.is_symlink():item['target']=os.readlink(f)
  elif f.is_file():
   with f.open('rb') as stream:item['sha256']=hashlib.file_digest(stream,'sha256').hexdigest()
  result[str(f.relative_to(product.parent))]=item
 return result
before=payload();(R/f'payload-before-{port}.json').write_text(json.dumps(before,indent=2))
owned=[]
def launch(args,label):
 f=open(R/'logs'/f'{port}-{label}.log','w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);owned.append(p);return p
def browser(label):
 p=launch(['dbus-run-session','--',str(product),'--no-remote','--profile',str(profile),'--marionette','about:blank'],label)
 for _ in range(200):
  assert p.poll() is None,'browser exited before Marionette'
  try:s=socket.create_connection(('127.0.0.1',port),.2);s.close();break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('Marionette port unavailable')
 time.sleep(3);return p
try:
 x=launch(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',display,'-screen','0','1680x1000x24','-nolisten','tcp'],'xvfb');time.sleep(1);assert x.poll() is None
 w=launch([str(deps/'usr/bin/openbox'),'--sm-disable'],'openbox');time.sleep(1);assert w.poll() is None
 (R/f'ownership-{port}.json').write_text(json.dumps({'display':display,'socketAbsentBeforeStart':True,'xvfbPid':x.pid,'openboxPid':w.pid,'product':str(product),'profile':str(profile)},indent=2))
 p=browser('setup-browser');subprocess.run([sys.executable,str(R/'probe.py'),str(port),'enable'],env=env,check=True,timeout=90);p.wait(timeout=15);assert p.returncode==0,'normal setup quit failed; do not reuse uncertain profile';(R/f'normal-quit-{port}.json').write_text(json.dumps({'setupBrowserExit':p.returncode,'forcedQuit':False,'sameProfileRestart':str(profile)},indent=2))
 p=browser('drop-browser');result=subprocess.run([sys.executable,str(R/'probe.py'),str(port),'drop'],env=env,check=False,timeout=180);p.wait(timeout=15);(R/f'final-quit-{port}.json').write_text(json.dumps({'browserExit':p.returncode,'probeExit':result.returncode,'forcedQuit':False},indent=2));assert p.returncode==0,'drop browser normal quit failed';assert result.returncode==0,'probe stopped; review its concrete fixture/observation evidence'
finally:
 for p in reversed(owned):
  if p.poll() is not None:continue
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except ProcessLookupError:pass
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)

 after=payload();(R/f'payload-after-{port}.json').write_text(json.dumps(after,indent=2));(R/f'payload-comparison-{port}.json').write_text(json.dumps({'entriesBefore':len(before),'entriesAfter':len(after),'unchanged':before==after,'differences':[name for name in sorted(set(before)|set(after)) if before.get(name)!=after.get(name)]},indent=2))
