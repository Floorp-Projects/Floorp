#!/usr/bin/env python3
import argparse,pathlib,os,json,time,subprocess,threading,traceback,urllib.parse,http.server,sys
from marionette_driver.marionette import Marionette
R=pathlib.Path('/workspace/qa-linux-22/C-extra/KAN-37');p=argparse.ArgumentParser();p.add_argument('--product',type=pathlib.Path,required=True);p.add_argument('--out',type=pathlib.Path,required=True);p.add_argument('--profile',type=pathlib.Path,required=True);p.add_argument('--reuse-profile',action='store_true');p.add_argument('--display',type=int,required=True);p.add_argument('--port',type=int,required=True);p.add_argument('--http-port',type=int,required=True);p.add_argument('--fixture',choices=['none','prompt','update','confirmed'],default='none');p.add_argument('--display-version');p.add_argument('--prior-version',default='155.0');p.add_argument('--uri');a=p.parse_args();out=a.out.resolve();out.mkdir(parents=True,exist_ok=False);profile=a.profile.resolve();assert profile.is_relative_to(R.resolve()),'only task-owned profile allowed';profile.mkdir(parents=True,exist_ok=a.reuse_profile)
def save(path,data):
 tmp=path.with_name(path.name+'.pending');tmp.write_text(json.dumps(data,indent=2)+'\n');tmp.replace(path)
proxy=urllib.parse.urlparse(os.environ['HTTPS_PROXY']);prefs={'remote.prefs.recommended':False,'marionette.port':a.port,'network.proxy.type':1,'network.proxy.http':proxy.hostname,'network.proxy.http_port':proxy.port,'network.proxy.ssl':proxy.hostname,'network.proxy.ssl_port':proxy.port,'network.proxy.no_proxies_on':'localhost,127.0.0.1'};fixture={}
if a.fixture!='none':
 assert a.display_version
 version=a.display_version if a.fixture=='prompt' else a.prior_version
 fixture={'browser.startup.homepage_override.mstone':version,'floorp.startup.oldVersion':version,'floorp.releaseNotes.audience':'existing','floorp.releaseNotes.choiceConfirmed':a.fixture=='confirmed','floorp.releaseNotes.promptShown':False}
 if a.fixture=='confirmed':fixture['floorp.releaseNotes.mode']='disabled'
 prefs.update(fixture)
(profile/'user.js').write_text('\n'.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');' for k,v in prefs.items())+'\n');save(out/'test-definition.json',{'fixture':a.fixture,'fixturePrefs':fixture,'uri':a.uri,'profileReused':a.reuse_profile,'product':str(a.product),'noSecurityPreferenceOverrides':True,'network':'inherited proxy + configured CA; no insecure TLS flag'})
if not (profile/'cert9.db').exists():subprocess.run(['certutil','-N','--empty-password','-d','sql:'+str(profile)],check=True)
subprocess.run(['certutil','-A','-d','sql:'+str(profile),'-n','environment-proxy-ca','-t','C,,','-i','/usr/local/share/ca-certificates/environment-proxy-ca.crt'],check=True)
env=dict(os.environ)
assert not any(k.startswith('MOZ_DISABLE_') and 'SANDBOX' in k and v for k,v in env.items())
home=out/'home';home.mkdir();runtime=out/'runtime';runtime.mkdir(mode=0o700);env.update(HOME=str(home),XDG_CONFIG_HOME=str(home/'config'),XDG_DATA_HOME=str(home/'data'),XDG_CACHE_HOME=str(home/'cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=':'+str(a.display),MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',LD_LIBRARY_PATH=str(a.product)+':/workspace/qa-linux-22/C/KAN-90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',XDG_DATA_DIRS='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/share:/usr/share',XDG_CONFIG_DIRS='/workspace/qa-linux-22/C/KAN-90/sysroot/etc/xdg')
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):self.send_response(200);self.send_header('Content-Type','text/html');self.end_headers();self.wfile.write(b'<!doctype html><title>KAN86 local response control</title><h1 id="local-control">Native browser local response control</h1>')
 def log_message(self,*a):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',a.http_port),Handler);threading.Thread(target=server.serve_forever,daemon=True).start();children=[];stop=threading.Event();samples=[];phase='launch';b=None

def launch(name,args):
 q=subprocess.Popen(args,env=env,stdout=open(out/(name+'.log'),'w'),stderr=subprocess.STDOUT);children.append(q);return q

