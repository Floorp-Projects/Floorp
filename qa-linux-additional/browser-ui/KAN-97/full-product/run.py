import os,sys,time,subprocess,signal,threading,json,hashlib,traceback,configparser
from pathlib import Path
from fixture import Handler,Server
R=Path(__file__).parent;exe=Path(sys.argv[1]).resolve();port=int(sys.argv[2]);label='full-'+str(port);out=R/'runs'/label;out.mkdir(parents=True,exist_ok=False)
assert exe.is_file();product=exe.parent;work=Path('/workspace/kan97-runtime/full-product')/label;profile=work/'profile';profile.mkdir(parents=True)
prefs={'marionette.port':port,'focusmanager.testmode':False,'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False}
text=''.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');\n' for k,v in prefs.items());(profile/'user.js').write_text(text);(out/'startup-user.js').write_text(text)
env=os.environ.copy();env.update(DISPLAY=':117',XDG_CURRENT_DESKTOP='openbox',XDG_SESSION_TYPE='x11',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',PYTHONPATH='/workspace/qa-linux-22/browser-ui/python',QA97_OUTPUT=str(out),QA97_EXPECT_GRE=str(product),QA97_SOURCE_SHA='d6bd2fc457774f55ab1f36a2b01c941f908f1ab0',QA97_PRODUCT_ROOT=str(product));assert 'MOZ_DISABLE_CONTENT_SANDBOX' not in env
if len(sys.argv)>3 and sys.argv[3]=='guards':env['QA97_GUARDS_ONLY']='1'

def digest(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
def manifest():
 data={}
 for p in sorted(product.rglob('*')):
  rel=str(p.relative_to(product))
  if p.is_symlink():data[rel]={'type':'symlink','link':os.readlink(p)}
  elif p.is_file():data[rel]={'type':'file','bytes':p.stat().st_size,'sha256':digest(p)}
 return data
ini={}
for name in ['application.ini','platform.ini']:
 parser=configparser.ConfigParser(interpolation=None);parser.read(product/name);ini[name]={section:dict(parser[section]) for section in parser.sections()}
(out/'packaged-ini.json').write_text(json.dumps(ini,indent=2))
before=manifest();(out/'product-before.json').write_text(json.dumps(before,indent=2));servers=[];processes=[];browser=None;result={'executable':str(exe),'greExpected':str(product),'sourceSha':env['QA97_SOURCE_SHA'],'productUnmodified':None,'normalProductExit':False}
try:
 assert not Path('/tmp/.X11-unix/X117').exists(),'private display already occupied'
 for hp in [29971,29972]:
  server=Server(('127.0.0.1',hp),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();servers.append(server)
 for name,args in [('display',['/workspace/.floorp-cloud/sysroot/usr/bin/Xvfb',':117','-screen','0','1440x1100x24','-nolisten','tcp']),('browser',['dbus-run-session','--',str(exe),'--no-remote','--profile',str(profile),'--marionette','about:blank'])]:
  f=open(out/(name+'.log'),'w');p=subprocess.Popen(args,env=env,stdout=f,stderr=f,start_new_session=True);processes.append((name,p));time.sleep(2);assert p.poll() is None,name+' startup failed'
  if name=='display':
   cfg=work/'openbox.xml';cfg.write_text('<openbox_config xmlns="http://openbox.org/3.4/rc"><focus><focusNew>yes</focusNew><followMouse>no</followMouse></focus></openbox_config>')
   we=env.copy();we['LD_LIBRARY_PATH']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/lib/x86_64-linux-gnu';we['XDG_DATA_DIRS']='/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/share:/usr/share'
   wf=open(out/'openbox.log','w');wm=subprocess.Popen(['/workspace/qa-linux-additional/browser-ui/KAN-30/deps/root/usr/bin/openbox','--sm-disable','--config-file',str(cfg)],env=we,stdout=wf,stderr=wf,start_new_session=True);processes.append(('wm',wm));time.sleep(1);assert wm.poll() is None
  else:browser=p
 result['ownedProcesses']={name:p.pid for name,p in processes};result['profile']=str(profile);(out/'ownership.json').write_text(json.dumps(result,indent=2))
 tested=subprocess.run([sys.executable,str(R/'probe.py'),str(port)],env=env,timeout=360);result['probeExit']=tested.returncode
 # Probe always requests normal app quit; never use SIGTERM as a passing shutdown.
 try:result['dbusProductExit']=browser.wait(timeout=25);result['normalProductExit']=result['dbusProductExit']==0
 except subprocess.TimeoutExpired:result['normalExitTimeout']=True
except Exception as e:result['runnerError']=repr(e);result['traceback']=traceback.format_exc()
finally:
 if browser is not None and browser.poll() is None:
  result['abnormalProductCleanup']=True
  try:os.killpg(browser.pid,signal.SIGTERM);browser.wait(timeout=10)
  except (ProcessLookupError,subprocess.TimeoutExpired):
   try:os.killpg(browser.pid,signal.SIGKILL)
   except ProcessLookupError:pass
 for name,p in reversed(processes):
  if name=='browser' or p.poll() is not None:continue
  try:os.killpg(p.pid,signal.SIGTERM);p.wait(timeout=10)
  except (ProcessLookupError,subprocess.TimeoutExpired):pass
 for s in servers:s.shutdown();s.server_close()
 after=manifest();(out/'product-after.json').write_text(json.dumps(after,indent=2));result['productUnmodified']=before==after;result['changedPayloadEntries']=[k for k in before if before[k]!=after.get(k)];result['addedRuntimeEntries']=[k for k in after if k not in before];result['originalPayloadUnchanged']=not result['changedPayloadEntries'];result['displayRemoved']=not Path('/tmp/.X11-unix/X117').exists();(out/'runner-result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
 if result.get('probeExit')!=0 or not result['normalProductExit'] or not result['originalPayloadUnchanged']:sys.exit(1)
