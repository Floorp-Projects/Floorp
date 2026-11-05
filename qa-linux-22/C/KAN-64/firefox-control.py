import os, sys, subprocess, time, json, traceback
from pathlib import Path
P=Path(__file__).parent
sys.path.insert(0,str(P/'python'))
from marionette_driver.marionette import Marionette
from marionette_driver.addons import Addons
env=os.environ.copy()
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','MOZ_DISABLE_RDD_SANDBOX','MOZ_DISABLE_GMP_SANDBOX','MOZ_DISABLE_GPU_SANDBOX']:
    env.pop(k,None)
env.update(HOME=str(P/'firefox-home'),XDG_CONFIG_HOME=str(P/'firefox-home/config'),XDG_CACHE_HOME=str(P/'firefox-home/cache'),XDG_DATA_HOME=str(P/'firefox-home/data'),XDG_RUNTIME_DIR=str(P/'runtime'),DISPLAY=':165',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',MOZ_APP_REMOTINGNAME='firefox-kan64',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1')
x=subprocess.Popen(['/workspace/.floorp-cloud/bin/Xvfb',':165','-screen','0','1440x1000x24','-nolisten','tcp'],env=env,stdout=open(P/'firefox-xvfb.log','w'),stderr=subprocess.STDOUT)
time.sleep(2)
b=subprocess.Popen(['dbus-run-session','--','/workspace/qa-linux-22/C/products/firefox/firefox','--no-remote','--profile',str(P/'firefox-home/profile'),'--marionette','--remote-allow-system-access','about:blank'],env=env,stdout=open(P/'firefox-browser.log','w'),stderr=subprocess.STDOUT)
try:
    m=Marionette(host='127.0.0.1',port=28665);m.raise_for_port(timeout=45);m.start_session();m.set_context('chrome')
    time.sleep(12)
    out={'capabilities':m.session_capabilities,'startup':m.execute_script('return {floorp:!!window.gFloorp,keys:Object.keys(window.gFloorp||{}),groups:gBrowser.tabGroups.length};')}
    print(json.dumps(out,indent=2),flush=True)
    out['addon']=Addons(m).install(str(P/'treestyletab.xpi'),temp=False)
    time.sleep(10)
    out['inspect']=m.execute_script('return {keys:Object.keys(window.gFloorp||{}),extensions:Services.prefs.getStringPref("extensions.webextensions.uuids", ""),tabs:Array.from(gBrowser.tabs).map(t=>({url:t.linkedBrowser.currentURI.spec,hidden:t.hidden}))};')
    (P/'firefox-control.json').write_text(json.dumps(out,indent=2));print(json.dumps(out['inspect'],indent=2),flush=True)
    out['workspaceApi']=m.execute_script('return {funcs:Object.keys(window.workspacesFuncs||{}),sidebar:Array.from(document.querySelectorAll("browser")).map(b=>({id:b.id,src:b.getAttribute("src")}))};')
    print(json.dumps(out['workspaceApi']),flush=True)
    out['sidebarShown']=m.execute_async_script("const done=arguments[arguments.length-1];const id=Array.from(SidebarController.sidebars.keys()).find(k=>k.includes('treestyletab'));SidebarController.show(id).then(()=>done({id,keys:Array.from(SidebarController.sidebars.keys())}));")
    time.sleep(3)
    out['created']=m.execute_script("""const ts=[1,2].map(i=>gBrowser.addTab('data:text/html,<title>KAN64-A'+i+'</title>Group A '+i,{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})); const g=gBrowser.addTabGroup(ts,{label:'KAN64-GROUP-A',color:'blue'});window.kan64={ts,g}; return {group:g.id};""")
    time.sleep(3);(P/'firefox-before.png').write_bytes(m.screenshot(format='binary'))
    out['hide']=m.execute_script("kan64.ts.forEach(t=>gBrowser.hideTab(t));return {tabs:kan64.ts.map(t=>({hidden:t.hidden})),groupDisplay:kan64.g.style.display,groupHidden:kan64.g.hidden};")
    time.sleep(3);(P/'firefox-after.png').write_bytes(m.screenshot(format='binary'))
    (P/'firefox-control.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2),flush=True)

except Exception:
    (P/'firefox-error.txt').write_text(traceback.format_exc());raise
finally:
    b.terminate();x.terminate()
    try:b.wait(timeout=10)
    except subprocess.TimeoutExpired:b.kill()