def monitor():
 previous={};hz=os.sysconf('SC_CLK_TCK');last=time.monotonic()
 while not stop.wait(.5):
  rows={}
  for path in pathlib.Path('/proc').iterdir():
   if not path.name.isdigit():continue
   try:
    text=(path/'stat').read_text();comm=text.split('(',1)[1].rsplit(')',1)[0];s=text.rsplit(')',1)[1].split();rows[int(path.name)]={'pid':int(path.name),'comm':comm,'state':s[0],'ppid':int(s[1]),'ticks':int(s[11])+int(s[12]),'threads':int(s[17]),'start':int(s[19]),'rssPages':int(s[21])}
   except (FileNotFoundError,ProcessLookupError,PermissionError,ValueError):pass
  owned={b.pid} if b else set();changed=True
  while changed:
   more={pid for pid,v in rows.items() if v['ppid'] in owned}-owned;changed=bool(more);owned|=more
  now=time.monotonic();data=[rows[pid] for pid in sorted(owned) if pid in rows];ticks=sum(max(0,v['ticks']-previous.get((v['pid'],v['start']),v['ticks'])) for v in data);samples.append({'timeUnix':time.time(),'phase':phase,'intervalSeconds':now-last,'cpuPercentSum':ticks/hz/max(.001,now-last)*100,'processes':data});previous={(v['pid'],v['start']):v['ticks'] for v in data};last=now
  if len(samples)%4==0:save(out/'cpu-samples.json',samples)

statusJS="""const p=Services.prefs;const read=(b,k)=>{const t=b.getPrefType(k);try{return t===32?b.getStringPref(k):t===64?b.getIntPref(k):t===128?b.getBoolPref(k):null}catch(e){return {unavailable:true,type:t,result:e.result}}};const pref=k=>({value:read(p,k),defaultValue:read(p.getDefaultBranch(''),k),user:p.prefHasUserValue(k)});let native={};try{const {AboutNewTab:A}=ChromeUtils.importESModule('resource:///modules/AboutNewTab.sys.mjs');native={initialized:A.initialized,activityStreamPresent:!!A.activityStream,activityStreamInitialized:A.activityStream?.initialized,parentAppInitialized:A.activityStream?.store?.getState()?.App?.initialized,feedKeys:Array.from(A.activityStream?.store?.feeds?.keys?.()||[])};}catch(e){native={error:String(e)}}let design={};try{const d=JSON.parse(p.getStringPref('floorp.design.configs'));design={design:d.globalConfigs?.userInterface,disableFloorpStart:d.uiCustomization?.disableFloorpStart};}catch{}const names=['remote.prefs.recommended','browser.newtabpage.activity-stream.testing.shouldInitializeFeeds','termsofuse.bypassNotification','termsofuse.acceptedVersion','termsofuse.currentVersion','termsofuse.minimumVersion','termsofuse.acceptedDate','browser.termsofuse.prefMigrationCheck','browser.newtabpage.activity-stream.selfLoading.enabled','browser.newtabpage.activity-stream.remote-renderer.enabled','browser.newtabpage.activity-stream.debug','browser.newtabpage.enabled','browser.newtabpage.activity-stream.enabled','browser.newtabpage.activity-stream.feeds.system.topstories','browser.newtabpage.activity-stream.feeds.system.topsites','browser.newtabpage.activity-stream.feeds.newtabinit','browser.newtabpage.activity-stream.feeds.telemetry','browser.newtabpage.activity-stream.newtabWallpapers.enabled','browser.newtabpage.activity-stream.newtabWallpapers.v2.enabled','browser.termsofuse.bypassNotification','browser.termsofuse.prefMigration.completed','browser.termsofuse.prefMigration.enabled','browser.termsofuse.termsAcceptedVersion','browser.termsofuse.termsAcceptedDate','browser.termsofuse.currentDate','browser.termsofuse.currentVersion','datareporting.policy.dataSubmissionPolicyAcceptedVersion','datareporting.policy.dataSubmissionPolicyNotifiedTime','distribution.id'];const prefs=Object.fromEntries(names.map(k=>[k,pref(k)]));let policies={};try{policies={status:Services.policies.status,activeKeys:Object.keys((Services.policies.getActivePolicies()||{})),termsOfUse:Services.policies.getActivePolicies()?.TermsOfUse??null};}catch(e){policies={error:String(e)}}return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,native,prefs,policies,design,protections:{sandbox:p.getIntPref('security.sandbox.content.level'),sandboxUser:p.prefHasUserValue('security.sandbox.content.level'),signatureRequired:p.getBoolPref('xpinstall.signatures.required'),signatureUser:p.prefHasUserValue('xpinstall.signatures.required')},tabs:Array.from(gBrowser.tabs).map(t=>({url:t.linkedBrowser.currentURI.spec,title:t.label,selected:t.selected,hidden:t.hidden,loading:t.linkedBrowser.webProgress.isLoadingDocument})),events:window.__kan37Events||[]};"""

