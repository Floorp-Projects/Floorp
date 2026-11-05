import json,time,argparse,collections
from pathlib import Path
from control import send
p=argparse.ArgumentParser();p.add_argument('case');p.add_argument('--port',type=int,default=19190);a=p.parse_args();r=Path('/workspace/qa-linux-22/C-extra/KAN-77/trials')/a.case;base='http://127.0.0.1:'+str(a.port);measure=json.loads((r/"measurements.json").read_text()) if (r/"measurements.json").exists() else []
def req():return json.loads((r/'http-requests.json').read_text())
def phase(id,**kw):
 if any(x['phase']==id for x in measure):return
 originalId=id
 if (r/('result-'+id+'.json')).exists():id+='-retry'
 before=len(req());t=time.time();v=send(a.case,id,**kw);assert 'errorType' not in v,(id,v);time.sleep(3);state=send(a.case,id+'-state');assert 'errorType' not in state
 after=req();counts=dict(collections.Counter(x['path'] for x in after[before:]));measure.append({'phase':originalId,'commandId':id,'fromIndex':before,'toIndex':len(after),'startUnix':t,'endUnix':time.time(),'counts':counts,'actionResult':v,'state':state});(r/'measurements.json').write_text(json.dumps(measure,indent=2)+'\n');print(id,counts,flush=True)
if measure:
 print(send(a.case,'resume-window-references',script='window.__kan77B=Array.from(gBrowser.tabs).find(t=>t.linkedBrowser.currentURI.spec.includes("/page/B"));window.__kan77C=Array.from(Services.wm.getEnumerator("navigator:browser")).find(w=>w!==window);return {hasB:!!window.__kan77B,hasC:!!window.__kan77C};'))
phase('initial-settled',script='return true;')
phase('background-B',script="window.__kan77B=gBrowser.addTab(arguments[0]+'/page/B',{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});return true;".replace('arguments[0]',json.dumps(base)))
phase('subframes-four',target='/page/A',context='content',script="for(let i=0;i<4;i++){let f=document.createElement('iframe');f.src='/frame/'+i;document.body.appendChild(f)}return document.querySelectorAll('iframe').length;")
phase('new-window-create',script='window.__kan77C=OpenBrowserWindow();return true;')
phase('other-window-C',script='const w=window.__kan77C;if(!w?.gBrowser)throw Error("new window not ready");w.gBrowser.selectedBrowser.loadURI(Services.io.newURI('+json.dumps(base+'/page/C')+'),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});return true;')
phase('two-window-background',script='gBrowser.getBrowserForTab(window.__kan77B).loadURI(Services.io.newURI('+json.dumps(base+'/page/B?second=1')+'),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});return true;')
phase('select-B',script='gBrowser.selectedTab=window.__kan77B;return true;')
phase('reload-B',script='gBrowser.selectedBrowser.reload();return true;')
phase('no-manifest',script='gBrowser.selectedBrowser.loadURI(Services.io.newURI('+json.dumps(base+'/page/none')+'),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});return true;')
phase('iframe-only',script='gBrowser.selectedBrowser.loadURI(Services.io.newURI('+json.dumps(base+'/page/iframeonly')+'),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});return true;')
phase('ineligible-about',script='gBrowser.selectedBrowser.loadURI(Services.io.newURI("about:blank"),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});return true;')
phase('return-A',script='gBrowser.selectedTab=Array.from(gBrowser.tabs).find(t=>t.linkedBrowser.currentURI.spec.endsWith("/page/A"));return true;',screenshot='return-A.png')
phase('open-real-pwa-panel',click='#ssbPageAction',script='return true;')
phase('panel-metadata',script='return {selected:gBrowser.selectedBrowser.currentURI.spec,panelState:document.getElementById("ssb-panel")?.state,title:document.getElementById("ssb-content-label")?.textContent,description:document.getElementById("ssb-content-description")?.textContent,installButton:document.getElementById("ssb-app-install-button")?.textContent,installLabel:document.getElementById("ssb-app-install-button")?.getAttribute("label")};',screenshot='panel.png')
phase('close-panel',nativeKeys=['Escape'])
phase('close-other-window',script='window.__kan77C.close();return true;')
print(send(a.case,'quit',stop=True),flush=True)
