import os,sys,subprocess,time,socket,json,signal
from pathlib import Path
ROOT=Path(__file__).parent
product=sys.argv[1];port=int(sys.argv[2]);scenarios=sys.argv[3:]
profile=ROOT/'runtime'/('profile-'+product+'-'+str(int(time.time())))
profile.mkdir();(profile/'user.js').write_text('user_pref("marionette.port",'+str(port)+');\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("floorp.tabstacks.enabled",true);\nuser_pref("browser.aboutwelcome.enabled",false);\nuser_pref("browser.tabs.splitView.enabled",true);\n')
config=json.loads((ROOT/'inspect-29284.json').read_text())['observations'][0]['design']
config=json.loads(config);config['tabbar']['tabbarStyle']='vertical'
with (profile/'user.js').open('a') as out:
 for key,value in {'sidebar.revamp':True,'sidebar.verticalTabs':True,'sidebar.visibility':'expand-on-hover','intl.uidirection':(os.environ.get('QA_DIR','ltr') if os.environ.get('QA_ROOTDIR_FIXTURE') else (1 if os.environ.get('QA_DIR')=='rtl' else 0)),'floorp.design.configs':json.dumps(config),'floorp.panelSidebar.config':json.dumps({'globalWidth':400,'autoUnload':False,'position_start':True,'displayed':True,'webExtensionRunningEnabled':False,'overlay':False,'openOnHover':False})}.items():out.write('user_pref('+json.dumps(key)+','+json.dumps(value)+');\n')
env=os.environ.copy();env.update(DISPLAY=':92',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',XDG_CACHE_HOME=str(ROOT/'runtime/cache'),XDG_CONFIG_HOME=str(ROOT/'runtime/config'),PYTHONPATH=os.environ.get('QA_PYTHONPATH',str(ROOT/'python')))
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
  subprocess.run([sys.executable,str(ROOT/'kan29-wayland.py'),case,str(port)],env=env,timeout=120)
finally:
 try:
  os.killpg(p.pid,signal.SIGTERM)
  try:p.wait(timeout=10)
  except subprocess.TimeoutExpired:os.killpg(p.pid,signal.SIGKILL)
 except ProcessLookupError:pass
