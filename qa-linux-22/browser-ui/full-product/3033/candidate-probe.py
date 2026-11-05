import base64,json,sys,time,traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
R=Path(__file__).parent;port=int(sys.argv[2]);m=Marionette('localhost',port=port,socket_timeout=20);m.start_session();m.set_context('chrome')
def js(s):return m.execute_script(s)
def shot(n):(R/f'{port}-{n}.png').write_bytes(base64.b64decode(m.screenshot(format='base64')))
r={'scenario':'candidate-adoption','cssFixture':False,'input':'gBrowser APIs; not UI menu or physical drag','observations':[]}
try:
 r['product']=js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS}')
 r['signaturePreferences']=js('return ["xpinstall.signatures.required","extensions.langpacks.signatures.required"].map(name=>({name,value:Services.prefs.getBoolPref(name,true),hasUserValue:Services.prefs.prefHasUserValue(name),default:Services.prefs.getDefaultBranch("").getBoolPref(name,true)}))')
 r['graphics']=m.execute_async_script('const done=arguments[arguments.length-1];ChromeUtils.importESModule("resource://gre/modules/Troubleshoot.sys.mjs").Troubleshoot.snapshot().then(s=>done({windowProtocol:s.graphics.windowProtocol,compositing:s.graphics.windowLayerManagerType}));')
 js('window.qaSets=[];for(let j=0;j<2;j++){let ts=Array.from({length:2},(_,i)=>gBrowser.addTab("data:text/html,<title>QA split "+j+"-"+i+"</title><h1>QA split "+j+"-"+i+"</h1>",{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()}));let gr=gBrowser.addTabGroup(ts,{label:"QA split "+j});gBrowser.addTabSplitView(ts,{insertBefore:ts[0]});qaSets.push({ts,gr});}gBrowser.selectedTab=qaSets[1].ts[0]');time.sleep(3)
 r['inactivePaneStyles']=js('return qaSets[0].ts.map(t=>{let p=t.linkedBrowser,root=document.getElementById("tabbrowser-tabpanels");while(p.parentElement!==root)p=p.parentElement;let c=getComputedStyle(p);return {position:c.position,visibility:c.visibility}})')
 r['observations'].append({'phase':'before','titles':js('return qaSets[1].ts.map(t=>t.linkedBrowser.contentTitle)')});shot('before')
 js('window.qaNew=gBrowser.replaceGroupWithWindow(qaSets[1].gr)');time.sleep(3)
 r['observations'].append({'phase':'moved','titles':js('return qaNew.gBrowser.tabGroups[0].tabs.map(t=>t.linkedBrowser.contentTitle)')});shot('moved')
 js('window.qaReturned=gBrowser.adoptTabGroup(qaNew.gBrowser.tabGroups[0],{tabIndex:gBrowser.tabs.length})');time.sleep(3)
 r['observations'].append({'phase':'returned','titles':js('return qaReturned.tabs.map(t=>t.linkedBrowser.contentTitle)')});shot('returned')
 r['passed']=all(x['titles']==['QA split 1-0','QA split 1-1'] for x in r['observations'])
except Exception as e:r.update(error=str(e),traceback=traceback.format_exc(),passed=False)
finally:
 m.delete_session();(R/f'candidate-{port}.json').write_text(json.dumps(r,indent=2));print(json.dumps(r))
