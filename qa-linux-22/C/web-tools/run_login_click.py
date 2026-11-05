import base64,os,sys,json,time,pathlib,subprocess,signal,urllib.parse,threading,http.server,socket
from marionette_driver.marionette import Marionette
BASE=pathlib.Path('/workspace/qa-linux-22/C'); TOOLS=BASE/'web-tools'; PRODUCTS=BASE/'products'
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write(b'<title>QA local fixture</title><h1>Browser content process control</h1>')
 def log_message(self,*a): pass
server=http.server.HTTPServer(('127.0.0.1',18736),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
env=os.environ.copy(); env.update(MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',DISPLAY=':126',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu')
assert not any(k.startswith('MOZ_DISABLE_') and k.endswith('SANDBOX') for k in env),'sandbox disabling env present'
xlog=open(TOOLS/'xvfb.log','w'); x=subprocess.Popen(['/workspace/.floorp-cloud/bin/Xvfb',':126','-screen','0','1366x1000x24','-nolisten','tcp'],env=env,stdout=xlog,stderr=xlog)
time.sleep(1)
allresults=[]
try:
 for idx,name in enumerate(['floorp','firefox']):
  root=TOOLS/(name+'-login-click');root.mkdir(exist_ok=True);profile=root/('profile-'+str(int(time.time())));profile.mkdir(exist_ok=True);home=root/'home';home.mkdir(exist_ok=True);runtime=root/'runtime';runtime.mkdir(exist_ok=True);runtime.chmod(0o700)
  proxy=urllib.parse.urlparse(os.environ['HTTPS_PROXY']);port=proxy.port or 8080
  prefs={'marionette.port':2851+idx,'network.proxy.type':1,'network.proxy.http':proxy.hostname,'network.proxy.http_port':port,'network.proxy.ssl':proxy.hostname,'network.proxy.ssl_port':port,'network.proxy.no_proxies_on':'localhost,127.0.0.1','browser.shell.checkDefaultBrowser':False}
  (profile/'user.js').write_text('\n'.join('user_pref(%s, %s);'%(json.dumps(k),json.dumps(v)) for k,v in prefs.items()))
  subprocess.run(['certutil','-N','--empty-password','-d','sql:'+str(profile)],check=True,stdin=subprocess.DEVNULL,timeout=10)
  for ca in ['mitmproxy-ca-cert','nebula-mitmproxy','environment-proxy-ca']:
   subprocess.run(['certutil','-A','-d','sql:'+str(profile),'-n',ca,'-t','C,,','-i',('/usr/local/share/ca-certificates/environment-proxy-ca.crt' if ca=='environment-proxy-ca' else '/etc/ssl/certs/'+ca+'.pem')],check=True)
  benv=env|{'HOME':str(home),'XDG_RUNTIME_DIR':str(runtime),'XDG_CONFIG_HOME':str(root/'config'),'XDG_CACHE_HOME':str(root/'cache')}
  log=open(root/'browser.log','w');proc=subprocess.Popen([str(PRODUCTS/name/name),'--no-remote','--profile',str(profile),'--marionette','-remote-allow-system-access','about:blank'],env=benv,stdout=log,stderr=log,start_new_session=True)
  try:

   time.sleep(4)
   m=Marionette(host='127.0.0.1',port=2851+idx,socket_timeout=40);m.start_session({'acceptInsecureCerts':False});m.timeout.page_load=25
   m.set_context('chrome')
   version=m.execute_script('return {name:Services.appinfo.name,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,ua:navigator.userAgent,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),tracking:Services.prefs.getBoolPref("privacy.trackingprotection.enabled"),cookieBehavior:Services.prefs.getIntPref("network.cookie.cookieBehavior")};')
   m.set_context('content');m.navigate('http://127.0.0.1:18736/control')
   control=m.execute_script('return {title:document.title,text:document.body.innerText,url:location.href};')
   (root/'version-control.json').write_text(json.dumps({'version':version,'control':control},indent=2));print(name,'control',control,flush=True)
   for private in [False]:
    if private:
     m.set_context('chrome');m.execute_script('window.OpenBrowserWindow({private:true});');time.sleep(2);m.switch_to_window(m.window_handles[-1]);m.set_context('content')
    m.set_context('chrome')
    m.execute_script('''window.__qaNav=[];window.__qaObs={observe(subject,topic){try {const c=subject.QueryInterface(Ci.nsIHttpChannel);const u=new URL(c.URI.spec);if(c.loadInfo.isTopLevelLoad)window.__qaNav.push({time:Date.now(),topic,url:(u.origin+(u.pathname.includes("/challenge-platform/")?"/cdn-cgi/challenge-platform/[redacted]":u.pathname)),status:topic==="http-on-examine-response"?c.responseStatus:null});}catch(e){}}};for(const t of ["http-on-modify-request","http-on-examine-response"])Services.obs.addObserver(window.__qaObs,t);''')
    for key,url,label in [('KAN-21','https://torbox.app/login','torbox-login-click')]:
     out=BASE/key/(name+('-private' if private else '-normal')+'-'+label); out.mkdir(exist_ok=True)
     m.set_context('chrome');m.execute_script('window.__qaNav=[];');m.set_context('content');err=None
     try: m.navigate(url)
     except Exception as e: err=type(e).__name__+': '+str(e)[:500]
     time.sleep(15)
     (out/'before-click.png').write_bytes(base64.b64decode(m.screenshot()))
     point=m.execute_script('const e=document.querySelector("input[type=password]");if(!e)return null;const r=e.getBoundingClientRect();const x=Math.round(r.x+22),y=Math.round(r.bottom+71);const hit=document.elementFromPoint(x,y);return {x,y,hit:hit?.tagName,hitClass:hit?.className};')
     if point:
      m.actions.perform([{'type':'pointer','id':'qa-mouse','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','duration':0,'x':point['x'],'y':point['y'],'origin':'viewport'},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]);m.actions.release()
     (out/'interaction.json').write_text(json.dumps({'kind':'single synthetic normal checkbox click, no retry, no credentials, no form submit','point':point},indent=2))
     states=[]
     for t in range(6):
      time.sleep(5)
      try:
       states.append(m.execute_script('return {time:Date.now(),title:document.title,url:location.origin+location.pathname,ready:document.readyState,text:document.body?.innerText.slice(0,2500),frames:Array.from(document.querySelectorAll("iframe")).map(f=>({title:f.title,src:(()=>{try{let u=new URL(f.src);return u.origin+u.pathname}catch{return f.src}})()})),turnstileResponsePresent:!!document.querySelector("[name=cf-turnstile-response]")?.value,webdriver:navigator.webdriver};'))
      except Exception as e: states.append({'error':type(e).__name__+': '+str(e)[:300]})
     try: (out/'screen.png').write_bytes(base64.b64decode(m.screenshot()))
     except Exception as e: print('screenshot',e,flush=True)
     m.set_context('chrome');nav=m.execute_script('return window.__qaNav;');m.set_context('content')
     result={'product':name,'private':private,'url':url,'version':version,'control':control,'navigationError':err,'samples':states,'navigation':nav,'environment':'Debian 13 Xvfb X11, automated Marionette, Cloud proxy/IP, isolated fresh profile, default protection prefs; proxy CA explicitly imported; TLS validation enabled'}
     (out/'result.json').write_text(json.dumps(result,indent=2));allresults.append(result);print(name,private,label,'done',states[-1].get('title'),len(nav),flush=True)
    m.set_context('chrome');m.execute_script('for(const t of ["http-on-modify-request","http-on-examine-response"])Services.obs.removeObserver(window.__qaObs,t);');m.set_context('content')
   m.delete_session()
  except Exception as e:
   print(name,'ERROR',repr(e),flush=True);(root/'error.txt').write_text(repr(e))
  finally:
   os.killpg(proc.pid,signal.SIGTERM)
   try:proc.wait(8)
   except:os.killpg(proc.pid,signal.SIGKILL)
finally:
 x.terminate();server.shutdown();(TOOLS/'login-click-results.json').write_text(json.dumps(allresults,indent=2))
