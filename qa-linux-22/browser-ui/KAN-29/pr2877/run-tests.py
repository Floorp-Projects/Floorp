import os,sys,subprocess,socket,time,signal
from pathlib import Path
R=Path(__file__).parent;B=R.parents[1];port=int(sys.argv[1]);product=sys.argv[2];tests=sys.argv[3:];profile=R/'runtime'/f'profile-{port}-{int(time.time())}';profile.mkdir()
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\n')
env=os.environ.copy();env.update(MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',PYTHONPATH=str(B/'python'));assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
f=open(R/'logs'/f'browser-{port}.log','w');p=subprocess.Popen([str(B/'products'/product/product),'--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=f,stderr=f,start_new_session=True)
try:
 for i in range(450):
  if p.poll() is not None:raise RuntimeError('browser exited '+str(p.returncode))
  try:s=socket.create_connection(('127.0.0.1',port),.2);s.close();break
  except OSError:time.sleep(.2)
 else:raise RuntimeError('port not ready')
 time.sleep(5)
 for test in tests:subprocess.run([sys.executable,str(R/'probe-tests.py'),str(port),test],env=env,timeout=220,check=True)
finally:
 try:
  os.killpg(p.pid,signal.SIGTERM)
  try:p.wait(timeout=10)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)
 except ProcessLookupError:pass
