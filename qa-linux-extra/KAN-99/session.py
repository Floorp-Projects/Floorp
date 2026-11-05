import os,pathlib,json,time,subprocess,urllib.parse,threading,http.server,traceback,hashlib
from marionette_driver.marionette import Marionette
P=pathlib.Path('/workspace/qa-linux-22/C-extra/KAN-99'); profile=P/'profile';profile.mkdir(exist_ok=True)
requests=[]
class H(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  requests.append({'path':self.path,'time':time.time()});(P/'requests.json').write_text(json.dumps(requests,indent=2));self.send_response(200);self.send_header('Content-Type','text/html' if not self.path.startswith('/network-ad') else 'image/svg+xml');self.end_headers();self.wfile.write((('<svg xmlns="http://www.w3.org/2000/svg" width="100" height="50"><rect width="100" height="50" fill="red"/></svg>') if self.path.startswith('/network-ad') else '<!doctype html><title>KAN99 signed uBO fixture</title><h1>Signed uBO fixture</h1><div id="qa-sponsored">SPONSORED cosmetic target</div><div class="ad-banner">generic ad-banner target</div><p id="qa-keep">Visible control</p><img src="/network-ad.svg?'+self.path.strip('/')+'"><button id="picker-target">Pick this element</button>').encode())
 def log_message(self,*a):pass
s=http.server.ThreadingHTTPServer(('127.0.0.1',18799),H);threading.Thread(target=s.serve_forever,daemon=True).start()
env=os.environ.copy();assert not any('SANDBOX' in k and k.startswith('MOZ_DISABLE_') for k in env)
env.update(DISPLAY=':207',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',HOME=str(P/'home'),XDG_RUNTIME_DIR=str(P/'runtime'));pathlib.Path(env['HOME']).mkdir(exist_ok=True);pathlib.Path(env['XDG_RUNTIME_DIR']).mkdir(exist_ok=True);pathlib.Path(env['XDG_RUNTIME_DIR']).chmod(0o700)
proxy=urllib.parse.urlparse(os.environ['HTTPS_PROXY']);prefs={'marionette.port':28799,'network.proxy.type':1,'network.proxy.http':proxy.hostname,'network.proxy.http_port':proxy.port,'network.proxy.ssl':proxy.hostname,'network.proxy.ssl_port':proxy.port,'network.proxy.no_proxies_on':'localhost,127.0.0.1','browser.shell.checkDefaultBrowser':False}
(profile/'user.js').write_text('\n'.join('user_pref(%s,%s);'%(json.dumps(k),json.dumps(v)) for k,v in prefs.items()))

if not (profile/'cert9.db').exists():subprocess.run(['certutil','-N','--empty-password','-d','sql:'+str(profile)],check=True)
subprocess.run(['certutil','-A','-d','sql:'+str(profile),'-n','environment-proxy-ca','-t','C,,','-i','/usr/local/share/ca-certificates/environment-proxy-ca.crt'],check=True)
x=subprocess.Popen(['/workspace/.floorp-cloud/bin/Xvfb',':207','-screen','0','1400x1000x24','-nolisten','tcp'],env=env,stdout=open(P/'xvfb.log','w'),stderr=subprocess.STDOUT);time.sleep(1)
b=subprocess.Popen(['/workspace/qa-linux-22/C/products/floorp/floorp','--no-remote','--profile',str(profile),'--marionette','about:blank'],env=env,stdout=open(P/'browser.log','w'),stderr=subprocess.STDOUT)
try:
 time.sleep(5);m=Marionette(host='127.0.0.1',port=28799,socket_timeout=60);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome');m.timeout.script=90
 (P/'ready.json').write_text(json.dumps(m.execute_script('return {version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required"),panel:!!window.gPanelSidebar,globals:Object.keys(window).filter(x=>/PanelSidebar|Panel/.test(x)).slice(0,50)};'),indent=2))
 last=0
 while True:
  c=P/'command.json'
  if c.exists() and c.stat().st_mtime_ns!=last:
   last=c.stat().st_mtime_ns;d=json.loads(c.read_text())
   if d.get('stop'):break
   try:
    
    if d.get('select') is not None:m.switch_to_window(m.window_handles[d['select']])
    if d.get('screenshot'):(P/d['screenshot']).write_bytes(__import__('base64').b64decode(m.screenshot()))
    m.set_context(d.get('context','chrome'));r=m.execute_async_script(d['script']) if d.get('async') else m.execute_script(d['script']);(P/('result-'+d['id']+'.json')).write_text(json.dumps(r,indent=2));print(d['id'],'done',flush=True)
   except Exception as e:(P/('result-'+d['id']+'.json')).write_text(json.dumps({'error':str(e),'trace':traceback.format_exc()},indent=2));print(d['id'],'error',flush=True)
  time.sleep(.3)
finally:
 b.terminate();x.terminate();s.shutdown()
