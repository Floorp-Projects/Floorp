#!/usr/bin/env python3
import os,subprocess,time,socket,json,threading,http.server,base64,signal
from pathlib import Path
import sys
sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages')
from PIL import ImageGrab
R=Path('/workspace/qa-linux-22/C/KAN-90'); V=Path(__file__).resolve().parent; variant=sys.argv[1]; browser=Path(sys.argv[2]).resolve(); O=V/variant;O.mkdir(exist_ok=True)
os.environ.update(DISPLAY=':194',FLATPAK_DBUSPROXY=str(R/'sysroot/usr/bin/xdg-dbus-proxy'),DBUS_SYSTEM_BUS_ADDRESS=os.environ['DBUS_SESSION_BUS_ADDRESS'])
os.environ['XDG_CURRENT_DESKTOP']='GNOME'
os.environ['MOZ_REMOTE_ALLOW_SYSTEM_ACCESS']='1'
os.environ['MOZ_APP_REMOTINGNAME']='qa-kan90-compare-'+variant
os.environ['XDG_DATA_DIRS']=str(R/'sysroot/usr/share')+':/usr/share'
logs=[];children=[];hits=[]
def launch(name,args,env=None):
 f=open(O/(name+'.log'),'w');p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT,env=env);children.append(p);logs.append(f);return p
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append({'time':time.time(),'path':self.path});self.send_response(200);self.end_headers();self.wfile.write(b'<title>KAN90 local link reached</title>Local fixture only')
 def log_message(self,*args):pass
