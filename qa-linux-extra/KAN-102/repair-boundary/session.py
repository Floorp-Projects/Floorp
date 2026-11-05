import os,pathlib,json,time,subprocess,urllib.parse,threading,http.server,traceback,argparse,socket
from marionette_driver.marionette import Marionette
p=argparse.ArgumentParser();p.add_argument('--out',type=pathlib.Path,required=True);p.add_argument('--binary',required=True);p.add_argument('--display',type=int,required=True);p.add_argument('--port',type=int,required=True);p.add_argument('--http-port',type=int,required=True);p.add_argument('--ws',choices=['on','off'],default='on');a=p.parse_args();P=a.out;P.mkdir(parents=True,exist_ok=False);profile=P/'profile';profile.mkdir();children=[]
class H(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write(('<!doctype html><title>STG QA '+self.path+'</title><h1>STG QA '+self.path+'</h1>').encode())
 def log_message(self,*a):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',a.http_port),H);threading.Thread(target=server.serve_forever,daemon=True).start()
env=os.environ.copy();assert not any(k.startswith('MOZ_DISABLE_') and 'SANDBOX' in k and v for k,v in env.items());env.update(DISPLAY=f':{a.display}',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',HOME=str(P/'home'),XDG_RUNTIME_DIR=str(P/'runtime'),XDG_DATA_DIRS='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/share:/usr/share',XDG_CONFIG_DIRS='/workspace/qa-linux-22/C/KAN-90/sysroot/etc/xdg');pathlib.Path(env['HOME']).mkdir();pathlib.Path(env['XDG_RUNTIME_DIR']).mkdir(mode=0o700)
proxy=urllib.parse.urlparse(os.environ['HTTPS_PROXY']);prefs={'marionette.port':a.port,'network.proxy.type':1,'network.proxy.http':proxy.hostname,'network.proxy.http_port':proxy.port,'network.proxy.ssl':proxy.hostname,'network.proxy.ssl_port':proxy.port,'network.proxy.no_proxies_on':'localhost,127.0.0.1','browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,'browser.startup.page':3,'xpinstall.signatures.required':True,'floorp.workspaces.enabled':a.ws=='on'}
(profile/'user.js').write_text('\n'.join('user_pref(%s,%s);'%(json.dumps(k),json.dumps(v)) for k,v in prefs.items()));subprocess.run(['certutil','-N','--empty-password','-d','sql:'+str(profile)],check=True);subprocess.run(['certutil','-A','-d','sql:'+str(profile),'-n','environment-proxy-ca','-t','C,,','-i','/usr/local/share/ca-certificates/environment-proxy-ca.crt'],check=True)
def launch(name,args):
 q=subprocess.Popen(args,env=env,stdout=open(P/(name+'.log'),'w'),stderr=subprocess.STDOUT);children.append(q);return q
try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',f':{a.display}','-screen','0','1440x1000x24','-nolisten','tcp']);time.sleep(1);wm=launch('openbox',['/workspace/qa-linux-22/C/KAN-90/sysroot/usr/bin/openbox','--sm-disable']);time.sleep(1);assert wm.poll() is None
 b=launch('browser',[a.binary,'--no-remote','--profile',str(profile),'--marionette','about:blank']);m=Marionette(host='127.0.0.1',port=a.port,socket_timeout=60);m.raise_for_port(timeout=45);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome');m.timeout.script=60;time.sleep(10)
 (P/'ready.json').write_text(json.dumps(m.execute_script('return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,pid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),workspaceEnabled:Services.prefs.getBoolPref("floorp.workspaces.enabled"),workspaceAPI:!!window.workspacesFuncs};'),indent=2));last=0
 while True:
  c=P/'command.json'
  if c.exists() and c.stat().st_mtime_ns!=last:
   last=c.stat().st_mtime_ns
   try:d=json.loads(c.read_text())
   except json.JSONDecodeError:continue
   if d.get('stop'):
    m.set_context('chrome');m.execute_script('setTimeout(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit),100);return true;');(P/'shutdown.json').write_text(json.dumps({'exit':b.wait(timeout=20)}));break
   try:
    if d.get('uri'):
     m.set_context('content')
     for h in m.window_handles:
      m.switch_to_window(h);m.set_context('content')
      href=m.execute_script('return location.href;')
      if isinstance(href,str) and d['uri'] in href:break
     else:raise Exception('requested tab not found')
    m.set_context(d.get('context','chrome'))
    if d.get('click'):m.find_element('css selector',d['click']).click()
    if d.get('restart'):
     m.set_context('chrome');m.execute_script('setTimeout(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit),100);return true;');old_exit=b.wait(timeout=20);assert old_exit==0
     b=launch('browser-restart',[a.binary,'--no-remote','--profile',str(profile),'--marionette']);m=Marionette(host='127.0.0.1',port=a.port,socket_timeout=60);m.raise_for_port(timeout=45);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome');m.timeout.script=60;time.sleep(10)
    if d.get('shot'):
     os.environ['DISPLAY']=env['DISPLAY'];import sys;sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages');from PIL import ImageGrab;ImageGrab.grab(xdisplay=env['DISPLAY']).save(P/d['shot'])
    result=m.execute_async_script(d['script']) if d.get('async') else m.execute_script(d.get('script','return true;'));(P/('result-'+d['id']+'.json')).write_text(json.dumps(result,indent=2));print(d['id'],'done',flush=True)
   except Exception as e:(P/('result-'+d['id']+'.json')).write_text(json.dumps({'error':str(e),'trace':traceback.format_exc()},indent=2));print(d['id'],'error',flush=True)
  time.sleep(.2)
finally:
 for q in reversed(children):
  if q.poll() is None:q.terminate()
 for q in children:
  try:q.wait(timeout=5)
  except subprocess.TimeoutExpired:q.kill();q.wait()
 server.shutdown()
