import os,sys,json,time,pathlib,subprocess,signal,socket,threading,http.server,re
from marionette_driver.marionette import Marionette
B=pathlib.Path('/workspace/qa-linux-22/C'); O=B/'KAN-71'; S=B/'environment/sysroot'; O.mkdir(exist_ok=True)
hits=[]
class H(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append({'path':self.path,'time':time.time()});self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write(b'<title>External link fixture</title><h1>Local external link fixture</h1>')
 def log_message(self,*a):pass
srv=http.server.HTTPServer(('127.0.0.1',18771),H);threading.Thread(target=srv.serve_forever,daemon=True).start()
def run(cmd,env):
 p=subprocess.run(cmd,env=env,text=True,capture_output=True,timeout=20);return {'rc':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
results=[]
for wi,wm in enumerate(['xfwm4','muffin']):
 for pi,product in enumerate(['floorp','firefox']):
  r=O/(wm+'-'+product);r.mkdir(exist_ok=True);home=r/'home';home.mkdir(exist_ok=True);profile=r/'profile';profile.mkdir(exist_ok=True)
  runtime=r/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700);config=r/'config';data=r/'data';(data/'applications').mkdir(parents=True,exist_ok=True)
  port=2871;display=':'+str(171+wi*2+pi);env=os.environ.copy();env.update(MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',DISPLAY=display,HOME=str(home),XDG_RUNTIME_DIR=str(runtime),XDG_CONFIG_HOME=str(config),XDG_DATA_HOME=str(data),XDG_CACHE_HOME=str(r/'cache'),XDG_CURRENT_DESKTOP='X-Cinnamon' if wm=='muffin' else 'X-Cinnamon',XDG_SESSION_TYPE='x11',PATH=str(S/'usr/bin')+':'+os.environ['PATH'],MOZ_APP_REMOTINGNAME='qa-c71-'+wm+'-'+product,LD_LIBRARY_PATH=str(S/'usr/lib/x86_64-linux-gnu')+':'+str(S/'usr/lib/x86_64-linux-gnu/muffin')+':/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',GSETTINGS_SCHEMA_DIR=str(S/'usr/share/glib-2.0/schemas'))
  assert not any(k.startswith('MOZ_DISABLE_') and k.endswith('SANDBOX') for k in env)
  (profile/'user.js').write_text('user_pref("marionette.port",2871);\nuser_pref("browser.shell.checkDefaultBrowser",false);\n')
  exe=B/'products'/product/product
  wrapper=r/'launcher.sh';wrapper.write_text('#!/bin/sh\nprintf "startup_id_present=%s activation_token_present=%s\\n" "${DESKTOP_STARTUP_ID:+yes}" "${XDG_ACTIVATION_TOKEN:+yes}" >> "'+str(r/'launcher.log')+'"\nexec "'+str(exe)+'" --profile "'+str(profile)+'" "$@"\n');wrapper.chmod(0o755)
  (data/'applications/qa-browser.desktop').write_text('[Desktop Entry]\nType=Application\nName=QA Browser\nExec='+str(wrapper)+' %u\nMimeType=x-scheme-handler/http;x-scheme-handler/https;\nStartupNotify=true\n')
  config.mkdir(exist_ok=True);(config/'mimeapps.list').write_text('[Default Applications]\nx-scheme-handler/http=qa-browser.desktop\nx-scheme-handler/https=qa-browser.desktop\n')
  procs=[]
  def start(cmd,name):
   log=open(r/(name+'.log'),'w');p=subprocess.Popen(cmd,env=env,stdout=log,stderr=log,start_new_session=True);procs.append(p);return p
  result={'product':product,'windowManager':wm,'environment':'Debian13 Xvfb X11 isolated DBus session; synthetic input, not physical; Muffin standalone is not full Cinnamon/Linux Mint','tests':[]}
  try:
   x=start(['/workspace/.floorp-cloud/bin/Xvfb',display,'-screen','0','1366x1000x24','-nolisten','tcp'],'xvfb');time.sleep(1)
   wp=start(['/usr/bin/xfwm4','--compositor=off'] if wm=='xfwm4' else [str(S/'usr/bin/muffin'),'--x11','--replace','--mutter-plugin='+str(S/'usr/lib/x86_64-linux-gnu/muffin/plugins/libdefault.so')],'wm');time.sleep(2)
   result['wmInitial']=run(['xprop','-root','_NET_SUPPORTING_WM_CHECK','_NET_SUPPORTED'],env)
   p=start([str(exe),'--profile',str(profile),'--marionette','--remote-allow-system-access','about:blank'],'browser')
   for a in range(80):
    try:
     m=Marionette(host='127.0.0.1',port=port,socket_timeout=30);m.start_session({'acceptInsecureCerts':False});break
    except Exception:time.sleep(.5)
   else:raise RuntimeError('Marionette not available')
   m.set_context('chrome');result['version']=m.execute_script('return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level")};')
   term=start([str(S/'usr/bin/xterm'),'-title','QA source terminal'],'terminal');time.sleep(2)
   ids=run(['/workspace/.floorp-cloud/bin/xdotool','search','--name','QA source terminal'],env)['stdout'].splitlines();tid=ids[-1]
   for mode in ['shell','synthetic-terminal']:
    run(['/workspace/.floorp-cloud/bin/xdotool','windowactivate','--sync',tid],env);time.sleep(1)
    marker='/'+wm+'-'+product+'-'+mode;url='http://127.0.0.1:18771'+marker
    before=run(['xprop','-root','_NET_ACTIVE_WINDOW'],env)
    if mode=='shell':inv=run(['xdg-open',url],env)
    else:
     inv=run(['/workspace/.floorp-cloud/bin/xdotool','type','--clearmodifiers','xdg-open '+url],env);run(['/workspace/.floorp-cloud/bin/xdotool','key','--clearmodifiers','Return'],env)
    time.sleep(5);after=run(['xprop','-root','_NET_ACTIVE_WINDOW'],env);windows=run(['/workspace/.floorp-cloud/bin/xdotool','search','--onlyvisible','--class',product],env)
    props=[]
    for wid in windows['stdout'].splitlines():props.append({'id':wid,'props':run(['xprop','-id',wid,'WM_CLASS','WM_NAME','WM_HINTS','_NET_WM_USER_TIME','_NET_WM_STATE'],env)})
    activeMatch=re.search(r'0x[0-9a-fA-F]+',after['stdout']);active=int(activeMatch.group(),16) if activeMatch else None
    result['tests'].append({'mode':mode,'before':before,'after':after,'invocation':inv,'requestReceived':any(h['path']==marker for h in hits),'originalProcessAlive':p.poll() is None,'browserWindows':props,'focusedBrowser':active in [int(v) for v in windows['stdout'].splitlines()]})
    run(['import','-window','root',str(r/(mode+'.png'))],env)
   m.delete_session()
  except Exception as e:result['error']=repr(e)
  finally:
   for p in reversed(procs):
    try:os.killpg(p.pid,signal.SIGTERM);p.wait(5)
    except:pass
   (r/'result.json').write_text(json.dumps(result,indent=2));results.append(result);print(json.dumps(result),flush=True)
(O/'results.json').write_text(json.dumps(results,indent=2));srv.shutdown()
