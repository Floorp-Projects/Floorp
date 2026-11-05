from pathlib import Path
import os,time,json,http.client,urllib.parse,subprocess,zipfile,re,hashlib,base64

def run(m,xd,outdir):
 result={'measurement':'passive controlled HTTP page/iframe scroll reports, sampled by SSE; no background tab switching to read state','cases':[],'sameWindowStaleFocusProbes':[],'actionApiFixture':'unmodified production executeGestureAction imported in the existing window realm through a temporary resource: test module; real Window/BrowsingContext objects only','allRequiredPass':False};counter=0;port=m.port;nativeIds={};bottomOnly=os.environ.get('QA97_API_BOTTOM_ONLY')=='1'
 def checkpoint(): (outdir/'guards.json').write_text(json.dumps(result,indent=2))
 checkpoint()
 # Pin reads to a real original window by its real owned selected-tab BC, not by a fake Window.
 m.set_context('chrome');ownedMarker='qa97-owned-'+str(port);m.execute_script('window.__qa97OwnedWindowMarker=arguments[0]',script_args=[ownedMarker])
 def js(s,args=None):
  m.set_context('chrome')
  wrapper='let es=Services.wm.getEnumerator("navigator:browser"),qaA=null;while(es.hasMoreElements()){let w=es.getNext();w=w.wrappedJSObject||w;if(w.__qa97OwnedWindowMarker==='+json.dumps(ownedMarker)+'){qaA=w;break;}}if(!qaA)throw Error("original owned window unavailable");return(function(window,gBrowser,document,mozInnerScreenX,mozInnerScreenY){return(function(){'+s+'}).apply(null,arguments[5]);})(qaA,qaA.gBrowser,qaA.document,qaA.mozInnerScreenX,qaA.mozInnerScreenY,arguments);'
  return m.execute_script(wrapper,script_args=args or [])
 def native_active():
  return subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','getactivewindow'],env=os.environ.copy(),text=True).strip()
 def snap(tokens):
  c=http.client.HTTPConnection('127.0.0.1',29971,timeout=8);c.request('GET','/snapshot?tokens='+urllib.parse.quote(','.join(tokens)));response=c.getresponse();d=json.loads(response.read());c.close();assert d['fresh'],('stale fixture measurement',d);return d
 def waitpage(token):
  deadline=time.monotonic()+12;error=None
  while time.monotonic()<deadline:
   try:
    d=snap([token]);r=d['values'][token];assert abs(r['root']['top']-1800)<=1 and abs(r['root']['overflow']['top']-1800)<=1 and abs(r['frame']['top']-1800)<=1;return d
   except Exception as e:error=repr(e);time.sleep(.15)
  raise RuntimeError('fixture did not initialize middle positions: '+str(error))
 def token(name):
  nonlocal counter
  counter+=1;return 'g97-'+str(port)+'-'+name+'-'+str(counter)
 def load(name,xid=None):
  t=token(name);url='http://127.0.0.1:29971/cross?token='+t+'&auto=1'
  if xid:xd('windowactivate','--sync',xid);time.sleep(.25)
  evidence={'token':t,'nativeBefore':native_active(),'focusBefore':focusmeta()}
  if xid:xd('windowactivate','--sync',xid);time.sleep(.15);assert native_active()==xid
  evidence['nativeImmediatelyBeforeKeys']=native_active()
  xd('key','ctrl+l');xd('type','--clearmodifiers','--delay','1',url);xd('key','Return')
  try:waitpage(t)
  except Exception as e:
   evidence.update({'error':repr(e),'nativeAfter':native_active(),'focusAfter':focusmeta()});result.setdefault('loadDiagnostics',[]).append(evidence);checkpoint();raise
  evidence.update({'nativeAfter':native_active(),'focusAfter':focusmeta()});result.setdefault('loadDiagnostics',[]).append(evidence);checkpoint();return t
 def focusmeta():
  data=js('''let f=Services.focus,b=f.focusedContentBrowsingContext;function un(x){return x?.wrappedJSObject||x;}function w(x){if(!x)return null;return {chromeContext:x.browsingContext?.id??x.docShell?.browsingContext?.id??null,focused:x.document.hasFocus(),delayedStartupFinished:x.gBrowserInit?.delayedStartupFinished,documentReady:x.document.readyState,controllerAttached:x.__mouseGestureControllerAttached===true,selected:x.gBrowser.selectedBrowser.browsingContext.id,uri:x.gBrowser.selectedBrowser.currentURI.spec,tabs:x.gBrowser.tabs.map(t=>({id:t.linkedBrowser.browsingContext.id,uri:t.linkedBrowser.currentURI.spec}))};}return {activeIsA:un(f.activeWindow)===un(window),activeIsB:!!window.qa97WindowB&&un(f.activeWindow)===un(window.qa97WindowB),focusedContext:b?.id??null,focusedTop:b?.top?.id??null,A:w(window),B:w(window.qa97WindowB)}''')
  data['nativeActiveWindow']=native_active();return data
 def point(which,x=655,y=265):
  return js('let w=arguments[0]==="A"?window:window.qa97WindowB;let r=w.gBrowser.selectedBrowser.getBoundingClientRect();return {x:w.mozInnerScreenX+r.x+arguments[1],y:w.mozInnerScreenY+r.y+arguments[2]}',[which,x,y])
 def framefocus(which):
  p=point(which)
  if which in nativeIds:xd('windowactivate','--sync',nativeIds[which]);time.sleep(.1)
  xd('mousemove','--sync',int(p['x']),int(p['y']),'click','1');time.sleep(.25)
 def gesture(which,direction):
  p=point(which)
  if which in nativeIds:xd('windowactivate','--sync',nativeIds[which]);time.sleep(.1)
  xd('mousemove','--sync',int(p['x']),int(p['y']));xd('mousedown','3');time.sleep(.06)
  try:
   for j in range(1,9):xd('mousemove','--sync',int(p['x']),int(p['y']+(-1 if direction=='top' else 1)*j*18));time.sleep(.02)
   time.sleep(.1);label=js('let w=arguments[0]==="A"?window:window.qa97WindowB;return w.document.getElementById("mouse-gesture-display-container")?.textContent',[which])
  finally:xd('mouseup','3')
  time.sleep(1.1);return label
 def flattened(s):
  d={}
  for t,v in s['values'].items():d[(t,'root')]=v['root']['top'];d[(t,'overflow')]=v['root']['overflow']['top'];d[(t,'frame')]=v['frame']['top']
  return d
 def check(before,after,target,part,direction):
  b=flattened(before);a=flattened(after);v=before['values'][target];maximum=v['root']['max'] if part=='root' else v['frame']['max'];wanted=0 if direction=='top' else maximum
  return {'atExpectedEndpoint':abs(a[(target,part)]-wanted)<=1,'expected':wanted,'actual':a[(target,part)],'nonTargetsUnchanged':all(abs(a[k]-n)<=1 for k,n in b.items() if k!=(target,part)),'beforePositions':{t+':'+p:n for (t,p),n in b.items()},'afterPositions':{t+':'+p:n for (t,p),n in a.items()}}
 def shot(name):
  try:
   from PIL import ImageGrab
   ImageGrab.grab(xdisplay=os.environ['DISPLAY']).save(outdir/(name+'.png'));result['screenshotMethod']='actual X11 root capture without focus changes'
  except Exception as e:
   m.set_context('chrome');(outdir/(name+'-chromeA.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')));result['screenshotMethod']='Marionette Chrome A fallback, not a claim of window B screenshot';result['screenshotCaptureLimitation']=repr(e)
 def selecttab(t):
  p=js('let t=gBrowser.tabs.find(t=>t.linkedBrowser.currentURI.spec.includes(arguments[0]));if(!t)throw Error("owned tab not found");let r=t.getBoundingClientRect();return {x:mozInnerScreenX+r.x+r.width/2,y:mozInnerScreenY+r.y+r.height/2}',[t]);xd('mousemove','--sync',int(p['x']),int(p['y']),'click','1');time.sleep(.25)
 def activate(t):
  ids=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','search','--onlyvisible','--name','KAN97 full '+t],env=os.environ.copy(),text=True).split();assert ids,('native owned window not found',t);xd('windowactivate','--sync',ids[0]);time.sleep(.3);return ids[0]
 # Existing matrix leaves one real selected tab in A. Native Ctrl+T creates the second.
 ta=load('tabA');js('window.qa97TabA=gBrowser.selectedTab');xd('key','ctrl+t');tb=load('tabB');js('window.qa97TabB=gBrowser.selectedTab');framefocus('A')
 for direction in ([] if bottomOnly else ['top','bottom']):
  selecttab(tb);framefocus('A');
  selecttab(ta);time.sleep(.2);selectedSample=focusmeta();xd('key','ctrl+l');time.sleep(.15);chromeFocus=focusmeta();result['sameWindowStaleFocusProbes'].append({'direction':direction,'afterNativeTabClick':selectedSample,'afterNativeUrlbarFocus':chromeFocus,'foreignBackgroundFocusActuallyObserved':bool(chromeFocus['focusedTop'] and chromeFocus['focusedTop']!=chromeFocus['A']['selected'])})
  # Reload through real URL-bar input to establish fresh middle positions for A.
  ta=load('tabA-'+direction);framefocus('A');before=snap([ta,tb]);fm=focusmeta();assert fm['A']['focused'] and fm['focusedTop']==fm['A']['selected']
  js('qa97.popups=0;qa97.events=[]');held=gesture('A',direction);after=snap([ta,tb]);row={'kind':'native-selected-tab-gesture','direction':direction,'input':'normal OS tab header selection and OS gesture','focusBefore':fm,'focusAfter':focusmeta(),'heldLabel':held,'before':before,'after':after,'check':check(before,after,ta,'frame',direction),'popups':js('return qa97.popups'),'events':js('return qa97.events')};result['cases'].append(row);checkpoint();shot('guard-tab-'+direction);print('guard tab',direction,row['check'],flush=True)
 # Open an actual second browser window via normal OS Ctrl+N; never call window.focus().
 selecttab(ta);nativeA=activate(ta);nativePid=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','getwindowpid',nativeA],env=os.environ.copy(),text=True).strip()
 beforeClients=set(subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','search','--onlyvisible','--pid',nativePid],env=os.environ.copy(),text=True).split())
 xd('key','ctrl+n');deadline=time.monotonic()+18
 while time.monotonic()<deadline:
  ready=js('let w=Services.wm.getMostRecentWindow("navigator:browser");if(w&&w!==window&&w.gBrowser&&w.__mouseGestureControllerAttached&&w.gBrowserInit?.delayedStartupFinished&&w.document.readyState==="complete"){window.qa97WindowB=w;return true;}return false')
  if ready:break
  time.sleep(.2)
 else:raise RuntimeError('normal second window did not initialize')
 afterClients=set(subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','search','--onlyvisible','--pid',nativePid],env=os.environ.copy(),text=True).split());newClients=afterClients-beforeClients
 assert newClients,('no new native product window',beforeClients,afterClients)
 candidates=[]
 for xid in newClients:
  geom=subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool','getwindowgeometry','--shell',xid],env=os.environ.copy(),text=True);g=dict(line.split('=',1) for line in geom.splitlines() if '=' in line)
  if int(g.get('WIDTH',0))>200 and int(g.get('HEIGHT',0))>200:candidates.append(xid)
 assert len(candidates)==1,('ambiguous new native window',candidates)
 nativeB=candidates[0];nativeIds.update({'A':nativeA,'B':nativeB});result['nativeWindowMapping']={'A':nativeA,'B':nativeB,'pid':nativePid,'beforeClients':sorted(beforeClients),'afterClients':sorted(afterClients),'beforeFirstLoad':focusmeta()};checkpoint()
 tw=load('windowB',nativeB);framefocus('B');js('let w=window.qa97WindowB;w.__qaGuardPopups=0;w.__qaGuardEvents=[];w.addEventListener("popupshown",e=>{if(e.target.id==="contentAreaContextMenu")w.__qaGuardPopups++},true);for(let n of ["mousedown","mouseup","contextmenu"])w.addEventListener(n,e=>{let r={type:n,trusted:e.isTrusted,x:e.clientX,y:e.clientY};w.__qaGuardEvents.push(r);w.setTimeout(()=>r.prevented=e.defaultPrevented,0)},true)')
 for direction in ([] if bottomOnly else ['top','bottom']):
  tw=load('windowB-'+direction,nativeB);framefocus('B');before=snap([ta,tb,tw]);fm=focusmeta();assert fm['B']['focused'] and fm['focusedTop']==fm['B']['selected']
  js('window.qa97WindowB.__qaGuardPopups=0;window.qa97WindowB.__qaGuardEvents=[]');held=gesture('B',direction);after=snap([ta,tb,tw]);row={'kind':'native-second-window-gesture','direction':direction,'input':'normal OS Ctrl+N window, OS focus and OS gesture','focusBefore':fm,'focusAfter':focusmeta(),'heldLabel':held,'before':before,'after':after,'check':check(before,after,tw,'frame',direction),'popups':js('return window.qa97WindowB.__qaGuardPopups'),'events':js('return window.qa97WindowB.__qaGuardEvents')};result['cases'].append(row);checkpoint();shot('guard-window-'+direction);print('guard window',direction,row['check'],flush=True)
 # Read, never alter, the exported production action module to select its native binding.
 root=Path(os.environ['QA97_PRODUCT_ROOT']);entry='chrome/noraneko/nora-content/assets/js/gestures.js'
 with zipfile.ZipFile(root/'omni.ja') as z:body=z.read(entry).decode()
 match=re.search(r'executeGestureAction\s+as\s+([A-Za-z_$][\w$]*)',body);export=match.group(1) if match else 'executeGestureAction';uri='chrome://noraneko/content/assets/js/gestures.js'
 fixture=outdir/'guard-api.mjs';fixture.write_text('import * as production from '+json.dumps(uri)+';\nconst action=production['+json.dumps(export)+'];\nif(typeof action!=="function")throw new Error("production action API unavailable");\nwindow.__qa97ExistingProductionAction=action;window.__qa97ActionApiReady=true;\n')
 result['productionActionImport']={'module':uri,'exportBinding':export,'unmodifiedModuleSha256':hashlib.sha256(body.encode()).hexdigest(),'fixtureSource':str(fixture),'notReplacement':'imports the existing window-realm module and function; no registry callback/scroll code substituted'}
 js('let h=Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler),f=Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);f.initWithPath(arguments[0]);h.setSubstitution("qa-kan97-full",Services.io.newFileURI(f));let s=document.createElement("script");s.type="module";s.src="resource://qa-kan97-full/guard-api.mjs";document.head.appendChild(s)',[str(outdir)])
 deadline=time.monotonic()+8
 while time.monotonic()<deadline:
  if js('return (window.wrappedJSObject||window).__qa97ActionApiReady===true'):break
  time.sleep(.1)
 else:raise RuntimeError('temporary resource module could not import production API; protection unchanged')
 for direction in (['bottom'] if bottomOnly else ['top','bottom']):
  activate(ta);ta=load('inactive-A-'+direction);activate(tw);framefocus('B');before=snap([ta,tb,tw]);fm=focusmeta();qualified=fm['B']['focused'] and not fm['A']['focused'] and fm['B']['focused'] and fm['focusedTop']==fm['B']['selected'] and fm['focusedTop']!=fm['A']['selected'] and fm['nativeActiveWindow']==nativeB;assert qualified,('real foreign-window focus mismatch not established',fm)
  js('qa97.popups=0;window.qa97WindowB.__qaGuardPopups=0')
  called=js('let w=window.wrappedJSObject||window;return w.__qa97ExistingProductionAction(arguments[0],w)',['gecko-scroll-to-'+direction]);time.sleep(1.1);after=snap([ta,tb,tw]);fa=focusmeta();row={'kind':'production-action-api-foreign-native-window-focus','direction':direction,'input':'explicit API fixture; real inactive target window A and native-focused frame/window B, not a physical gesture on inactive A','qualifiedActualFocusMismatch':qualified,'actionReturned':called,'popups':js('return qa97.popups+window.qa97WindowB.__qaGuardPopups'),'focusBefore':fm,'focusAfter':fa,'before':before,'after':after,'check':check(before,after,ta,'root',direction),'foreignNativeFocusRetained':fa['nativeActiveWindow']==fm['nativeActiveWindow'] and fa['focusedTop']==fm['focusedTop']};result['cases'].append(row);checkpoint();shot('guard-api-'+direction);print('guard API',direction,row['check'],flush=True)
 js('Services.io.getProtocolHandler("resource").QueryInterface(Ci.nsIResProtocolHandler).setSubstitution("qa-kan97-full",null);let w=window.wrappedJSObject||window;delete w.__qa97ExistingProductionAction;delete w.__qa97ActionApiReady')
 result['sameWindowStaleFocusInterpretation']='Native tab/URL-bar observations only. If no mismatch is observed, that exact stale same-window state is not claimed; ordinary native cross-tab isolation and real foreign-window mismatch API guard are separate evidence.'
 result['expectedGuardCount']=1 if bottomOnly else 6;result['allRequiredPass']=all(c['check']['atExpectedEndpoint'] and c['check']['nonTargetsUnchanged'] and c.get('popups',0)==0 and c.get('foreignNativeFocusRetained',True) and c.get('actionReturned',True) for c in result['cases']) and len(result['cases'])==(1 if bottomOnly else 6)
 (outdir/'guards.json').write_text(json.dumps(result,indent=2));return result
