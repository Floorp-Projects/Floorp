import os,sys,json,time,pathlib,subprocess,signal,http.server,threading,re
from marionette_driver.marionette import Marionette
B=pathlib.Path('/workspace/qa-linux-22/C');O=B/'KAN-71/activation-followup';S=B/'environment/sysroot'
product=sys.argv[1];wm=sys.argv[2] if len(sys.argv)>2 else 'muffin';r=O/(wm+'-'+product+('-audit' if '--audit' in sys.argv else ''));r.mkdir(exist_ok=True)
if wm=='cinnamon':S=O/'cinnamon-sysroot'
env=os.environ.copy();runtime=r/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700)
env.update(DISPLAY=':175',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_APP_REMOTINGNAME='qa-c71-gui-'+wm+'-'+product,HOME=str(r/'home'),XDG_RUNTIME_DIR=str(runtime),XDG_CONFIG_HOME=str(r/'config'),XDG_DATA_HOME=str(r/'data'),XDG_CACHE_HOME=str(r/'cache'),XDG_CURRENT_DESKTOP='X-Cinnamon',XDG_SESSION_TYPE='x11',PATH=str(S/'usr/bin')+':'+os.environ['PATH'],LD_LIBRARY_PATH=str(S/'usr/lib/x86_64-linux-gnu')+':'+str(S/'usr/lib/x86_64-linux-gnu/muffin')+':/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',GSETTINGS_SCHEMA_DIR=str(S/'usr/share/glib-2.0/schemas'),XDG_DATA_DIRS=str(S/'usr/share')+':/usr/share',GI_TYPELIB_PATH=str(S/'usr/lib/x86_64-linux-gnu/girepository-1.0'),CINNAMON_LIBDIR=str(S/'usr/lib/x86_64-linux-gnu/cinnamon'),CINNAMON_DATADIR=str(S/'usr/share/cinnamon'))
if wm=='cinnamon':
 env.update(XDG_SESSION_DESKTOP='cinnamon',DESKTOP_SESSION='cinnamon',XDG_CONFIG_DIRS=str(S/'etc/xdg')+':/etc/xdg',PYTHONPATH=str(S/'usr/lib/python3/dist-packages'),CINNAMON_JS=str(S/'usr/share/cinnamon/js'))
 env['LD_LIBRARY_PATH']+=':'+str(S/'usr/lib/x86_64-linux-gnu/cinnamon')+':'+str(S/'usr/lib/x86_64-linux-gnu/cjs')
 env['GI_TYPELIB_PATH']+=':'+':'.join(str(S/'usr/lib/x86_64-linux-gnu'/x) for x in ['cinnamon','muffin','cjs/girepository-1.0'])+':'+str(S/'usr/lib/girepository-1.0')
 for path in ['home/.config/autostart','config/autostart','data/dbus-1/services','data/applications']:(r/path).mkdir(parents=True,exist_ok=True)
 for original,target in [(S/'etc/xdg/autostart',r/'config/autostart'),(S/'usr/share/applications',r/'data/applications'),(S/'usr/share/dbus-1/services',r/'data/dbus-1/services')]:
  for f in original.glob('*'):
   if f.is_file() and f.suffix in ['.desktop','.service']:
    text=f.read_text(errors='replace');text='\n'.join(line.replace('/usr/',str(S/'usr')+'/') if line.startswith('Exec=') else line for line in text.splitlines())+'\n';(target/f.name).write_text(text)
 if '--bus-ready' not in sys.argv:os.execvpe('dbus-run-session',['dbus-run-session','--',sys.executable,__file__,product,wm,*sys.argv[3:],'--bus-ready'],env)
