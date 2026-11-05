"""Measure pane layout/hit tests and verify W3C pointer actions reach content."""
import json, os, sys, time, traceback
from pathlib import Path
from marionette_driver.marionette import Marionette
ROOT = Path(__file__).resolve().parent.parent
port = int(sys.argv[2])
m = Marionette('localhost', port=port, socket_timeout=20)
m.start_session(); m.set_context('chrome')
def js(code, args=None): return m.execute_script(code, script_args=args or [])
r = {'scenario':'layout-and-w3c-click', 'input':'synthetic W3C pointer, not physical', 'observations':[]}
try:
    r['product'] = js('return {name:Services.appinfo.name,version:Services.appinfo.version,build:Services.appinfo.appBuildID,os:Services.appinfo.OS}')
    css = Path(os.environ['QA_SOURCE_CSS']).read_text()
    js('document.getElementById("floorp-split-view-styles").textContent=arguments[0]',[css])
    js('''window.qaSets=[];
    for(let j=0;j<3;j++){
      let ts=Array.from({length:j===2?1:2},(_,i)=>{
        let title='pane-'+j+'-'+i;
        let html='<title>'+title+'</title><body style="margin:0;height:100vh" onclick="document.title+=String.fromCharCode(32,99,108,105,99,107,101,100)">'+title+'</body>';
        return gBrowser.addTab('data:text/html,'+encodeURIComponent(html),{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()});
      });
      if(j<2){gBrowser.addTabGroup(ts,{label:'Layout '+j});gBrowser.addTabSplitView(ts,{insertBefore:ts[0]});}
      qaSets.push(ts);
    }''')
    time.sleep(2)
    for phase in ['before-adoption','after-return']:
        if phase=='after-return':
            js('window.qaDestination=gBrowser.replaceGroupWithWindow(qaSets[1][0].group)');time.sleep(2)
            js('qaDestination.document.getElementById("floorp-split-view-styles").textContent=arguments[0]',[css])
            js('qaSets[1]=Array.from(gBrowser.adoptTabGroup(qaDestination.gBrowser.tabGroups[0],{tabIndex:gBrowser.tabs.length}).tabs)');time.sleep(1)
        for group in [0,1,2,1]:
            js('gBrowser.selectedTab=qaSets[arguments[0]][0]',[group]);time.sleep(.5)
            measured=js('''const root=document.getElementById('tabbrowser-tabpanels');
              return {split:root.hasAttribute('data-floorp-split'),selected:gBrowser.selectedTab.linkedBrowser.contentTitle,
                bounds:root.getBoundingClientRect().toJSON(),panes:qaSets.map((tabs,j)=>tabs.map((t,i)=>{
                let p=t.linkedBrowser;while(p.parentElement!==root)p=p.parentElement;
                let r=p.getBoundingClientRect(),s=getComputedStyle(p),hit=document.elementFromPoint(r.x+r.width/2,r.y+r.height/2);
                return {group:j,index:i,rect:r.toJSON(),position:s.position,visibility:s.visibility,ownsCenter:!!hit&&p.contains(hit)};
              }))};''')
            measured.update(phase=phase,activeGroup=group,clicks=[])
            for i,pane in enumerate(measured['panes'][group]):
                rect=pane['rect'];x=int(rect['x']+rect['width']/2);y=int(rect['y']+rect['height']/2)
                previous=js('return qaSets[arguments[0]][arguments[1]].linkedBrowser.contentTitle',[group,i])
                m._send_message('WebDriver:PerformActions', {'actions':[{'type':'pointer','id':'layout-mouse','parameters':{'pointerType':'mouse'},'actions':[{'type':'pointerMove','origin':'viewport','x':x,'y':y,'duration':50},{'type':'pointerDown','button':0},{'type':'pointerUp','button':0}]}]})
                time.sleep(.25)
                title=js('return qaSets[arguments[0]][arguments[1]].linkedBrowser.contentTitle',[group,i])
                measured['clicks'].append({'index':i,'title':title,'passed':title==previous+' clicked'})
            r['observations'].append(measured)
    r['passed']=all(all(c['passed'] for c in a['clicks']) for a in r['observations'])
except Exception as e:
    r.update(error=str(e),traceback=traceback.format_exc(),passed=False)
finally:
    m.delete_session()
    (ROOT/('layout-'+str(port)+'.json')).write_text(json.dumps(r,indent=2))
    print(json.dumps(r))
