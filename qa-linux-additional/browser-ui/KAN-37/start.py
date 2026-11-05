import os,subprocess,time,signal,sys
from pathlib import Path
R=Path(__file__).parent; product=os.environ.get('QA_PRODUCT','floorp');display=os.environ.get('QA_DISPLAY',':99');port=int(os.environ.get('QA_PORT','29837'));work=Path('/workspace/kan37-runtime')/product;work.mkdir(exist_ok=True);profile=(Path('/workspace/kan37-runtime/profile') if product=='floorp' else work/'profile');profile.mkdir(exist_ok=True)
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\n')
if os.environ.get('QA_PROXY')=='1':
 if not (profile/'cert9.db').exists():subprocess.run(['certutil','-N','-d','sql:'+str(profile),'--empty-password'],check=True)
 with (profile/'user.js').open('a') as pref:
  pref.write('user_pref("network.proxy.type",1);\nuser_pref("network.proxy.http","proxy");\nuser_pref("network.proxy.http_port",8080);\nuser_pref("network.proxy.ssl","proxy");\nuser_pref("network.proxy.ssl_port",8080);\nuser_pref("network.proxy.no_proxies_on","localhost,127.0.0.1");\n')
 for ca in Path('/usr/local/share/ca-certificates').glob('*.crt'):
  subprocess.run(['certutil','-A','-d','sql:'+str(profile),'-n','Cloud session '+ca.stem,'-t','C,,','-i',str(ca)],check=True)
env=os.environ.copy();env.update(DISPLAY=display,MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu');assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
ps=[]
try:
 for name,args in [('xvfb',['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',display,'-screen','0','1600x1000x24','-nolisten','tcp']),('floorp',['dbus-run-session','--',f'/workspace/qa-linux-22/browser-ui/products/{product}/{product}','--no-remote','--profile',str(profile),'--marionette','about:newtab'])]:
  f=open(R/'logs'/(name+'-'+product+'.log'),'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);ps.append(p);time.sleep(2);assert p.poll() is None, name+' failed to start';(R/'logs'/('pid-'+name+'-'+product+'.txt')).write_text(str(p.pid))
 print('READY',flush=True)
 while ps[-1].poll() is None:time.sleep(1)
finally:
 for p in reversed(ps):
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except (ProcessLookupError,subprocess.TimeoutExpired):pass
