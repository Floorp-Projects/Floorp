#!/usr/bin/env python3
"""Real xdg-desktop-portal OpenURI -> signed Flatpak Floorp control; no Thunderbird."""
import os,sys,subprocess,time,json,threading,http.server
from pathlib import Path
sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages')
from PIL import ImageGrab
from marionette_driver.marionette import Marionette
R=Path(__file__).resolve().parent.parent; O=Path(__file__).resolve().parent
os.environ.update(DISPLAY=':192',FLATPAK_DBUSPROXY=str(R/'sysroot/usr/bin/xdg-dbus-proxy'),DBUS_SYSTEM_BUS_ADDRESS=os.environ['DBUS_SESSION_BUS_ADDRESS'],XDG_CURRENT_DESKTOP='GNOME',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_APP_REMOTINGNAME='qa-kan90-floorp')
os.environ['XDG_DATA_DIRS']=str(R/'sysroot/usr/share')+':/usr/share'
children=[];fds=[];hits=[]
def launch(name,args):
 f=open(O/(name+'.log'),'w');fds.append(f);p=subprocess.Popen(args,stdout=f,stderr=subprocess.STDOUT);children.append(p);return p
def command(args):
 p=subprocess.run(args,capture_output=True,text=True,timeout=30);return {'command':args,'rc':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
class Handler(http.server.BaseHTTPRequestHandler):
 def do_GET(self):
  hits.append({'time':time.time(),'path':self.path});self.send_response(200);self.send_header('Cache-Control','no-store');self.end_headers();self.wfile.write(b'<title>KAN90 real portal marker</title>Real portal local fixture')
 def log_message(self,*args):pass
server=http.server.HTTPServer(('127.0.0.1',19092),Handler);threading.Thread(target=server.serve_forever,daemon=True).start()
result={'key':'KAN-90','sourceSha':'984509d2053be7bae9027133bda7259cd706b208','control':'Independent direct real portal invocation; Thunderbird is not part of this followup','environment':'Debian13.6 Xvfb/Openbox/private session+fixture system DBus; Flatpak1.16.6 Floorp12.21.1 Gecko159.0.1','input':'gdbus OpenURI API call; any chooser input is synthetic X11'}
try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',':192','-screen','0','1280x900x24','-nolisten','tcp']);time.sleep(1)
 launch('openbox',[str(R/'sysroot/usr/bin/openbox'),'--sm-disable']);time.sleep(.5)
 sd=Path(os.environ['XDG_DATA_HOME'])/'dbus-1/services';sd.mkdir(parents=True,exist_ok=True)
 for service in (R/'sysroot/usr/share/dbus-1/services').glob('*.service'):
  sd.joinpath(service.name).write_text(service.read_text().replace('/usr/libexec/',str(R/'sysroot/usr/libexec')+'/'))
 pp=Path(os.environ['HOME'])/'.var/app/one.ablaze.floorp/qa-portal-profile';pp.mkdir(parents=True,exist_ok=True)
 pp.joinpath('user.js').write_text('user_pref("marionette.port", 28292);\nuser_pref("browser.shell.checkDefaultBrowser", false);\nuser_pref("browser.startup.homepage", "about:blank");\n')
 ad=Path(os.environ['XDG_DATA_HOME'])/'applications';ad.mkdir(parents=True,exist_ok=True)
 ad.joinpath('qa-floorp.desktop').write_text('[Desktop Entry]\nName=QA Floorp\nType=Application\nExec='+str(R/'flatpak-env.sh')+' flatpak run --user one.ablaze.floorp --profile '+str(pp)+' %u\nMimeType=x-scheme-handler/http;x-scheme-handler/https;text/html;\n')
 cfg=Path(os.environ['XDG_CONFIG_HOME']);cfg.mkdir(parents=True,exist_ok=True)
 cfg.joinpath('mimeapps.list').write_text('[Default Applications]\nx-scheme-handler/http=qa-floorp.desktop\nx-scheme-handler/https=qa-floorp.desktop\ntext/html=qa-floorp.desktop\n')
 result['dbusActivation']=command(['dbus-update-activation-environment','DISPLAY','XDG_CURRENT_DESKTOP','XDG_DATA_DIRS','MOZ_APP_REMOTINGNAME','FLATPAK_DBUSPROXY'])
 launch('portal-gtk',[str(R/'sysroot/usr/libexec/xdg-desktop-portal-gtk')]);launch('portal',[str(R/'sysroot/usr/libexec/xdg-desktop-portal'),'--verbose'])
 launch('dbus-monitor',['dbus-monitor','--session',"interface='org.freedesktop.portal.OpenURI'","interface='org.freedesktop.portal.Request'"])
 fp=launch('floorp',['flatpak','run','--user','one.ablaze.floorp','--profile',str(pp),'--marionette','about:blank']);time.sleep(15)
 m=Marionette(host='127.0.0.1',port=28292,socket_timeout=60);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome')
 def state():
  return m.execute_script('return {pid:Services.appinfo.processID,version:Services.appinfo.version,sandboxLevel:Services.prefs.getIntPref("security.sandbox.content.level"),tabs:Array.from(gBrowser.tabs).map(t=>t.linkedBrowser.currentURI.spec)};')
 def processes():
  return m.execute_async_script('const done=arguments[arguments.length-1]; ChromeUtils.requestProcInfo().then(p=>{const prune=x=>({pid:x.pid,type:x.type,children:(x.children||[]).map(prune)});done(prune(p));},e=>done({error:String(e)}));')
 result['before']=state();result['processesBefore']=processes();result['flatpakInstancesBefore']=command(['flatpak','ps','--columns=pid,application'])
 ImageGrab.grab(xdisplay=':192').save(O/'before-portal.png')
 result['portalCall']=command(['gdbus','call','--session','--dest','org.freedesktop.portal.Desktop','--object-path','/org/freedesktop/portal/desktop','--method','org.freedesktop.portal.OpenURI.OpenURI','','http://127.0.0.1:19092/from-real-portal',"{'ask': <false>}"])
 (O/'ready.json').write_text(json.dumps(result,indent=2))
 # Allow the real backend/default handler to respond. If a chooser appears,
 # an operator-independent synthetic test action can be supplied in click.json.
 for tick in range(100):
  if hits:break
  cf=O/'click.json'
  if cf.exists():
   point=json.loads(cf.read_text());cf.unlink();subprocess.run(['xdotool','mousemove','--sync',str(point['x']),str(point['y']),'click','1'],check=True);result.setdefault('chooserClicks',[]).append(point)
  if tick%10==0:ImageGrab.grab(xdisplay=':192').save(O/'waiting-portal.png')
  time.sleep(.5)
 time.sleep(3);result['hits']=hits;result['after']=state();result['processesAfter']=processes();result['flatpakInstancesAfter']=command(['flatpak','ps','--columns=pid,application']);ImageGrab.grab(xdisplay=':192').save(O/'after-portal.png')
 result['originalParentRetained']=result['before']['pid']==result['after']['pid']
except Exception as e:result['error']=str(e)
finally:
 result['hits']=hits;(O/'result.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2),flush=True)
 for p in reversed(children):
  if p.poll() is None:p.terminate()
 for p in children:
  try:p.wait(timeout=4)
  except subprocess.TimeoutExpired:p.kill()
 server.shutdown()
