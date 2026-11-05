import os,subprocess,time,pathlib,json,signal
O=pathlib.Path(__file__).parent;S=O/'cinnamon-sysroot';r=O/'cinnamon-session-probe';r.mkdir(exist_ok=True)
env=os.environ.copy()
for name in ['home','config','data/applications','runtime','cache']:(r/name).mkdir(parents=True,exist_ok=True)
(r/'runtime').chmod(0o700)
env.update(DISPLAY=':176',HOME=str(r/'home'),XDG_CONFIG_HOME=str(r/'config'),XDG_DATA_HOME=str(r/'data'),XDG_RUNTIME_DIR=str(r/'runtime'),XDG_CACHE_HOME=str(r/'cache'),XDG_CURRENT_DESKTOP='X-Cinnamon',XDG_SESSION_TYPE='x11',XDG_SESSION_DESKTOP='cinnamon',DESKTOP_SESSION='cinnamon',PATH=str(S/'usr/bin')+':'+os.environ['PATH'],LD_LIBRARY_PATH=':'.join(str(S/'usr/lib/x86_64-linux-gnu'/x) for x in ['','cinnamon','muffin','cjs'])+':/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',GI_TYPELIB_PATH=':'.join(str(S/'usr/lib/x86_64-linux-gnu'/x) for x in ['girepository-1.0','cinnamon','muffin','cjs/girepository-1.0']),GSETTINGS_SCHEMA_DIR=str(S/'usr/share/glib-2.0/schemas'),XDG_DATA_DIRS=str(S/'usr/share')+':/usr/share',PYTHONPATH=str(S/'usr/lib/python3/dist-packages'),CINNAMON_DATADIR=str(S/'usr/share/cinnamon'),CINNAMON_JS=str(S/'usr/share/cinnamon/js'))
env['GI_TYPELIB_PATH']+=':'+str(S/'usr/lib/girepository-1.0')
# Preserve the vendor session components; only relocate absolute extracted paths.
for p in (S/'usr/share/applications').glob('*.desktop'):
 text=p.read_text(errors='replace')
 if '/usr/bin/' in text:(r/'data/applications'/p.name).write_text(text.replace('/usr/bin/',str(S/'usr/bin')+'/'))
subprocess.run(['/usr/lib/x86_64-linux-gnu/glib-2.0/glib-compile-schemas',str(S/'usr/share/glib-2.0/schemas')],check=True)
procs=[]
def start(args,log):
 p=subprocess.Popen(args,env=env,stdout=open(r/log,'w'),stderr=subprocess.STDOUT,start_new_session=True);procs.append(p);return p
try:
 start(['/workspace/.floorp-cloud/bin/Xvfb',':176','-screen','0','1280x1024x24','-nolisten','tcp','-dpi','96'],'xvfb.log');time.sleep(1)
 p=start([str(S/'usr/lib/x86_64-linux-gnu/cinnamon-session/cinnamon-session-binary'),'--session','cinnamon','--debug'],'session.log');time.sleep(25)
 result={'sessionPid':p.pid,'sessionAlive':p.poll() is None,'exitCode':p.poll()}
 for name,args in {'wm':['xprop','-root','_NET_SUPPORTING_WM_CHECK','_NET_CLIENT_LIST'],'screenshot':['import','-window','root',str(r/'session.png')],'processes':['ps','-eo','pid,ppid,comm']}.items():
  c=subprocess.run(args,env=env,capture_output=True,text=True,timeout=10);result[name]={'rc':c.returncode,'stdout':c.stdout,'stderr':c.stderr}
 (r/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k!='processes'}),flush=True)
finally:
 for p in reversed(procs):
  if p.poll() is None:
   try:os.killpg(p.pid,signal.SIGTERM);p.wait(5)
   except:pass