assert not any(k.startswith('MOZ_DISABLE_') and k.endswith('SANDBOX') for k in env)
for p in ['home','config','data/applications','profile']:(r/p).mkdir(parents=True,exist_ok=True)
profile=r/'profile';(profile/'user.js').write_text('user_pref("marionette.port",2875);\nuser_pref("browser.shell.checkDefaultBrowser",false);\n')
exe=B/'products'/product/product
desktop='[Desktop Entry]\nType=Application\nName=KAN71 QA Browser\nExec=/usr/bin/python3 '+str(O/'desktop_launcher.py')+' '+str(r/'launcher.jsonl')+' '+str(exe)+' '+str(profile)+' %u\nMimeType=x-scheme-handler/http;x-scheme-handler/https;\nStartupNotify=true\nStartupWMClass=qa-c71-gui-'+wm+'-'+product+'\n'
(r/'data/applications/qa-browser.desktop').write_text(desktop);(r/'config/mimeapps.list').write_text('[Default Applications]\nx-scheme-handler/http=qa-browser.desktop\nx-scheme-handler/https=qa-browser.desktop\n')
hits=[]
class H(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append({'path':self.path,'time':time.monotonic()});self.send_response(200);self.end_headers();self.wfile.write(b'<title>KAN71 GTK link received</title>Local activation fixture')
 def log_message(self,*a):pass
server=http.server.HTTPServer(('127.0.0.1',18775),H);threading.Thread(target=server.serve_forever,daemon=True).start()
children=[]
def start(args,name):
 p=subprocess.Popen(args,env=env,stdout=open(r/(name+'.log'),'w'),stderr=subprocess.STDOUT,start_new_session=True);children.append(p);return p
def cmd(args):
 p=subprocess.run(args,env=env,text=True,capture_output=True,timeout=20);return {'rc':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
result={'product':product,'windowManager':wm,'input':'synthetic XTest pointer click on independent native GTK LinkButton','tests':[]}
try:
 start(['/workspace/.floorp-cloud/bin/Xvfb',':175','-screen','0','1280x1024x24','-nolisten','tcp','-dpi','96'],'xvfb');time.sleep(1)
 if wm=='cinnamon':wp=start([str(S/'usr/lib/x86_64-linux-gnu/cinnamon-session/cinnamon-session-binary'),'--session','cinnamon'],'cinnamon-session')
 elif wm=='muffin':wp=start([str(S/'usr/bin/muffin'),'--x11','--replace','--mutter-plugin='+str(S/'usr/lib/x86_64-linux-gnu/muffin/plugins/libdefault.so')],'muffin')
 else:wp=start(['/usr/bin/xfwm4','--compositor=off'],'xfwm4')
 time.sleep(6)
 start(['/usr/bin/python3',str(O/'startup_monitor.py'),str(r/'startup-messages.jsonl')],'startup-monitor')
 browser=start([str(exe),'--profile',str(profile),'--marionette','about:blank'],'browser')
 for attempt in range(80):
  try:
   m=Marionette(host='127.0.0.1',port=2875,socket_timeout=30);m.start_session({'acceptInsecureCerts':False});break
  except Exception:time.sleep(.5)
 else:raise RuntimeError('browser observation session unavailable')
 m.set_context('chrome');result['version']=m.execute_script('return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,parentPid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level")};')
 for n in range(1 if '--audit' in sys.argv else 2):
  marker='/'+wm+'-'+product+'-'+str(n);eventfile=r/('click-'+str(n)+'.json')
  gui=start(['/usr/bin/python3',str(O/'gui_link.py'),'http://127.0.0.1:18775'+marker,str(eventfile)],'gui-'+str(n));time.sleep(2)
  search=cmd(['/workspace/.floorp-cloud/bin/xdotool','search','--onlyvisible','--name','KAN71 Independent GTK Link App']);wid=search['stdout'].splitlines()[-1]
  # Click physically positioned GUI link; no browser raise/focus call after this.
  cmd(['/workspace/.floorp-cloud/bin/xdotool','windowactivate','--sync',wid]);time.sleep(.3)
  before=cmd(['xprop','-root','_NET_ACTIVE_WINDOW']);geo=cmd(['/workspace/.floorp-cloud/bin/xdotool','getwindowgeometry','--shell',wid]);g=dict(line.split('=',1) for line in geo['stdout'].splitlines() if '=' in line)
  point=json.loads(eventfile.with_suffix('.ready.json').read_text())
  cmd(['/workspace/.floorp-cloud/bin/xdotool','mousemove','--sync',str(point['x']),str(point['y']),'click','1']);time.sleep(5)
  after=cmd(['xprop','-root','_NET_ACTIVE_WINDOW']);windows=cmd(['/workspace/.floorp-cloud/bin/xdotool','search','--onlyvisible','--class','qa-c71-gui-'+wm+'-'+product]);match=re.search(r'0x[0-9a-fA-F]+',after['stdout']);active=int(match.group(),16) if match else None
  launches=[json.loads(x) for x in (r/'launcher.jsonl').read_text().splitlines()] if (r/'launcher.jsonl').exists() else []
  startup=[json.loads(x) for x in (r/'startup-messages.jsonl').read_text().splitlines()] if (r/'startup-messages.jsonl').exists() else []
  result['tests'].append({'trial':n,'event':json.loads(eventfile.read_text()) if eventfile.exists() else None,'launcher':launches[-1] if launches else None,'startupMessages':startup,'before':before,'after':after,'browserWindowIds':windows['stdout'].splitlines(),'focusedBrowser':active in [int(x) for x in windows['stdout'].splitlines()],'requestReceived':any(h['path']==marker for h in hits),'originalParentAlive':browser.poll() is None,'windowManagerAlive':wp.poll() is None})
  cmd(['import','-window','root',str(r/('after-'+str(n)+'.png'))]);gui.terminate();gui.wait(5)
 result['hits']=hits
 result['end']=m.execute_script('return {parentPid:Services.appinfo.processID,urls:Array.from(gBrowser.tabs).map(t=>t.linkedBrowser.currentURI.spec)};');m.delete_session()
 result['rootProperties']=cmd(['xprop','-root','_NET_SUPPORTING_WM_CHECK','_NET_SUPPORTED'])
except Exception as e:result['error']=repr(e)
finally:
 for p in reversed(children):
  if p.poll() is None:
   try:os.killpg(p.pid,signal.SIGTERM);p.wait(5)
   except:pass
 server.shutdown();(r/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result),flush=True)
