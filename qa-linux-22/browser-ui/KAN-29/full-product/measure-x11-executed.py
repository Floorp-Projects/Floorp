import sys,os,json,time,base64,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(os.environ['QA29_OUTPUT']);port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20,startup_timeout=20);m.start_session();m.set_context('chrome')
def js(code,args=[]):return m.execute_script(code,script_args=args)
def mouse(x,y,click=False):
 a=[{'type':'pointerMove','origin':'viewport','x':int(x),'y':int(y),'duration':100}]
 if click:a +=[{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]
 m._send_message('WebDriver:PerformActions',{'actions':[{'type':'pointer','id':'panel-layout','parameters':{'pointerType':'mouse'},'actions':a}]})
def rect(sel):return js('let e=document.querySelector(arguments[0]);return e?e.getBoundingClientRect().toJSON():null',[sel])
clickTrace=[]
def click(sel,settle=.45):
 r=rect(sel)
 if not r or r['width']<=0 or r['height']<=0:raise RuntimeError('UI target missing '+sel)
 x=r['x']+r['width']/2;y=r['y']+r['height']/2;hit=js('let e=document.elementFromPoint(arguments[0],arguments[1]);return {id:e?.id,tag:e?.localName,classes:String(e?.className),outer:e?.outerHTML?.slice(0,700)}',[x,y]);js('window.qa29UiEvents=[]');mouse(x,y,True);time.sleep(settle);clickTrace.append({'selector':sel,'rect':r,'point':[x,y],'hitBefore':hit,'events':js('return window.qa29UiEvents')})
def snap():return js('''return {direction:getComputedStyle(document.documentElement).direction,rtlUI:window.RTL_UI,hover:document.documentElement.hasAttribute('sidebar-expand-on-hover'),state:SidebarController.getUIState(),startWidth:document.getElementById('browser').style.getPropertyValue('--floorp-panel-start-width'),endWidth:document.getElementById('browser').style.getPropertyValue('--floorp-panel-end-width'),elements:['browser','sidebar-container','panel-sidebar-select-box','panel-sidebar-box','panel-sidebar-splitter','tabbrowser-tabbox','tabbrowser-tabs'].map(id=>{let e=document.getElementById(id);let r=e?.getBoundingClientRect();let c=e&&getComputedStyle(e);return {id,rect:r?.toJSON(),expanded:e?.hasAttribute('sidebar-launcher-expanded'),display:c?.display,visibility:c?.visibility,position:c?.position,order:c?.order,opacity:c?.opacity}})}''')
def away():
 r=rect('#tabbrowser-tabbox');v=js('return {w:innerWidth,h:innerHeight}');x=r['x']+r['width']/2;y=r['y']+r['height']/2;mouse(max(2,min(v['w']-3,x)),max(2,min(v['h']-3,y)));time.sleep(.65)
def side_effects():
 out={'input':'W3C actual tab header click, panel close and actual bookmarks reopen','fixture':'Two controlled real data-page tabs created via gBrowser.addTab; no layout/style repair or focus forcing.'}
 js('if(!window.qa29PaintTabs)window.qa29PaintTabs=["rgb(23, 113, 191)","rgb(211, 83, 37)"].map((color,i)=>gBrowser.addTab("data:text/html,"+encodeURIComponent(`<title>QA29 paint ${i}</title><body style="margin:0;background:${color};color:white"><h1>QA29 paint ${i}</h1>`),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));')
 out['tabs']=[]
 for i,color in [(0,(23,113,191)),(1,(211,83,37))]:
  rr=js('return window.qa29PaintTabs[arguments[0]].getBoundingClientRect().toJSON()',[i]);viewport=js('return {w:innerWidth,h:innerHeight}');reachable=rr['width']>0 and rr['height']>0 and 0<=rr['x']+rr['width']/2<viewport['w'] and 0<=rr['y']+rr['height']/2<viewport['h']
  if not reachable:
   out['tabs'].append({'index':i,'targetRect':rr,'unreachableTarget':True,'actual':{'selected':False,'title':None},'pixelMatches':False});continue
  mouse(rr['x']+rr['width']/2,rr['y']+rr['height']/2,True);time.sleep(.3)
  deadline=time.monotonic()+5
  while time.monotonic()<deadline:
   if js('return gBrowser.selectedBrowser.contentTitle')==f'QA29 paint {i}':break
   time.sleep(.1)
  before=snap();image=base64.b64decode(m.screenshot(format='base64'));name=f"paint-{os.environ['QA_DIR']}-{native}-{panel}-{pinned}-{i}.png";(R/name).write_bytes(image)
  import io
  from PIL import Image
  im=Image.open(io.BytesIO(image)).convert('RGB');tabbox=rect('#tabbrowser-tabbox');scale=im.width/js('return innerWidth');xy=(max(0,min(im.width-1,int((tabbox['x']+tabbox['width']/2)*scale))),max(0,min(im.height-1,int((tabbox['y']+tabbox['height']/2)*scale))));pixel=im.getpixel(xy)
  actual=js('return {selected:gBrowser.selectedTab===window.qa29PaintTabs[arguments[0]],title:gBrowser.selectedBrowser.contentTitle,url:gBrowser.selectedBrowser.currentURI.spec}',[i]);out['tabs'].append({'index':i,'actual':actual,'pixel':pixel,'expectedPixel':color,'pixelPoint':xy,'targetRect':rr,'pixelMatches':tabbox['x']>=0 and tabbox['right']<=viewport['w']+1 and tabbox['width']>50 and max(abs(pixel[k]-color[k]) for k in range(3))<=3,'screenshot':name,'layout':before})
 out['beforeClose']=snap();traceStart=len(clickTrace)
 try:
  click('#panel-sidebar-close');out['closed']=snap();out['closedWidth']=rect('#panel-sidebar-box')['width'];reopenStarted=time.monotonic();click('#default-panel-bookmarks',settle=0);out['reopenSamples']=[]
  for delay in [.3,.8,1.5]:
   time.sleep(max(0,reopenStarted+delay-time.monotonic()));out['reopenSamples'].append({'requestedSeconds':delay,'actualSeconds':time.monotonic()-reopenStarted,'width':rect('#panel-sidebar-box')['width'],'layout':snap()})
  out['reopened']=snap();out['reopenedWidth']=rect('#panel-sidebar-box')['width'];out['closeReopenPass']=out['closedWidth']<20 and abs(out['reopenedWidth']-400)<=1
 except Exception as e:out['closeReopenError']=repr(e);out['closeReopenPass']=False
 out['closeReopenClickTrace']=clickTrace[traceStart:];out['tabClicksSelect']=all(x['actual']['selected'] and x['actual']['title']==f"QA29 paint {x['index']}" for x in out['tabs']);out['paintPass']=all(x['pixelMatches'] for x in out['tabs']);return out
r={'scenario':'KAN29 panel-open repeat hover/native-pin','input':'W3C synthetic UI mouse; not physical','requestedDirection':os.environ.get('QA_DIR','ltr'),'fixtures':['Startup prefs select vertical tabs/expand-on-hover and design; live prefs and SidebarController.setPosition select side. Panel open and launcher pin use actual UI clicks. Only actual localized direction is accepted; no root-dir fallback.'],'observations':[]}
try:
 r['product']=js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,platformBuild:Services.appinfo.platformBuildID,gre:Services.dirsvc.get("GreD",Ci.nsIFile).path,os:Services.appinfo.OS}');expected=json.loads(Path(os.environ['QA29_MANIFEST']).read_text())['live'];assert all(r['product'][k]==v for k,v in expected.items()),(r['product'],expected)
 r['graphics']=m.execute_async_script('let done=arguments[arguments.length-1];ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs").Troubleshoot.snapshot().then(s=>done({windowProtocol:s.graphics.windowProtocol,compositing:s.graphics.windowLayerManagerType}));')
 expectedProtocol=json.loads(Path(os.environ['QA29_MANIFEST']).read_text()).get('expectedProtocol','wayland');r['expectedProtocol']=expectedProtocol
 if r['graphics']['windowProtocol']!=expectedProtocol:raise RuntimeError('actual native protocol mismatch')
 js('window.qa29UiEvents=[];for(let type of ["mousedown","mouseup","click"])window.addEventListener(type,e=>qa29UiEvents.push({type:e.type,trusted:e.isTrusted,button:e.button,target:e.target.id||String(e.target.className),path:e.composedPath().slice(0,5).map(x=>x.id||String(x.className)||x.nodeName)}),true)')
 r['initialDirection']=js('return {direction:getComputedStyle(document.documentElement).direction,rtlUI:window.RTL_UI,appLocale:Services.locale.appLocaleAsBCP47,appLocaleRTL:Services.locale.isAppLocaleRTL,uidirection:Services.prefs.getPrefType("intl.uidirection")===64?Services.prefs.getIntPref("intl.uidirection"):Services.prefs.getStringPref("intl.uidirection","")}')
 if os.environ.get('QA_REQUIRE_RTL') and not (r['initialDirection']['rtlUI'] is True and r['initialDirection']['appLocaleRTL'] is True and r['initialDirection']['direction']=='rtl'):raise RuntimeError('Actual localized RTL required; fixture fallback prohibited')
 if r['initialDirection']['direction']!=r['requestedDirection']:raise RuntimeError('actual locale direction mismatch; fallback prohibited')
 js('CustomizableUI.addWidgetToArea("sidebar-button",CustomizableUI.AREA_NAVBAR,0)');r['fixtures'].append('CustomizableUI.addWidgetToArea places the standard Sidebar widget in nav-bar; pin/unpin then uses W3C UI click.')
 time.sleep(1)
 for native in [True,False]:
  for panel in [True,False]:
   js('Services.prefs.setBoolPref("sidebar.position_start",arguments[0]);SidebarController.setPosition();let c=JSON.parse(Services.prefs.getStringPref("floorp.panelSidebar.config"));c.position_start=arguments[1];Services.prefs.setStringPref("floorp.panelSidebar.config",JSON.stringify(c));',[native,panel]);time.sleep(.7)
   configured=js('return {native:Services.prefs.getBoolPref("sidebar.position_start"),panel:JSON.parse(Services.prefs.getStringPref("floorp.panelSidebar.config")).position_start,direction:getComputedStyle(document.documentElement).direction,RTL_UI:window.RTL_UI}')
   if configured['native']!=native or configured['panel']!=panel or configured['direction']!=r['requestedDirection']:raise RuntimeError('configuration/locale precondition lost: '+str(configured))
   for pinned in [False,True]:
    if os.environ.get('QA29_SINGLE_STATE') and [int(native),int(panel),int(pinned)]!=[int(x) for x in os.environ['QA29_SINGLE_STATE'].split(',')]:continue
    obs={'nativePositionStart':native,'panelPositionAtEndPref':panel,'requestedNativePin':pinned,'configured':configured,'panelOpenedBy':'W3C click #default-panel-bookmarks','rounds':[]}
    try:
     panelRect=rect('#panel-sidebar-box')
     if not panelRect or panelRect['width']<20:click('#default-panel-bookmarks')
     away();hover=js('return document.documentElement.hasAttribute("sidebar-expand-on-hover")')
     if hover==pinned:click('#sidebar-button')
     away();obs['actualNativePin']=not js('return document.documentElement.hasAttribute("sidebar-expand-on-hover")');obs['pinEstablished']=obs['actualNativePin']==pinned
     for repeat in range(3):
      try:
       away();collapsed=snap();a=rect('#sidebar-container');v=js('return {w:innerWidth,h:innerHeight}');x=max(2,min(v['w']-3,a['x']+min(20,a['width']/2)));y=a['y']+min(180,a['height']/2)
       if a['width']<=0 or a['x']+a['width']<=0 or a['x']>=v['w']:obs['rounds'].append({'collapsed':collapsed,'unreachableAnchor':True});continue
       mouse(x,y);samples=[]
       for n in range(4):time.sleep(.3);samples.append(snap())
       boxes=[next(e['rect'] for e in q['elements'] if e['id']=='tabbrowser-tabbox') for q in samples];panels=[next(e['rect'] for e in q['elements'] if e['id']=='panel-sidebar-box') for q in samples]
       obs['rounds'].append({'collapsed':collapsed,'hoverPoint':[x,y],'samples':samples,'tabboxWithinViewport':all(b['x']>=-1 and b['right']<=v['w']+1 and b['width']>50 for b in boxes),'panel400':all(abs(b['width']-400)<=1 for b in panels),'lateHorizontalMotion':max(abs(b['x']-boxes[-1]['x']) for b in boxes[1:])})
      except Exception as e:obs['rounds'].append({'measurementError':repr(e),'traceback':traceback.format_exc()})
     try:obs['sideEffects']=side_effects()
     except Exception as e:obs['sideEffects']={'measurementError':repr(e),'traceback':traceback.format_exc()}
     obs['panelWidthAfterSideEffects']=rect('#panel-sidebar-box')['width']
    except Exception as e:obs['stateInteractionError']=repr(e);obs['traceback']=traceback.format_exc()
    try:
     name=f"{r['requestedDirection']}-{port}-{int(native)}-{int(panel)}-{int(pinned)}";(R/(name+'.png')).write_bytes(base64.b64decode(m.screenshot(format='base64')));obs['screenshot']=name+'.png';obs['finalLayout']=snap()
    except Exception as e:obs['captureError']=repr(e)
    effects=obs.get('sideEffects',{});obs['productPass']=obs.get('pinEstablished',False) and len(obs['rounds'])==3 and all(q.get('tabboxWithinViewport',False) and q.get('panel400',False) and q.get('lateHorizontalMotion',99)<=1 for q in obs['rounds']) and effects.get('tabClicksSelect',False) and effects.get('paintPass',False) and effects.get('closeReopenPass',False)
    r['observations'].append(obs);(R/f"measure-{r['requestedDirection']}-{port}.json").write_text(json.dumps(r,indent=2))
 r['expectedStateCount']=1 if os.environ.get('QA29_SINGLE_STATE') else 8;r['collectionComplete']=len(r['observations'])==r['expectedStateCount'];r['productPass']=r['collectionComplete'] and all(o['productPass'] for o in r['observations']);r['hoverRoundsCollected']=sum(len(o['rounds']) for o in r['observations'])

except Exception as e:r['error']=repr(e);r['traceback']=traceback.format_exc()
finally:
 (R/f"measure-{r['requestedDirection']}-{port}.json").write_text(json.dumps(r,indent=2));print(json.dumps({'error':r.get('error'),'graphics':r.get('graphics'),'initialDirection':r.get('initialDirection'),'observations':len(r['observations'])}));m.delete_session()
if r.get('error') or len(r['observations'])!=(1 if os.environ.get('QA29_SINGLE_STATE') else 8):raise SystemExit(1)
