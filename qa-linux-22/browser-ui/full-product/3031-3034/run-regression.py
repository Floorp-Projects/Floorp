import os,sys,time,socket,subprocess,signal
from pathlib import Path
R=Path(__file__).parent;B=R.parents[1];stage=Path('/workspace/full-product-3031-3034');port=int(sys.argv[1]);profile=stage/'run'/f'profile-{port}';profile.mkdir(parents=True)
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\n')
env=os.environ.copy();env.update(DISPLAY=':97',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',XDG_CACHE_HOME=str(stage/'run/cache'),XDG_CONFIG_HOME=str(stage/'run/config'),PYTHONPATH=str(B/'python'),QA_COMBO_XTEST='1',QA_COMBO_EXPECT_PASS='1');assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
xlog=open(R/'logs'/f'xvfb-{port}.log','w');x=subprocess.Popen(['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',':97','-screen','0','1440x1100x24','-nolisten','tcp'],env=env,stdout=xlog,stderr=xlog,start_new_session=True)
p=None
try:
 for i in range(100):
  if x.poll() is not None:raise RuntimeError('Xvfb exited '+str(x.returncode))
  if Path('/tmp/.X11-unix/X97').is_socket():break
  time.sleep(.1)
 f=open(R/'logs'/f'browser-{port}.log','w');p=subprocess.Popen(['dbus-run-session','--',str(stage/'obj-artifact-build-output/dist/bin/floorp'),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=f,stderr=f,start_new_session=True)
 for i in range(450):
  if p.poll() is not None:raise RuntimeError('Browser exited '+str(p.returncode))
  try:s=socket.create_connection(('127.0.0.1',port),.2);s.close();break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('port not ready')
 time.sleep(5);subprocess.run([sys.executable,str(R/'regression.py'),'combination',str(port)],env=env,timeout=180,check=True)
finally:
 for proc in [p,x]:
  if proc is None:continue
  try:
   os.killpg(proc.pid,signal.SIGTERM)
   try:proc.wait(timeout=10)
   except subprocess.TimeoutExpired:os.killpg(proc.pid,signal.SIGKILL)
  except ProcessLookupError:pass
