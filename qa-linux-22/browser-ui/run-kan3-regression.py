import os,sys,subprocess,time,socket,json,signal
from pathlib import Path
ROOT=Path(__file__).parent
product=sys.argv[1];port=int(sys.argv[2]);scenarios=sys.argv[3:]
profile=ROOT/'runtime'/('profile-'+product+'-'+str(int(time.time())))
profile.mkdir(parents=True);(ROOT/'environment').mkdir(exist_ok=True);(profile/'user.js').write_text('user_pref("marionette.port",'+str(port)+');\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("floorp.tabstacks.enabled",true);\nuser_pref("browser.aboutwelcome.enabled",false);\nuser_pref("browser.tabs.splitView.enabled",true);\n')
env=os.environ.copy();env.update(DISPLAY=env.get('DISPLAY', ':92'),MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',XDG_CACHE_HOME=str(ROOT/'runtime/cache'),XDG_CONFIG_HOME=str(ROOT/'runtime/config'),PYTHONPATH=str(ROOT/'python'))
assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
f=open(ROOT/'environment'/(product+'-run-'+str(port)+'.log'),'w')
p=subprocess.Popen(['dbus-run-session','--',str(ROOT/'products'/product/product),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=f,stderr=f,start_new_session=True)
try:
 for i in range(100):
  if p.poll() is not None:raise RuntimeError('browser exited '+str(p.returncode))
  try:
   s=socket.create_connection(('127.0.0.1',port),.3);s.close();break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('port not ready')
 time.sleep(3)
 for case in scenarios:
  print('RUN',product,case,flush=True)
  subprocess.run([sys.executable,str(ROOT/'KAN-3/regression-probe.py'),case,str(port)],env=env,timeout=120)
finally:
 os.killpg(p.pid,signal.SIGTERM)
 try:p.wait(timeout=10)
 except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)
