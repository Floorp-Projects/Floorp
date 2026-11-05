import sys,os,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=20);m.start_session();m.set_context('chrome')
def js(code,args=[]):return m.execute_script(code,script_args=args)
def mouse(x,y,click=False):
 a=[{'type':'pointerMove','origin':'viewport','x':int(x),'y':int(y),'duration':100}]
 if click:a +=[{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]
 m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'panel-layout','parameters':{'pointerType':'mouse'},'actions':a}]})
def rect(sel):return js('let e=document.querySelector(arguments[0]);return e?e.getBoundingClientRect().toJSON():null',[sel])
def click(sel):
 r=rect(sel)
 if not r or r['width']<=0 or r['height']<=0:raise RuntimeError('UI target missing '+sel)
 mouse(r['x']+r['width']/2,r['y']+r['height']/2,True);time.sleep(.45)
def snap():return js('''return {direction:getComputedStyle(document.documentElement).direction,rtlUI:window.RTL_UI,hover:document.documentElement.hasAttribute('sidebar-expand-on-hover'),state:SidebarController.getUIState(),startWidth:document.getElementById('browser').style.getPropertyValue('--floorp-panel-start-width'),endWidth:document.getElementById('browser').style.getPropertyValue('--floorp-panel-end-width'),elements:['browser','sidebar-container','panel-sidebar-select-box','panel-sidebar-box','panel-sidebar-splitter','tabbrowser-tabbox','tabbrowser-tabs'].map(id=>{let e=document.getElementById(id);let r=e?.getBoundingClientRect();let c=e&&getComputedStyle(e);return {id,rect:r?.toJSON(),expanded:e?.hasAttribute('sidebar-launcher-expanded'),display:c?.display,visibility:c?.visibility,position:c?.position,order:c?.order,opacity:c?.opacity}})}''')
def away():
 r=rect('#tabbrowser-tabbox');mouse(r['x']+r['width']/2,r['y']+r['height']/2);time.sleep(.65)
r={'scenario':'KAN29 panel-open repeat hover/native-pin','input':'W3C synthetic UI mouse; not physical','requestedDirection':os.environ.get('QA_DIR','ltr'),'fixtures':['Startup prefs select vertical tabs/expand-on-hover and design; live prefs and SidebarController.setPosition select side. Panel open and launcher pin use actual UI clicks. RTL fallback, if needed, is explicitly recorded.'],'observations':[]}
try:
 r['product']=js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS}')
 r['graphics']=m.execute_async_script('let done=arguments[arguments.length-1];ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs").Troubleshoot.snapshot().then(s=>done({windowProtocol:s.graphics.windowProtocol,compositing:s.graphics.windowLayerManagerType}));')
 r['initialDirection']=js('return {direction:getComputedStyle(document.documentElement).direction,rtlUI:window.RTL_UI,appLocale:Services.locale.appLocaleAsBCP47,appLocaleRTL:Services.locale.isAppLocaleRTL,uidirection:Services.prefs.getPrefType("intl.uidirection")===64?Services.prefs.getIntPref("intl.uidirection"):Services.prefs.getStringPref("intl.uidirection","")}')
 if os.environ.get('QA_REQUIRE_RTL') and not (r['initialDirection']['rtlUI'] is True and r['initialDirection']['appLocaleRTL'] is True and r['initialDirection']['direction']=='rtl'):raise RuntimeError('Actual localized RTL required; fixture fallback prohibited')
 if r['initialDirection']['direction']!=r['requestedDirection']:
  js('document.documentElement.setAttribute("dir",arguments[0])',[r['requestedDirection']]);r['fixtures'].append('Root dir attribute fallback: synthetic direction layout, not a localized RTL session.')
 js('CustomizableUI.addWidgetToArea("sidebar-button",CustomizableUI.AREA_NAVBAR,0)');r['fixtures'].append('CustomizableUI.addWidgetToArea places the standard Sidebar widget in nav-bar; pin/unpin then uses W3C UI click.')
 time.sleep(1)
 for native in ([] if os.environ.get("QA_PROBE_ONLY") else ([True] if os.environ.get("QA_SHORT") else [True,False])):
  for panel in ([False] if os.environ.get("QA_SHORT") else [True,False]):
   js('Services.prefs.setBoolPref("sidebar.position_start",arguments[0]);SidebarController.setPosition();let c=JSON.parse(Services.prefs.getStringPref("floorp.panelSidebar.config"));c.position_start=arguments[1];Services.prefs.setStringPref("floorp.panelSidebar.config",JSON.stringify(c));',[native,panel]);time.sleep(.7)
   if rect('#panel-sidebar-box')['width']<20:click('#default-panel-bookmarks')
   for pinned in ([False] if os.environ.get("QA_SHORT") else [False,True]):
    away()
    hover=js('return document.documentElement.hasAttribute("sidebar-expand-on-hover")')
    if hover==pinned:click('#sidebar-button')
    away()
    obs={'nativePositionStart':native,'panelPositionAtEndPref':panel,'requestedNativePin':pinned,'panelOpenedBy':'W3C click #default-panel-bookmarks','rounds':[]}
    for repeat in range(1 if os.environ.get("QA_SHORT") else 3):
     away();collapsed=snap();a=rect('#sidebar-container');w=js('return innerWidth');x=max(2,min(w-3,a['x']+min(20,a['width']/2)));y=a['y']+min(180,a['height']/2)
     if a['width']<=0 or a['x']+a['width']<=0 or a['x']>=w:obs['rounds'].append({'collapsed':collapsed,'unreachableAnchor':True});continue
     mouse(x,y);samples=[]
     for n in range(4):time.sleep(.3);samples.append(snap())
     obs['rounds'].append({'collapsed':collapsed,'hoverPoint':[x,y],'samples':samples})
    name=f'KAN-29/wayland-{port}-{int(native)}-{int(panel)}-{int(pinned)}';(R/(name+'.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')));obs['screenshot']=name+'.png';r['observations'].append(obs)
except Exception as e:r['error']=repr(e);r['traceback']=traceback.format_exc()
finally:
 (R/f'KAN-29/wayland-panel-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps({'error':r.get('error'),'graphics':r.get('graphics'),'initialDirection':r.get('initialDirection'),'observations':len(r['observations'])}));m.delete_session()
