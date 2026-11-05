#!/usr/bin/env python3
import os,subprocess,time,socket,json,threading,http.server,base64,signal
from pathlib import Path
import sys
sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages')
from PIL import ImageGrab
R=Path(__file__).resolve().parent
os.environ.update(DISPLAY=':190',FLATPAK_DBUSPROXY=str(R/'sysroot/usr/bin/xdg-dbus-proxy'),DBUS_SYSTEM_BUS_ADDRESS=os.environ['DBUS_SESSION_BUS_ADDRESS'])
os.environ['XDG_CURRENT_DESKTOP']='GNOME'
os.environ['MOZ_REMOTE_ALLOW_SYSTEM_ACCESS']='1'
os.environ['MOZ_APP_REMOTINGNAME']='qa-kan90-floorp'
os.environ['XDG_DATA_DIRS']=str(R/'sysroot/usr/share')+':/usr/share'
logs=[];children=[];hits=[]
def launch(name,args,env=None):
 f=open(R/(name+'.log'),'w');p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT,env=env);children.append(p);logs.append(f);return p
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append({'time':time.time(),'path':self.path});self.send_response(200);self.end_headers();self.wfile.write(b'<title>KAN90 local link reached</title>Local fixture only')
 def log_message(self,*args):pass
server=http.server.HTTPServer(('127.0.0.1',19090),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
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
result={'sourceSha':'984509d2053be7bae9027133bda7259cd706b208','environment':'Debian13.6 Xvfb/Openbox private session DBus; Flatpak1.16.6; Thunderbird140.12.0esr','syntheticInput':True}
try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',':190','-screen','0','1280x900x24','-nolisten','tcp']);time.sleep(1)
 launch('openbox',[str(R/'sysroot/usr/bin/openbox'),'--sm-disable']);time.sleep(.5)
 # User service activation copies preserve vendor semantics but point at user-extracted executables.
 sd=Path(os.environ['XDG_DATA_HOME'])/'dbus-1/services';sd.mkdir(parents=True,exist_ok=True)
 for service in (R/'sysroot/usr/share/dbus-1/services').glob('*.service'):
  sd.joinpath(service.name).write_text(service.read_text().replace('/usr/libexec/',str(R/'sysroot/usr/libexec')+'/'))
 appdir=Path(os.environ['XDG_DATA_HOME'])/'applications';appdir.mkdir(parents=True,exist_ok=True)
 appdir.joinpath('qa-floorp.desktop').write_text('[Desktop Entry]\nName=QA Floorp\nType=Application\nExec='+str(R/'flatpak-env.sh')+' flatpak run --user one.ablaze.floorp --profile '+str(Path(os.environ['HOME'])/'.var/app/one.ablaze.floorp/qa-profile')+' %u\nMimeType=x-scheme-handler/http;x-scheme-handler/https;text/html;\n')
 cfg=Path(os.environ['XDG_CONFIG_HOME']);cfg.mkdir(parents=True,exist_ok=True)
 cfg.joinpath('mimeapps.list').write_text('[Default Applications]\nx-scheme-handler/http=qa-floorp.desktop\nx-scheme-handler/https=qa-floorp.desktop\ntext/html=qa-floorp.desktop\n')
 subprocess.run(['dbus-update-activation-environment','DISPLAY','XDG_CURRENT_DESKTOP','XDG_DATA_DIRS'],check=True)
 launch('portal-gtk',[str(R/'sysroot/usr/libexec/xdg-desktop-portal-gtk')])
 launch('portal',[str(R/'sysroot/usr/libexec/xdg-desktop-portal'),'--verbose'])
 launch('dbus-monitor',['dbus-monitor','--session',"interface='org.freedesktop.portal.OpenURI'"])
 pp=Path(os.environ['HOME'])/'.var/app/one.ablaze.floorp/qa-profile';pp.mkdir(parents=True,exist_ok=True)
 pp.joinpath('user.js').write_text('user_pref("marionette.port", 28290);\nuser_pref("browser.shell.checkDefaultBrowser", false);\nuser_pref("browser.startup.homepage", "about:blank");\n')
 fp=launch('floorp',['flatpak','run','--user','one.ablaze.floorp','--profile',str(pp),'--marionette','-remote-allow-system-access','about:blank']);result['floorpLauncherPid']=fp.pid
 fm=waitport(28290);fm.call('Marionette:SetContext',{'value':'chrome'});result['floorpStart']=fm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version, tabs:gBrowser.tabs.length};')
 (R/'fixture.eml').write_text('From: fixture@example.invalid\nTo: fixture@example.invalid\nSubject: KAN90 local fixture\nMIME-Version: 1.0\nContent-Type: text/html; charset=UTF-8\n\n<html><body style="font-size:32px"><a href="http://127.0.0.1:19090/from-thunderbird">Open local KAN90 fixture</a></body></html>\n')
 tp=R/'thunderbird-profile';tp.mkdir(exist_ok=True);tp.joinpath('user.js').write_text('user_pref("marionette.port", 28291);\nuser_pref("mail.shell.checkDefaultClient", false);\nuser_pref("widget.use-xdg-desktop-portal.open-uri", 2);\n')
 tb_env=dict(os.environ);tb_env['GTK_USE_PORTAL']='1'
 result['portalSelection']='Thunderbird GTK_USE_PORTAL=1 and widget.use-xdg-desktop-portal.open-uri=2; actual OpenURI must be observed'
 tb=launch('thunderbird',[str(R/'sysroot/usr/lib/thunderbird/thunderbird'),'--profile',str(tp),'--marionette','-remote-allow-system-access','-file',str(R/'fixture.eml')],env=tb_env);tm=waitport(28291);tm.call('Marionette:SetContext',{'value':'chrome'});time.sleep(5)
 result['thunderbirdWindows']=tm.js('return Array.from(Services.wm.getEnumerator(null)).map(w=>({title:w.document.title,url:w.location.href}));')
 result['thunderbirdDom']=tm.js('return Array.from(document.querySelectorAll("browser")).map(b=>({id:b.id,src:b.src,uri:b.currentURI?.spec}));')
 ImageGrab.grab(xdisplay=':190').save(R/'before-link.png')
 (R/'state-ready.json').write_text(json.dumps(result,indent=2))
 # Explicit synthetic pointer input to the visible local .eml link.
 subprocess.run(['xdotool','mousemove','--sync','200','215','click','1'],check=True);time.sleep(5)
 result['hits']=hits
 result['floorpEnd']=fm.js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,tabs:Array.from(gBrowser.tabs).map(t=>t.linkedBrowser.currentURI.spec)};')
 ImageGrab.grab(xdisplay=':190').save(R/'after-link.png')
except Exception as e:
 result['error']=str(e)
finally:
 result['hits']=hits;(R/'integration-result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
 for p in reversed(children):
  if p.poll() is None:p.terminate()
 for p in children:
  try:p.wait(timeout=4)
  except subprocess.TimeoutExpired:p.kill()
 server.shutdown()
