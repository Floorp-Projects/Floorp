from pathlib import Path
import sys,os,time,json,subprocess,base64
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);product=sys.argv[2];m=Marionette('localhost',port=port,socket_timeout=30);m.start_session();m.set_context('chrome')
def js(s):return m.execute_script(s)
def xd(*args):return subprocess.check_output(['/workspace/.floorp-cloud/sysroot/usr/bin/xdotool',*map(str,args)],env=os.environ,text=True)
r={'key':'KAN-95','product':product,'input':'OS XTest synthetic click; not physical input','cssFixture':False}
try:
 r['defaultDesign']=js('return Services.prefs.getStringPref("floorp.design.configs","")') if product=='floorp' else None
 js('window.maximize();window.qaA=gBrowser.addTab("data:text/html,<title>QA A</title>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});window.qaB=gBrowser.addTab("data:text/html,<title>QA B</title>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});gBrowser.selectedTab=qaA');time.sleep(1)
 r['geometry']=js('let e=qaB,b=e.querySelector(".tab-background");return{tab:e.getBoundingClientRect().toJSON(),background:b.getBoundingClientRect().toJSON(),screen:{x:mozInnerScreenX,y:mozInnerScreenY,screenY,sizemode:document.documentElement.getAttribute("sizemode")},css:{backgroundMarginTop:getComputedStyle(b).marginTop},gtkCsdAvailable:matchMedia("(-moz-gtk-csd-available)").matches}')
 try:r['matchedMarginRules']=js('return [...InspectorUtils.getCSSStyleRules(qaB.querySelector(".tab-background"))].filter(r=>r.style&&(r.style.marginTop||r.style.marginBlock||r.style.margin)).map(r=>({selector:r.selectorText,margin:r.style.margin,marginTop:r.style.marginTop,marginBlock:r.style.marginBlock,source:r.parentStyleSheet?.href}))')
 except Exception as e:r['ruleInspectionError']=str(e)
 q=r['geometry'];x=int(q['screen']['x']+q['tab']['x']+q['tab']['width']/2);y=int(q['screen']['y']+q['tab']['y'])
 r['before']=js('return gBrowser.selectedTab===qaB');xd('mousemove','--sync',x,y);xd('click',1);time.sleep(.4);r['clientTabTopClick']={'screenX':x,'screenY':y,'selectedTarget':js('return gBrowser.selectedTab===qaB')}
 js('gBrowser.selectedTab=qaA');xd('mousemove','--sync',x,int(q['screen']['y']+q['tab']['y']+q['tab']['height']/2));xd('click',1);time.sleep(.4);r['centerControlSelected']=js('return gBrowser.selectedTab===qaB')
 # Real screen edge hits OS titlebar in this environment, recorded separately.
 js('gBrowser.selectedTab=qaA');xd('mousemove','--sync',x,0);xd('click',1);time.sleep(.3);r['absoluteScreenEdgeSelected']=js('return gBrowser.selectedTab===qaB')
 r['ewmh']=subprocess.check_output(['xprop','-id',xd('getactivewindow').strip(),'_NET_WM_STATE','_NET_FRAME_EXTENTS'],env=os.environ,text=True)
 (R/'screenshots'/f'{product}-{port}-edge.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
except Exception as e:r['error']=str(e)
finally:(R/f'{product}-edge-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r));m.delete_session()
