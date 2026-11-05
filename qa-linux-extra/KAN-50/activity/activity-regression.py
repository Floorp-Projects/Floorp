#!/usr/bin/env python3
"""Actual-product panel PiP regression; old affected product must fail assertions.
Run inside dbus-run-session with marionette_driver, ffmpeg, Xvfb, Openbox, xdotool.
Never imports mocks, modifies product resources or disables product protections.
"""
import argparse,functools,hashlib,http.server,json,os,subprocess,sys,threading,time,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
p=argparse.ArgumentParser();p.add_argument('--binary',type=Path,required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--display',type=int,required=True);p.add_argument('--port',type=int,required=True);p.add_argument('--http-port',type=int,required=True);p.add_argument('--source-sha',required=True);p.add_argument('--video-file',type=Path);p.add_argument('--native-test',type=Path);p.add_argument('--order',choices=['normal-first','panel-first'],default='normal-first');a=p.parse_args()
R=a.out.resolve();R.mkdir(parents=True,exist_ok=False);P=R/'home/profile';P.mkdir(parents=True);F=R/'fixture';F.mkdir();runtime=R/'runtime';runtime.mkdir(mode=0o700)
import shutil
if a.video_file:shutil.copy2(a.video_file,F/'video.webm')
else:subprocess.run(['ffmpeg','-hide_banner','-loglevel','error','-f','lavfi','-i','testsrc2=size=480x270:rate=24','-t','60','-c:v','libvpx','-b:v','350k','-an',str(F/'video.webm')],check=True)
(F/'captions.vtt').write_text('WEBVTT\n\n'+'\n\n'.join(f'{i}\n00:00:{i:02d}.000 --> 00:00:{i+1:02d}.000\nQA caption {i:02d}' for i in range(59)))
(F/'index.html').write_text('<!doctype html><title>KAN50 real VP8 local fixture</title><style>body{margin:0;background:#345;color:white}video{width:100%;max-width:480px}</style><h2>KAN50 local VP8 fixture</h2><video src="/video.webm" controls loop muted playsinline><track kind="subtitles" src="/captions.vtt" srclang="en" label="QA" default></video><p>Local generated video; not YouTube</p>')
url=f'http://127.0.0.1:{a.http_port}/index.html';prefs={'marionette.port':a.port,'browser.shell.checkDefaultBrowser':False,'browser.aboutwelcome.enabled':False,'browser.startup.homepage':'about:blank','floorp.panelSidebar.enabled':True,'floorp.panelSidebar.data':json.dumps({'data':[{'id':'qa-pip','type':'web','width':500,'url':url}]})}
(P/'user.js').write_text('\n'.join('user_pref('+json.dumps(k)+','+json.dumps(v)+');' for k,v in prefs.items()))
env=dict(os.environ)
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','MOZ_DISABLE_GPU_SANDBOX','MOZ_DISABLE_RDD_SANDBOX','MOZ_DISABLE_GMP_SANDBOX','MOZ_DISABLE_SOCKET_PROCESS_SANDBOX']:
 if env.get(k):raise SystemExit('Refusing inherited protection disabling variable: '+k)
env.update(HOME=str(R/'home'),XDG_CONFIG_HOME=str(R/'home/config'),XDG_DATA_HOME=str(R/'home/data'),XDG_CACHE_HOME=str(R/'home/cache'),XDG_RUNTIME_DIR=str(runtime),DISPLAY=f':{a.display}',LD_LIBRARY_PATH='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1',MOZ_APP_REMOTINGNAME=f'qa-kan50-regression-{a.port}',PATH='/workspace/.floorp-cloud/bin:'+env['PATH'])
env.update(XDG_DATA_DIRS='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/share:/usr/share',XDG_CONFIG_DIRS='/workspace/qa-linux-22/C/KAN-90/sysroot/etc/xdg')
os.environ.update(env)
class Handler(http.server.SimpleHTTPRequestHandler):
 def log_message(self,*args):pass
server=http.server.ThreadingHTTPServer(('127.0.0.1',a.http_port),functools.partial(Handler,directory=str(F)));threading.Thread(target=server.serve_forever,daemon=True).start()
children=[];r={'sourceSha':a.source_sha,'binarySha256':hashlib.sha256(a.binary.read_bytes()).hexdigest(),'fixture':'generated60s VP8, localhost only; not YouTube','syntheticInput':'native muted video.play API plus XTest native video context-menu PiP and player close buttons','readOnlyObservation':'existing browser messageManager frame script only reads video/UA controls state; no PiP/playback mocks','fixtureSha256':hashlib.sha256((F/'video.webm').read_bytes()).hexdigest(),'cases':{}}
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
def readRemote(browser,body):
 return m.execute_async_script('const done=arguments[arguments.length-1],mm='+browser+'.messageManager;mm.addMessageListener("QAKAN50:read",function f(m){mm.removeMessageListener("QAKAN50:read",f);done(m.data)});mm.loadFrameScript("data:application/javascript,"+encodeURIComponent('+json.dumps('sendAsyncMessage("QAKAN50:read",'+body+');')+'),false);')
def videoState(browser):
 return readRemote(browser,'(()=>{const v=content.document.querySelector("video"),t=v.textTracks[0];return {time:v.currentTime,paused:v.paused,cloning:v.isCloningElementVisually,hidden:content.document.hidden,visibility:content.document.visibilityState,cues:Array.from(t?.activeCues||[]).map(c=>c.text),trackMode:t?.mode,frames:v.getVideoPlaybackQuality().totalVideoFrames};})()')
def pipExpression(browser):
 return 'Array.from(Services.wm.getEnumerator("Toolkit:PictureInPicture")).find(w=>ChromeUtils.importESModule("moz-src:///toolkit/components/pictureinpicture/PictureInPicture.sys.mjs").PictureInPicture.weakWinToBrowser.get(w)==='+browser+')'
def captionState(browser):
 return readRemote('('+pipExpression(browser)+').document.getElementById("browser")','({text:content.document.getElementById("texttracks")?.textContent,hidden:content.document.hidden})')
def state():
 return js('const p=ChromeUtils.importESModule("moz-src:///toolkit/components/pictureinpicture/PictureInPicture.sys.mjs").PictureInPicture;return {rootBC:browsingContext.id,rootForced:browsingContext.forceAppWindowActive,rootActive:browsingContext.isActive,windowState:window.windowState,normalPipAttribute:gBrowser.selectedTab.getAttribute("pictureinpicture"),panelBC:window.qaPanel?.browsingContext.id,panelTop:window.qaPanel?.browsingContext.top.id,panelForced:window.qaPanel?.browsingContext.forceAppWindowActive,panelPipAttribute:window.qaPanel?.gBrowser.selectedTab.getAttribute("pictureinpicture"),rootMap:p.originatingWinWeakMap.get(window),panelMap:window.qaPanel?p.originatingWinWeakMap.get(qaPanel):null,players:p.currentPlayerCount,windows:Array.from(Services.wm.getEnumerator("Toolkit:PictureInPicture")).length};')
def openVideo(browser):
 body='var v=content.document.querySelector("video");v.play().then(()=>sendAsyncMessage("QAKAN50:play",{time:v.currentTime,paused:v.paused}),e=>sendAsyncMessage("QAKAN50:play",{error:String(e)}));'
 play=m.execute_async_script('const done=arguments[arguments.length-1],mm='+browser+'.messageManager;mm.addMessageListener("QAKAN50:play",function f(m){mm.removeMessageListener("QAKAN50:play",f);done(m.data)});mm.loadFrameScript("data:application/javascript,"+encodeURIComponent('+json.dumps(body)+'),false);')
 time.sleep(1.5);q=media(browser);rect=readRemote(browser,'content.document.querySelector("video").getBoundingClientRect().toJSON()')
 o=js('const b='+browser+',r=b.getBoundingClientRect(),w=b.ownerDocument.defaultView;return {x:w.mozInnerScreenX+r.x,y:w.mozInnerScreenY+r.y};')
 r['lastProbe']={'browser':browser,'media':q,'videoRect':rect,'origin':o,'nativePlayResult':play}
 subprocess.run(['xdotool','mousemove','--sync',str(round(o['x']+rect['x']+rect['width']/2)),str(round(o['y']+rect['y']+rect['height']/2))],check=True);time.sleep(.3);subprocess.run(['xdotool','click','3'],check=True);time.sleep(.5)
 menu=js('const w=('+browser+').ownerDocument.defaultView,e=w.document.getElementById("context-video-pictureinpicture");return {state:w.document.getElementById("contentAreaContextMenu")?.state,hidden:e?.hidden,key:e?.getAttribute("accesskey")};');r['lastProbe']['menu']=menu
 assert menu['state']=='open' and menu['hidden'] is False,'harness: native video PiP menu not available'
 subprocess.run(['xdotool','key',menu['key']],check=True);time.sleep(1.5)
 assert videoState(browser)['cloning'],'harness: actual PiP video did not clone'
 return {'activity':state(),'video':videoState(browser),'captions':captionState(browser)}
def closeTarget(browser):
 q=js('const w='+pipExpression(browser)+';w.focus();const r=w.document.getElementById("close").getBoundingClientRect();return {x:w.mozInnerScreenX+r.x+r.width/2,y:w.mozInnerScreenY+r.y+r.height/2};');click(q['x'],q['y']);time.sleep(1);return state()
def samplePanel():return {'activity':state(),'video':videoState('qaVideoBrowser'),'captions':captionState('qaVideoBrowser')}
checks=[]
def check(label,condition):checks.append({'label':label,'passed':bool(condition)})

try:
 launch('xvfb',['/workspace/.floorp-cloud/bin/Xvfb',f':{a.display}','-screen','0','1440x1000x24','-nolisten','tcp']);time.sleep(1);wm=launch('openbox',['/workspace/qa-linux-22/C/KAN-90/sysroot/usr/bin/openbox','--sm-disable']);time.sleep(1)
 assert wm.poll() is None,'harness: window manager not running'
 wmInfo=subprocess.run(['xprop','-root','_NET_SUPPORTING_WM_CHECK','_NET_SUPPORTED'],capture_output=True,text=True,check=True).stdout;r['windowManagerEvidence']=wmInfo
 assert '_NET_ACTIVE_WINDOW' in wmInfo,'harness: no managed EWMH desktop'
 launch('product',[str(a.binary.resolve()),'--no-remote','--profile',str(P),'--marionette',url]);m=Marionette(host='127.0.0.1',port=a.port);m.raise_for_port(timeout=45);m.start_session({'acceptInsecureCerts':False});m.set_context('chrome');m.timeout.script=10;time.sleep(12)
 r['runtime']=js('return {pid:Services.appinfo.processID,version:Services.appinfo.version,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level")};')
 js('gBrowser.selectedTab=Array.from(gBrowser.tabs).find(t=>t.linkedBrowser.currentURI.spec==='+json.dumps(url)+');')
 q=js('const r=document.getElementById("qa-pip").getBoundingClientRect();return {x:mozInnerScreenX+r.x+r.width/2,y:mozInnerScreenY+r.y+r.height/2};');click(q['x'],q['y']);time.sleep(6)
 js('window.qaPanel=document.getElementById("sidebar-panel-qa-pip").browsingContext.associatedWindow;window.qaVideoBrowser=qaPanel.gBrowser.selectedBrowser;')
 if a.order=='normal-first':
  r['firstOpen']=openVideo('gBrowser.selectedBrowser');r['bothOpen']=openVideo('qaVideoBrowser')
 else:
  r['firstOpen']=openVideo('qaVideoBrowser');r['bothOpen']=openVideo('gBrowser.selectedBrowser')
 check('two players share one root AppWindow count',r['bothOpen']['activity']['rootMap']==2)
 r['normalClosedWhilePanelRemains']=closeTarget('gBrowser.selectedBrowser')
 check('closing normal player retains panel activity force',r['normalClosedWhilePanelRemains']['rootForced'] and r['normalClosedWhilePanelRemains']['rootMap']==1)
 wid=subprocess.check_output(['xdotool','search','--onlyvisible','--pid',str(r['runtime']['pid']),'--name','KAN50 real']).decode().splitlines()[0]
 subprocess.run(['xdotool','windowminimize',wid],check=True);time.sleep(2);r['minimized1']=samplePanel();time.sleep(2);r['minimized2']=samplePanel()
 assert r['minimized1']['activity']['windowState']==2,'harness: requested minimize did not occur'
 check('minimized root remains active for panel PiP',r['minimized2']['activity']['rootActive'] and r['minimized2']['activity']['rootForced'])
 check('source document stays visible for captions',not r['minimized2']['video']['hidden'])
 check('real WebVTT captions continue while minimized',bool(r['minimized1']['captions']['text']) and r['minimized1']['captions']['text']!=r['minimized2']['captions']['text'])
 shot('minimized-panel-captions')
 subprocess.run(['xdotool','windowmap',wid],check=True);subprocess.run(['xdotool','windowactivate','--sync',wid],check=True);time.sleep(.5)
 r['afterAllClose']=closeTarget('qaVideoBrowser')
 check('last close releases force and root count',not r['afterAllClose']['rootForced'] and r['afterAllClose']['rootMap'] is None)
 check('last close clears native PiP tab indicators',r['afterAllClose']['normalPipAttribute'] is None and r['afterAllClose']['panelPipAttribute'] is None)
 check('last close returns player count to zero',r['afterAllClose']['players']==0 and r['afterAllClose']['windows']==0)
 r['runtimeErrors']=js('return Services.console.getMessageArray().filter(x=>x.message?.includes("forceAppWindowActive")||x.message?.includes("PictureInPicture.sys.mjs")).map(x=>({message:x.message})).slice(-15);')
 check('no originating-window activity exception',not r['runtimeErrors'])
 r['after']=js('return {pid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level")};');check('sandbox unchanged',r['runtime']['sandbox']==r['after']['sandbox']==6)
 if a.native_test:
  r['nativeBrowserTests']=m.execute_async_script('const done=arguments[arguments.length-1];'+a.native_test.read_text()+';qaRunPipActivityTests().then(()=>done({passed:true}),e=>done({passed:false,error:String(e),stack:e.stack}));');check('native embedded chrome activity tests',r['nativeBrowserTests']['passed'])
 r['checks']=checks;r['passed']=all(c['passed'] for c in checks)
except Exception as e:
 r['passed']=False;r['error']=str(e);r['harnessTrace']=traceback.format_exc();r['checks']=checks
 try:shot('harness-error')
 except Exception:pass
finally:
 (R/'result.json').write_text(json.dumps(r,indent=2)+'\n')
 for q in reversed(children):
  if q.poll() is None:q.terminate()
 for q in children:
  try:q.wait(timeout=5)
  except subprocess.TimeoutExpired:q.kill();q.wait()
 server.shutdown()
print(json.dumps({'passed':r.get('passed'),'error':r.get('error'),'checks':checks,'result':str(R/'result.json')}));sys.exit(0 if r.get('passed') else 1)
