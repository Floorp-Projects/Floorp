import os, sys, subprocess, time, json, traceback
from pathlib import Path
P=Path(__file__).parent
sys.path.insert(0,str(P/'python'))
from marionette_driver.marionette import Marionette
from marionette_driver.addons import Addons
profile=P/'fresh-home/profile'
profile.mkdir(parents=True,exist_ok=True)
(profile/'user.js').write_text('user_pref("marionette.port",28664);\nuser_pref("browser.shell.checkDefaultBrowser",false);\nuser_pref("browser.aboutwelcome.enabled",false);\n')
env=os.environ.copy()
for k in ['MOZ_DISABLE_CONTENT_SANDBOX','MOZ_DISABLE_RDD_SANDBOX','MOZ_DISABLE_GMP_SANDBOX','MOZ_DISABLE_GPU_SANDBOX']:
    env.pop(k,None)
env.update(HOME=str(P/'fresh-home'),XDG_CONFIG_HOME=str(P/'fresh-home/config'),XDG_CACHE_HOME=str(P/'fresh-home/cache'),XDG_DATA_HOME=str(P/'fresh-home/data'),XDG_RUNTIME_DIR=str(P/'runtime'),DISPLAY=':164',LD_LIBRARY_PATH='/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu',MOZ_APP_REMOTINGNAME='floorp-kan64',MOZ_REMOTE_ALLOW_SYSTEM_ACCESS='1')
x=subprocess.Popen(['/workspace/.floorp-cloud/bin/Xvfb',':164','-screen','0','1440x1000x24','-nolisten','tcp'],env=env,stdout=open(P/'xvfb.log','w'),stderr=subprocess.STDOUT)
time.sleep(2)
b=subprocess.Popen(['dbus-run-session','--','/workspace/qa-linux-22/C/products/floorp/floorp','--no-remote','--profile',str(P/'fresh-home/profile'),'--marionette','about:blank'],env=env,stdout=open(P/'browser.log','w'),stderr=subprocess.STDOUT)
try:
    m=Marionette(host='127.0.0.1',port=28664);m.raise_for_port(timeout=45);m.start_session();m.set_context('chrome')
    time.sleep(12)
    out={'capabilities':m.session_capabilities,'startup':m.execute_script('return {floorp:!!window.gFloorp,keys:Object.keys(window.gFloorp||{}),groups:gBrowser.tabGroups.length};')}
    print(json.dumps(out,indent=2),flush=True)
    out['addon']=Addons(m).install(str(P/'treestyletab.xpi'),temp=False)
    time.sleep(10)
    out['signature']=m.execute_async_script("const done=arguments[arguments.length-1];ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs').AddonManager.getAddonByID('treestyletab@piro.sakura.ne.jp').then(a=>done({id:a.id,version:a.version,signedState:a.signedState,isActive:a.isActive,appDisabled:a.appDisabled}));")
    out['security']=m.execute_script("return {sandboxLevel:Services.prefs.getIntPref('security.sandbox.content.level'),signaturesRequired:Services.prefs.getBoolPref('xpinstall.signatures.required'),allowInsecureCertificates:false};")
    out['inspect']=m.execute_script('return {keys:Object.keys(window.gFloorp||{}),extensions:Services.prefs.getStringPref("extensions.webextensions.uuids", ""),tabs:Array.from(gBrowser.tabs).map(t=>({url:t.linkedBrowser.currentURI.spec,hidden:t.hidden}))};')
    (P/'probe.json').write_text(json.dumps(out,indent=2));print(json.dumps(out['inspect'],indent=2),flush=True)
    out['workspaceApi']=m.execute_script('return {funcs:Object.keys(window.workspacesFuncs||{}),sidebar:Array.from(document.querySelectorAll("browser")).map(b=>({id:b.id,src:b.getAttribute("src")}))};')
    print(json.dumps(out['workspaceApi']),flush=True)
    out['sidebarShown']=m.execute_async_script("const done=arguments[arguments.length-1];const id=Array.from(SidebarController.sidebars.keys()).find(k=>k.includes('treestyletab'));SidebarController.show(id).then(()=>done({id,keys:Array.from(SidebarController.sidebars.keys())}));")
    time.sleep(3)
    out['created']=m.execute_script("""const a=workspacesFuncs.getSelectedWorkspaceID(); const ts=[1,2].map(i=>gBrowser.addTab('data:text/html,<title>KAN64-A'+i+'</title>Workspace A '+i,{triggeringPrincipal:Services.scriptSecurityManager.getSystemPrincipal()})); const g=gBrowser.addTabGroup(ts,{label:'KAN64-GROUP-A',color:'blue'});window.kan64={a,ts,g}; return {a,group:g.id,tabs:ts.map(t=>({id:t.id,ws:t.getAttribute('floorpWorkspaceId')}))};""")
    time.sleep(3);(P/'workspace-a.png').write_bytes(m.screenshot(format='binary'))
    out['before']=m.execute_script("return {group:{display:kan64.g.style.display,hidden:kan64.g.hidden,tabs:Array.from(kan64.g.tabs).map(t=>({hidden:t.hidden,ws:t.getAttribute('floorpWorkspaceId')}))},browsers:Array.from(document.querySelectorAll('browser')).map(b=>({id:b.id,src:b.getAttribute('src')}))};")
    out['switch']=m.execute_script("const b=workspacesFuncs.createNoNameWorkspace(); workspacesFuncs.changeWorkspace(b);kan64.b=b;return {a:kan64.a,b,selected:workspacesFuncs.getSelectedWorkspaceID()};")
    time.sleep(3);(P/'workspace-b.png').write_bytes(m.screenshot(format='binary'))
    out['after']=m.execute_script("return {group:{display:kan64.g.style.display,hidden:kan64.g.hidden,tabs:Array.from(kan64.g.tabs).map(t=>({hidden:t.hidden,ws:t.getAttribute('floorpWorkspaceId')}))},browsers:Array.from(document.querySelectorAll('browser')).map(b=>({id:b.id,src:b.getAttribute('src')}))};")
    out['cycles']=[]
    for i in range(2):
        m.execute_script("workspacesFuncs.changeWorkspace(kan64.a);")
        time.sleep(1)
        a=m.execute_script("return {display:kan64.g.style.display,tabs:kan64.ts.map(t=>t.hidden)};")
        m.execute_script("workspacesFuncs.changeWorkspace(kan64.b);")
        time.sleep(1)
        bstate=m.execute_script("return {display:kan64.g.style.display,tabs:kan64.ts.map(t=>t.hidden)};")
        out['cycles'].append({'returnA':a,'switchB':bstate})
    (P/'workspace-b-final.png').write_bytes(m.screenshot(format='binary'))
    (P/'probe.json').write_text(json.dumps(out,indent=2));print(json.dumps(out,indent=2),flush=True)


except Exception:
    (P/'error.txt').write_text(traceback.format_exc());raise
finally:
    b.terminate();x.terminate()
    try:b.wait(timeout=10)
    except subprocess.TimeoutExpired:b.kill()