server=http.server.HTTPServer(('127.0.0.1',19094),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
from marionette_driver.marionette import Marionette as Driver
class Marionette:
 def __init__(self,port):
  self.m=Driver(host='127.0.0.1',port=port,socket_timeout=60)
  self.m.start_session({'acceptInsecureCerts':False})
 def call(self,method,args={}):
  if method=='Marionette:SetContext':self.m.set_context(args['value']);return None
 def js(self,script):return self.m.execute_script(script)
def waitport(port):
 time.sleep(15)
 return Marionette(port)
result={'sourceSha':('38bde3075f197b9a88490875d1534bb79d6159a5' if variant=='12.17.2' else '984509d2053be7bae9027133bda7259cd706b208'),'browserDistribution':'official release Linux x86_64 archive','variant':variant,'environment':'Debian13.6 Xvfb/Openbox private session DBus; Flatpak1.16.6; Thunderbird159.0 official Flathub org.mozilla.thunderbird','syntheticInput':True}
try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',':194','-screen','0','1280x900x24','-nolisten','tcp']);time.sleep(1)
 launch('openbox',[str(R/'sysroot/usr/bin/openbox'),'--sm-disable']);time.sleep(.5)
 # User service activation copies preserve vendor semantics but point at user-extracted executables.
 sd=Path(os.environ['XDG_DATA_HOME'])/'dbus-1/services';sd.mkdir(parents=True,exist_ok=True)
 for service in (R/'sysroot/usr/share/dbus-1/services').glob('*.service'):
  sd.joinpath(service.name).write_text(service.read_text().replace('/usr/libexec/',str(R/'sysroot/usr/libexec')+'/'))
 appdir=Path(os.environ['XDG_DATA_HOME'])/'applications';appdir.mkdir(parents=True,exist_ok=True)
 appdir.joinpath('qa-floorp.desktop').write_text('[Desktop Entry]\nName=QA Floorp\nType=Application\nExec='+str(V/('launch-'+variant+'.sh'))+' --profile '+str(V/(variant+'-floorp-profile'))+' %u\nMimeType=x-scheme-handler/http;x-scheme-handler/https;text/html;\n')
 cfg=Path(os.environ['XDG_CONFIG_HOME']);cfg.mkdir(parents=True,exist_ok=True)
 cfg.joinpath('mimeapps.list').write_text('[Default Applications]\nx-scheme-handler/http=qa-floorp.desktop\nx-scheme-handler/https=qa-floorp.desktop\ntext/html=qa-floorp.desktop\n')
 subprocess.run(['dbus-update-activation-environment','DISPLAY','XDG_CURRENT_DESKTOP','XDG_DATA_DIRS'],check=True)
 launch('portal-gtk',[str(R/'sysroot/usr/libexec/xdg-desktop-portal-gtk')])
 launch('portal',[str(R/'sysroot/usr/libexec/xdg-desktop-portal'),'--verbose'])
 launch('dbus-monitor',['dbus-monitor','--session',"interface='org.freedesktop.portal.OpenURI'","interface='org.freedesktop.portal.Request'"])
 pp=V/(variant+'-floorp-profile');pp.mkdir(parents=True,exist_ok=True)
 pp.joinpath('user.js').write_text('user_pref("marionette.port", 28295);\nuser_pref("browser.shell.checkDefaultBrowser", false);\nuser_pref("browser.startup.homepage", "about:blank");\n')
 fp=launch('floorp',[str(browser),'--profile',str(pp),'--marionette','-remote-allow-system-access','about:blank']);result['floorpLauncherPid']=fp.pid
 fm=waitport(28295);fm.call('Marionette:SetContext',{'value':'chrome'});result['floorpStart']=fm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),tabs:gBrowser.tabs.length};')
 (Path(os.environ['HOME'])/'.var/app/org.mozilla.thunderbird').mkdir(parents=True,exist_ok=True)
 (Path(os.environ['HOME'])/'.var/app/org.mozilla.thunderbird/fixture.eml').write_text('From: fixture@example.invalid\nTo: fixture@example.invalid\nSubject: KAN90 local fixture\nMIME-Version: 1.0\nContent-Type: text/html; charset=UTF-8\n\n<html><body style="font-size:32px"><a href="http://127.0.0.1:19094/from-thunderbird-origin">Open local KAN90 fixture</a></body></html>\n')
 tp=Path(os.environ['HOME'])/('.var/app/org.mozilla.thunderbird/qa-comparison-'+variant);tp.mkdir(parents=True,exist_ok=True);tp.joinpath('user.js').write_text('user_pref("marionette.port", 28296);\nuser_pref("mail.shell.checkDefaultClient", false);\n')
 tb_env=dict(os.environ);tb_env.pop('GTK_USE_PORTAL',None);tb_env.pop('MOZ_APP_REMOTINGNAME',None);tb_env['MOZ_LOG']='GIOService:5'
 result['portalSelection']='Signed official Thunderbird159.0 Flatpak; shipped portal prefs unchanged; Flatpak identity automatic, no GTK_USE_PORTAL/remoting override'
 tb=launch('thunderbird',['flatpak','run','--user','org.mozilla.thunderbird','--profile',str(tp),'--marionette','-remote-allow-system-access','-file',str(Path(os.environ['HOME'])/'.var/app/org.mozilla.thunderbird/fixture.eml')],env=tb_env);tm=waitport(28296);tm.call('Marionette:SetContext',{'value':'chrome'});time.sleep(5)
 result['thunderbirdWindows']=tm.js('return Array.from(Services.wm.getEnumerator(null)).map(w=>({title:w.document.title,url:w.location.href}));')
 result['thunderbirdDom']=tm.js('return Array.from(document.querySelectorAll("browser")).map(b=>({id:b.id,src:b.src,uri:b.currentURI?.spec}));')
 result['thunderbirdRuntime']=tm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,mimeHandler:Services.prefs.getIntPref(\"widget.use-xdg-desktop-portal.mime-handler\"),openUri:Services.prefs.getIntPref(\"widget.use-xdg-desktop-portal.open-uri\"),gtkUsePortal:Services.env.get(\"GTK_USE_PORTAL\"),flatpakId:Services.env.get(\"FLATPAK_ID\"),remotingOverride:Services.env.get(\"MOZ_APP_REMOTINGNAME\"),sandbox:Services.prefs.getIntPref(\"security.sandbox.content.level\")};')
 result['handlerInfo']=tm.js('const h=Cc[\"@mozilla.org/uriloader/external-protocol-service;1\"].getService(Ci.nsIExternalProtocolService).getProtocolHandlerInfo(\"http\");return {preferredAction:h.preferredAction,useSystemDefault:Ci.nsIHandlerInfo.useSystemDefault,useHelperApp:Ci.nsIHandlerInfo.useHelperApp,alwaysAsk:h.alwaysAskBeforeHandling,defaultDescription:h.defaultDescription,preferredApplication:h.preferredApplicationHandler?.name,possibleHandlers:Array.from({length:h.possibleApplicationHandlers.length},(_,i)=>h.possibleApplicationHandlers.queryElementAt(i,Ci.nsIHandlerApp).name)};')
 result['flatpakInstancesBefore']=subprocess.run(['flatpak','ps','--columns=instance,pid,application'],capture_output=True,text=True).stdout
 result['floorpBefore']=fm.js('return {pid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref(\"security.sandbox.content.level\"),tabs:Array.from(gBrowser.tabs).map(t=>t.linkedBrowser.currentURI.spec)};')
 result['floorpProcessesBefore']=fm.m.execute_async_script('const done=arguments[arguments.length-1];ChromeUtils.requestProcInfo().then(p=>done({pid:p.pid,children:p.children.map(c=>({pid:c.pid,type:c.type}))}));')
 ImageGrab.grab(xdisplay=':194').save(O/'before-link.png')
 (O/'state-ready.json').write_text(json.dumps(result,indent=2))
 # Explicit synthetic pointer input to the visible local .eml link.
 subprocess.run(['xdotool','mousemove','--sync','200','215','click','1'],check=True);time.sleep(6)
 result['hits']=hits
 result['floorpEnd']=fm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,tabs:Array.from(gBrowser.tabs).map(t=>t.linkedBrowser.currentURI.spec)};')
 result['floorpProcessesAfter']=fm.m.execute_async_script('const done=arguments[arguments.length-1];ChromeUtils.requestProcInfo().then(p=>done({pid:p.pid,children:p.children.map(c=>({pid:c.pid,type:c.type}))}));')
 import re
 trace=(O/'dbus-monitor.log').read_text();senders=set(re.findall(r'sender=(:[0-9.]+)',trace));result['dbusSenders']={}
 for sender in sorted(senders):
  q=subprocess.run(['gdbus','call','--session','--dest','org.freedesktop.DBus','--object-path','/org/freedesktop/DBus','--method','org.freedesktop.DBus.GetConnectionUnixProcessID',sender],capture_output=True,text=True);result['dbusSenders'][sender]={'rc':q.returncode,'stdout':q.stdout,'stderr':q.stderr}
  match=re.search(r'uint32 (\d+)',q.stdout)
  if match:
   pid=match.group(1);result['dbusSenders'][sender]['processName']=Path('/proc/'+pid+'/comm').read_text().strip()
   try:
    import configparser
    info=configparser.ConfigParser(interpolation=None);info.read('/proc/'+pid+'/root/.flatpak-info');result['dbusSenders'][sender]['flatpakAppId']=info.get('Application','name',fallback=None)
   except Exception as e:result['dbusSenders'][sender]['identityReadError']=type(e).__name__
 result['flatpakInstancesAfter']=subprocess.run(['flatpak','ps','--columns=instance,pid,application'],capture_output=True,text=True).stdout
 ImageGrab.grab(xdisplay=':194').save(O/'after-link.png')
except Exception as e:
 result['error']=str(e)
finally:
 result['hits']=hits;(O/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
 for p in reversed(children):
  if p.poll() is None:p.terminate()
 for p in children:
  try:p.wait(timeout=4)
  except subprocess.TimeoutExpired:p.kill()
 server.shutdown()
