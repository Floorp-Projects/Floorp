#!/usr/bin/env python3
"""Actual-product panel PiP regression; old affected product must fail assertions.
Run inside dbus-run-session with marionette_driver, ffmpeg, Xvfb, Openbox, xdotool.
Never imports mocks, modifies product resources or disables product protections.
"""
import argparse,functools,hashlib,http.server,json,os,subprocess,sys,threading,time,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--display',type=int,required=True);p.add_argument('--port',type=int,required=True);p.add_argument('--http-port',type=int,required=True);p.add_argument('--source-sha',required=True);p.add_argument('--video-file',type=Path);a=p.parse_args()
R=a.out.resolve();R.mkdir(parents=True,exist_ok=False);P=R/'home/profile';P.mkdir(parents=True);F=R/'fixture';F.mkdir();runtime=R/'runtime';runtime.mkdir(mode=0o700)
import shutil
if a.video_file:shutil.copy2(a.video_file,F/'video.webm')
else:subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-f','lavfi','-i','testsrc2=size=480x270:rate=24','-t','60','-c:v','libvpx','-b:v','350k','-an',str(F/'video.webm')],check=True)
(F/'index.html').write_text('<!doctype html><title>KAN50 real VP8 local fixture</title><style>body{margin:0;background:#345;color:white}video{width:100%;max-width:480px}</style><h2>KAN50 local VP8 fixture</h2><video src="/video.webm" controls loop muted playsinline></video><p>Local generated video; not YouTube</p>')
url=f'http://127.0.0.1:{a.http_port}/index.html';prefs={'marionette.port':a.port,'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,'browser.startup.homepage':'about:blank','floorp.panelSidebar.enabled':True,'floorp.panelSidebar.data':json.dumps({'data':[{'id':'qa-pip','type':'web','width':500,'url':url}]})}
(P/'user.js').write_text('\n'.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');' for k,v in prefs.items()))
env=dict(os.environ)
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','MOZ_DISABLE_GPU_SANDBOX','MOZ_DISABLE_RDD_SANDBOX','MOZ_DISABLE_GMP_SANDBOX','MOZ_DISABLE_SOCKET_PROCESS_SANDBOX']:
 if env.get(k):raise SystemExit('Refusing inherited protection disabling variable: '+k)
