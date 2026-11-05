import os,sys,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[1]);scenario=sys.argv[2];m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=15);m.start_session();m.set_context('chrome')
def js(code,args=[]):return m.execute_script(code,script_args=args)
def shot(name):(R/'screenshots'/f'{port}-{name}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
def inspect():return js('return [...document.querySelectorAll("#default-panel-bookmarks,#panel-sidebar-box,#panel-sidebar-select-box,#panel-sidebar-header,#panel-sidebar-close,#zen-mode-button,#toggle_zenmode,#menu_ToolsPopup")].map(e=>({id:e.id,localName:e.localName,role:e.getAttribute("role"),label:e.getAttribute("label"),rect:e.getBoundingClientRect().toJSON(),parent:e.parentElement?.id,hidden:e.hidden,display:getComputedStyle(e).display,visibility:getComputedStyle(e).visibility}))')
r={'scenario':scenario,'product':js('return {version:Services.appinfo.version,build:Services.appinfo.appBuildID,name:Services.appinfo.name,os:Services.appinfo.OS}'),'observations':[]}
try:
 r['wayland']=m.execute_async_script('ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs").Troubleshoot.snapshot().then(x=>arguments[arguments.length-1]({windowProtocol:x.graphics.windowProtocol,compositor:x.graphics.windowLayerManagerType,adapter:x.graphics.adapterDescription,application:x.application}));',script_timeout=20000)
 r['observations'].append({'dom':inspect(),'panelConfig':js('return Services.prefs.getStringPref("floorp.panelSidebar.config","")')});shot('initial')
 if scenario=='paint':
  def pointer(x,y,click=False):
   seq=[{'type':'pointerMove','origin':'viewport','x':int(x),'y':int(y),'duration':120}]
   if click:seq.extend([{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}])
   m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'qa-zen-pointer','parameters':{'pointerType':'mouse'},'actions':seq}]})
  def click(sel):
   q=js('let e=document.querySelector(arguments[0]);let r=e.getBoundingClientRect();return {x:r.x+r.width/2,y:r.y+r.height/2,width:r.width,height:r.height}',[sel]);assert q['width']>0 and q['height']>0,(sel,q);pointer(q['x'],q['y'],True);time.sleep(.35)
  def state():return js('return {attrs:[...document.documentElement.attributes].filter(a=>a.name.startsWith("zenmode")).map(a=>[a.name,a.value]),elements:["#navigator-toolbox","#TabsToolbar","#nav-bar","#panel-sidebar-box","#panel-sidebar-select-box","#panel-sidebar-header"].map(s=>{let e=document.querySelector(s),c=getComputedStyle(e);return {selector:s,rect:e.getBoundingClientRect().toJSON(),color:c.color,backgroundColor:c.backgroundColor,backgroundImage:c.backgroundImage,opacity:c.opacity,zIndex:c.zIndex,transform:c.transform,display:c.display,pointerEvents:c.pointerEvents}}),panelBrowsers:[...document.querySelectorAll("#panel-sidebar-box browser")].map(e=>({id:e.id,url:e.currentURI?.spec,rect:e.getBoundingClientRect().toJSON()}))}')
  # Use a test page that extends behind the Zen overlay. This is content fixture only.
  m.set_context('content');m.navigate('data:text/html,<title>QA Zen Paint</title><body style="margin:0;background:rgb(255,0,64);height:2000px"><h1>QA backdrop</h1>');m.set_context('chrome');time.sleep(.6)
  click('#default-panel-bookmarks');r['observations'].append({'phase':'panel-ui-open','state':state()});shot('panel-ui-open')
  # Real keyboard/menu interaction rather than calling the Zen controller.
  m._send_message('WebDriver:PerformActions',{'actions':[{'type':'key','id':'qa-zen-keyboard','actions':[{'type':'keyDown','value':'\ue00a'},{'type':'keyDown','value':'t'},{'type':'keyUp','value':'t'},{'type':'keyUp','value':'\ue00a'}]}]});time.sleep(.4)
  r['observations'].append({'phase':'tools-menu','dom':inspect()})
  click('#toggle_zenmode');time.sleep(.8)
  assert js('return document.documentElement.hasAttribute("zenmode")'),'Zen menu did not enable'
  r['fixtures']=['synthetic W3C keyboard Alt+T and pointer click on Tools > Toggle Zen Mode','synthetic W3C click on default-panel-bookmarks to open real panel body','test data URL with red/cyan backdrop','temporary theme install via AddonManager.installTemporaryAddon; no signing/security preferences changed']
  for theme in ['default','color','image']:
   if theme!='default':
    r.setdefault('themeInstalls',[]).append(m.execute_async_script('const done=arguments[arguments.length-1];const {AddonManager}=ChromeUtils.importESModule("resource://gre/modules/AddonManager.sys.mjs");let f=Cc["@mozilla.org/file/local;1"].createInstance(Ci.nsIFile);f.initWithPath(arguments[0]);AddonManager.installTemporaryAddon(f).then(async a=>{await a.enable();done({id:a.id,type:a.type,isActive:a.isActive,temporarilyInstalled:a.temporarilyInstalled,signedState:a.signedState})}).catch(e=>done({error:String(e)}))',script_args=[str(R/'theme'/(theme+'.xpi'))],script_timeout=20000));time.sleep(1)
   for backdrop,color in [('red','rgb(255,0,64)'),('cyan','rgb(0,255,192)')]:
    m.set_context('content');js('document.body.style.background=arguments[0]',[color]);m.set_context('chrome')
    dims=js('return {width:innerWidth,height:innerHeight}');pointer(dims['width']-2,2);time.sleep(.45)
    snap=state();r['observations'].append({'phase':'simultaneous','theme':theme,'backdrop':backdrop,'state':snap});shot(theme+'-'+backdrop+'-simultaneous')
    assert any(e['selector']=='#panel-sidebar-box' and e['rect']['width']>100 and e['opacity']=='1' for e in snap['elements']),'panel body not visible'
    assert {a[0] for a in snap['attrs']}>={'zenmode','zenmode-reveal-top','zenmode-reveal-side'},snap['attrs']
  r['completedPaintConditions']=sum(o.get('phase')=='simultaneous' for o in r['observations'])
  assert r['completedPaintConditions']==6

except Exception as e:r.update(error=repr(e),traceback=traceback.format_exc())
finally:(R/f'{port}-{scenario}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r,ensure_ascii=False));m.delete_session()
if r.get('error'):sys.exit(1)