try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',':'+str(a.display),'-screen','0','1440x1000x24','-nolisten','tcp']);time.sleep(1);wm=launch('openbox',['/workspace/qa-linux-22/C/KAN-90/sysroot/usr/bin/openbox','--sm-disable']);time.sleep(1);assert wm.poll() is None
 args=[str(a.product/('floorp' if (a.product/'floorp').exists() else 'firefox')),'--no-remote','--profile',str(profile),'--marionette'];args+=([a.uri] if a.uri else []);b=launch('browser',args);thread=threading.Thread(target=monitor,daemon=True);thread.start();start=time.monotonic();m=Marionette(host='127.0.0.1',port=a.port,socket_timeout=60);m.raise_for_port(timeout=55);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome');m.timeout.script=8;m.client.socket_timeout=5;m.execute_script("window.__kan37Events=[];window.__kan37Observer={observe(subject,topic,data){window.__kan37Events.push({topic,data:data?String(data).slice(0,80):null,time:Date.now()});}};for(const t of ['newtab-visible','newtab-top-sites-changed','telemetry-tou-accepted-or-ineligible','browser-ready-for-user-startup'])Services.obs.addObserver(window.__kan37Observer,t);return true;");value=m.execute_script(statusJS);save(out/'ready.json',{'startupSeconds':time.monotonic()-start,'value':value,'monitorRootPid':b.pid});last=0
 while b.poll() is None:
  command=out/'command.json'
  if command.exists() and command.stat().st_mtime_ns!=last:
   last=command.stat().st_mtime_ns;d=json.loads(command.read_text());phase=d['id'];started=time.monotonic()
   if d.get('stop'):
    result={'requestedNormalQuit':True}
    try:m.set_context('chrome');m.execute_script('setTimeout(()=>Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit),100);return true;');result['exitCode']=b.wait(timeout=20)
    except Exception as e:
     result.update(errorType=type(e).__name__,error=str(e),forcedTermination=True);b.terminate()
     try:result['exitCode']=b.wait(timeout=8)
     except subprocess.TimeoutExpired:b.kill();result['exitCode']=b.wait()
    save(out/'shutdown.json',result);break
   try:
    if d.get('target'):
     m.set_context('chrome');target=m.execute_script("const target=arguments[0];const t=Array.from(gBrowser.tabs).find(t=>t.linkedBrowser.currentURI.spec.includes(target));if(!t)return null;const {NavigableManager}=ChromeUtils.importESModule('chrome://remote/content/shared/NavigableManager.sys.mjs');return NavigableManager.getIdForBrowser(t.linkedBrowser);",script_args=[d['target']]);assert target,'requested target not found';m.set_context('content');m.switch_to_window(target)
    m.set_context(d.get('context','chrome'))
    if d.get('nativeKeys'):
     m.set_context('chrome');m.execute_script('window.focus();return true;')
     for key in d['nativeKeys']:subprocess.run(['/workspace/.floorp-cloud/bin/xdotool','key',key],env=env,check=True)
    if d.get('click'):m.find_element('css selector',d['click']).click()
    if d.get('screenshot'):
     sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages');from PIL import ImageGrab;ImageGrab.grab(xdisplay=env['DISPLAY']).save(out/d['screenshot'])
    script=d.get('script',statusJS);rpc_start=time.monotonic();value=m.execute_async_script(script) if d.get('async') else m.execute_script(script);save(out/('result-'+d['id']+'.json'),{'elapsedMs':(time.monotonic()-started)*1000,'rpcMs':(time.monotonic()-rpc_start)*1000,'value':value})
   except Exception as e:save(out/('result-'+d['id']+'.json'),{'elapsedMs':(time.monotonic()-started)*1000,'errorType':type(e).__name__,'error':str(e),'trace':traceback.format_exc()})
  time.sleep(.15)
except Exception as e:
 try:
  sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages');from PIL import ImageGrab;ImageGrab.grab(xdisplay=env['DISPLAY']).save(out/'session-error.png')
 except Exception:pass
 save(out/'session-error.json',{'errorType':type(e).__name__,'error':str(e),'trace':traceback.format_exc()})
finally:
 stop.set();time.sleep(.6);save(out/'cpu-samples.json',samples)
 for q in reversed(children):
  if q.poll() is None:q.terminate()
 for q in children:
  try:q.wait(timeout=5)
  except subprocess.TimeoutExpired:q.kill();q.wait()
 server.shutdown()