env.update(HOME=str(R/'home'),XDG_CONFIG_HOME=str(R/'home/config'),XDG_DATA_HOME=str(R/'home/data'),XDG_CACHE_HOME=str(R/'home/cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=f':{a.display}',LD_LIBRARY_PATH='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_APP_REMOTINGNAME=f'qa-kan50-regression-{a.port}',PATH='/workspace/.floorp-cloud/bin:'+env['PATH'])
os.environ.update(env)
class Handler(http.server.SimpleHTTPRequestHandler):
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',a.http_port),functools.partial(Handler,directory=str(F)));threading.Thread(target=server.serve_forever,daemon=True).start()
children=[];r={'sourceSha':a.source_sha,'binarySha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'fixture':'generated60s VP8, localhost only; not YouTube','syntheticInput':'XTest native video play/toggle/close buttons','readOnlyObservation':'existing browser messageManager frame script only reads video/UA controls state; no PiP/playback mocks','fixtureSha256':hashlib.sha256((F/'video.webm').read_bytes()).hexdigest(),'cases':{}}
def launch(name,args):
 q=subprocess.Popen(args,env=env,stdout=open(R/(name+'.log'),'w'),stderr=subprocess.STDOUT);children.append(q);return q
def click(x,y):
 subprocess.run(['xdotool','mousemove','--sync',str(round(x)),str(round(y))],check=True);time.sleep(.4);subprocess.run(['xdotool','click','1'],check=True)
def js(s):return m.execute_script(s)
def media(browser):
 code='var qv=content.document.querySelector("video"),qr=qv.openOrClosedShadowRoot;sendAsyncMessage("QAKAN50:state",{time:qv.currentTime,duration:qv.duration,ready:qv.readyState,paused:qv.paused,frames:qv.getVideoPlaybackQuality().totalVideoFrames,cloning:qv.isCloningElementVisually,width:qv.videoWidth,height:qv.videoHeight,toggleRect:qr?.getElementById("pictureInPictureToggle")?.getBoundingClientRect().toJSON(),playRect:qr?.getElementById("playButton")?.getBoundingClientRect().toJSON()});'
 return m.execute_async_script('const done=arguments[arguments.length-1],mm='+browser+'.messageManager;mm.addMessageListener("QAKAN50:state",function f(m){mm.removeMessageListener("QAKAN50:state",f);done(m.data)});mm.loadFrameScript("data:application/javascript,"+encodeURIComponent('+json.dumps(code)+'),false);')
def browserClick(browser,rect):
 o=js('const b='+browser+',r=b.getBoundingClientRect(),w=b.ownerDocument.defaultView;return {x:w.mozInnerScreenX+r.x,y:w.mozInnerScreenY+r.y};');click(o['x']+rect['x']+rect['width']/2,o['y']+rect['y']+rect['height']/2)
def pipState():return js('return Array.from(Services.wm.getEnumerator("Toolkit:PictureInPicture")).map(w=>({closed:w.closed,hasBrowser:!!w.document.getElementById("browser"),playLabel:w.document.getElementById("playpause")?.getAttribute("aria-label"),width:w.innerWidth,height:w.innerHeight}));')
def closePip():
 q=js('const w=Services.wm.getMostRecentWindow("Toolkit:PictureInPicture"),r=w.document.getElementById("close").getBoundingClientRect();return {x:w.mozInnerScreenX+r.x+r.width/2,y:w.mozInnerScreenY+r.y+r.height/2};');click(q['x'],q['y']);time.sleep(1);return pipState()
def shot(name):
 sys.path.append('/opt/codex/runtimes/codex-primary-runtime/dependencies/python/lib/python3.12/site-packages');from PIL import ImageGrab;ImageGrab.grab(xdisplay=f':{a.display}').save(R/(name+'.png'))
def exercise(name,browser):
 q={'before':media(browser)};browserClick(browser,q['before']['playRect']);time.sleep(2);q['playing']=media(browser);browserClick(browser,q['playing']['toggleRect']);time.sleep(2);q['windows']=pipState();q['afterOpen']=media(browser);shot(name+'-pip');q['afterClose']=closePip();q['afterCloseMedia']=media(browser);r['cases'][name]=q;return q
try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',f':{a.display}','-screen','0','1440x1000x24','-nolisten','tcp']);time.sleep(1);launch('openbox',['/workspace/qa-linux-22/C/KAN-90/sysroot/usr/bin/openbox','--sm-disable'])
 launch('product',[str(a.binary.resolve()),'--no-remote','--profile',str(P),'--marionette',url]);m=Marionette(host='127.0.0.1',port=a.port);m.raise_for_port(timeout=45);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome');m.timeout.script=10;time.sleep(12)
 r['runtime']=js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level")};')
 js('gBrowser.selectedTab=Array.from(gBrowser.tabs).find(t=>t.linkedBrowser.currentURI.spec==='+json.dumps(url)+');')
 normal=exercise('normal','gBrowser.selectedBrowser');assert normal['afterOpen']['cloning'] and not normal['afterClose'],'normal tab PiP control failed'
 q=js('const r=document.getElementById("qa-pip").getBoundingClientRect();return {x:mozInnerScreenX+r.x+r.width/2,y:mozInnerScreenY+r.y+r.height/2};');click(q['x'],q['y']);time.sleep(6)
 r['panelStructure']=js('window.qaPanel=document.getElementById("sidebar-panel-qa-pip").browsingContext.associatedWindow;window.qaVideoBrowser=qaPanel.floorpWebPanelContentBrowser||qaPanel.gBrowser?.selectedBrowser||qaPanel.document.getElementById("floorp-webpanel-content-browser");return {gBrowserType:typeof qaPanel.gBrowser,mappedTab:!!qaPanel.gBrowser?.getTabForBrowser?.(qaVideoBrowser),remoteType:qaVideoBrowser.remoteType};')
 panel=exercise('panel','qaVideoBrowser');r['runtimeErrors']=js('return Services.console.getMessageArray().filter(x=>x.message?.includes("PictureInPicture")).map(x=>({message:x.message})).slice(-12);')
 r['after']=js('return {pid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level")};')
 assert panel['playing']['frames']>0 and panel['playing']['ready']==4,'panel real decode failed'
 assert panel['afterOpen']['cloning'],'panel video was not visually cloned into PiP'
 assert not panel['afterClose'],'PiP close button left an owned panel player open'
 assert r['runtime']['sandbox']==r['after']['sandbox']==6,'sandbox level changed'
 r['passed']=True
except Exception as e:r['passed']=False;r['error']=str(e);r['harnessTrace']=traceback.format_exc()
finally:
 (R/'result.json').write_text(json.dumps(r,indent=2)+'\n')
 for q in reversed(children):
  if q.poll() is None:q.terminate()
 for q in children:
  try:q.wait(timeout=5)
  except subprocess.TimeoutExpired:q.kill();q.wait()
 server.shutdown()
print(json.dumps({'passed':r.get('passed'),'error':r.get('error'),'result':str(R/'result.json')}));sys.exit(0 if r.get('passed') else 1)
