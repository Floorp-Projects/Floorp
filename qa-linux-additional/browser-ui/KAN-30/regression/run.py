import os,sys,json,time,socket,signal,subprocess
from pathlib import Path
R=Path(__file__).parent;B=Path('/workspace/qa-linux-22/browser-ui');port=int(sys.argv[1]);kind=sys.argv[2]if len(sys.argv)>2 else'probe';product=os.environ.get('QA_PRODUCT',str(B/'products/floorp/floorp'));profile=R/'runtime'/f'profile-{port}';profile.mkdir(parents=True,exist_ok=False);(R/'logs').mkdir(exist_ok=True)
design=json.loads(json.loads((B/'inspect-29284.json').read_text())['observations'][0]['design']);design.setdefault('uiCustomization',{}).setdefault('chromeExtras',{}).update(sidebarOverlap=True,autohideSidebar=True)
prefs={'marionette.port':port,'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,'floorp.design.configs':json.dumps(design)}
(profile/'user.js').write_text(''.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');\n'for k,v in prefs.items()))
env=os.environ.copy();env.update(DISPLAY=':117',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',PYTHONPATH=str(B/'python'),XDG_CACHE_HOME=str(R/'runtime/cache'),XDG_CONFIG_HOME=str(R/'runtime/config'));assert 'MOZ_DISABLE_CONTENT_SANDBOX'not in env
owned=[]
def launch(args,name):
 f=open(R/'logs'/name,'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);owned.append(p);return p
try:
 assert not Path('/tmp/.X11-unix/X117').exists(),'display117 already owned'
 x=launch(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',':117','-screen','0','1440x1100x24','-nolisten','tcp'],f'xvfb-{port}.log');time.sleep(1)
 if x.poll()is not None:raise RuntimeError('private Xvfb failed; do not reuse any display')
 (R/f'ownership-{port}.json').write_text(json.dumps({'display':':117','xvfbPid':x.pid,'privateSocketWasAbsent':True,'xvfbAlive':True}))
 p=launch(['dbus-run-session','--',product,'--no-remote','--profile',str(profile),'--marionette','about:blank'],f'browser-{port}.log')
 for i in range(180):
  if p.poll()is not None:raise RuntimeError('product exit '+str(p.returncode))
  try:s=socket.create_connection(('localhost',port),.2);s.close();break
  except OSError:time.sleep(.3)
 else:raise RuntimeError('Marionette not ready')
 time.sleep(4);subprocess.run([sys.executable,str(R/'probe.py'),str(port),kind],env=env,check=True,timeout=240)
finally:
 for p in reversed(owned):
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except ProcessLookupError:pass
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)
