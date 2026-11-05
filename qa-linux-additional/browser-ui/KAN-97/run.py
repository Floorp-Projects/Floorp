import os,sys,time,subprocess,signal,threading,json
from pathlib import Path
from fixture import Handler,ThreadingHTTPServer
R=Path(__file__).parent;product=sys.argv[1];port=int(sys.argv[2]);tag=product+('-metadata' if len(sys.argv)>3 and sys.argv[3]=='metadata' else '');work=Path('/workspace/kan97-runtime')/product;work.mkdir(parents=True,exist_ok=True);profile=work/'profile';profile.mkdir(exist_ok=True)
paths={'patched':'/workspace/kan97-runtime/candidate/floorp','official':'/workspace/qa-linux-22/browser-ui/products/floorp/floorp','combined':'/workspace/full-product-3031-3034/obj-artifact-build-output/dist/bin/floorp','firefox':'/workspace/qa-linux-22/browser-ui/products/firefox/firefox'}
(profile/'user.js').write_text(f'user_pref("marionette.port",{port});\nuser_pref("focusmanager.testmode",false);\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\n')
env=os.environ.copy();env.update(DISPLAY=':117',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',PYTHONPATH='/workspace/qa-linux-22/browser-ui/python');assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
servers=[];procs=[]
try:
 assert not Path('/tmp/.X11-unix/X117').exists(),'display already owned'
 for hp in [29971,29972]:
  server=ThreadingHTTPServer(('127.0.0.1',hp),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();servers.append(server)
 for name,args in [('display',['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',':117','-screen','0','1440x1100x24','-nolisten','tcp']),('browser',['dbus-run-session','--',paths[product],'--no-remote','--profile',str(profile),'--marionette','about:blank'])]:
  f=open(R/'logs'/(tag+'-'+name+'.log'),'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);procs.append(p);time.sleep(2);assert p.poll() is None,name+' startup failed'
  if name=='display':
   cfg=R/'openbox.xml';cfg.write_text('<openbox_config xmlns="http://openbox.org/3.4/rc"><focus><focusNew>yes</focusNew><followMouse>no</followMouse></focus></openbox_config>')
   we=env.copy();we['LD_LIBRARY_PATH']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu';we['XDG_DATA_DIRS']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share'
   wf=open(R/'logs'/(tag+'-openbox.log'),'w');wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable','--config-file',str(cfg)],env=we,stdout=wf,stderr=wf,start_new_session=True);procs.append(wm);time.sleep(1);assert wm.poll() is None,'WM startup failed'
 (R/(tag+'-ownership.json')).write_text(json.dumps({'display':':117','pidDisplay':procs[0].pid,'pidWindowManager':procs[1].pid,'pidBrowserGroup':procs[2].pid,'product':paths[product],'profile':str(profile)},indent=2))
 subprocess.run([sys.executable,str(R/('metadata.py' if len(sys.argv)>3 and sys.argv[3]=='metadata' else 'probe.py')),product,str(port)],env=env,timeout=240,check=True)
finally:
 for p in reversed(procs):
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except (ProcessLookupError,subprocess.TimeoutExpired):pass
 for s in servers:s.shutdown();s.server_close()
