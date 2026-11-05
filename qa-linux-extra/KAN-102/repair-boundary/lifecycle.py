import sys,json,time,subprocess,os
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/C-extra/KAN-102');from control import send
r=Path('/workspace/qa-linux-22/C-extra/KAN-102/repair-boundary');kind=sys.argv[1];display=sys.argv[2];case='repair-boundary/'+kind;q=lambda id,s='return true;',**k:send(case,'life-'+id,s,**k)
q('observe',(r/'observers.js').read_text())
cap="const done=arguments[arguments.length-1];setTimeout(async()=>{const result={native:qaState(),extension:await qaExt(),manage:await qaManage(),events:qaEvents.splice(0)};done(result);},1500);"
def snap(id):
 d=q(id,cap,**{'async':True});print(kind,id,json.dumps({'ws':d['native']['workspace'],'selected':d['native']['selected'],'fixtureShown':[t['url'].split('/')[-2:] for t in d['native']['tabs'] if t['url'].startswith('http://127.0.0.1') and '/window2/' not in t['url'] and t['display']!='none'],'groups':d['manage'].get('groups'),'extensionInactive':d['extension'].get('extensionInactive',False)}),flush=True);return d
if (r/(kind+'-workspaces.json')).exists():ws=json.loads((r/(kind+'-workspaces.json')).read_text())
else:
 q('baseline-B',context='content',uri='manage/manage.html',click='.tab[title*="/B/1"]')
 ws=q('baseline-create','const A=workspacesFuncs.getSelectedWorkspaceID(),B=workspacesFuncs.createNoNameWorkspace();return {A,B};');q('baseline-return','workspacesFuncs.changeWorkspace('+json.dumps(ws['A'])+');return true;');(r/(kind+'-workspaces.json')).write_text(json.dumps(ws,indent=2)+'\n')
snap('start')
# Native normal reload of the signed management page, then actual STG row clicks.
q('reload',"const t=Array.from(gBrowser.tabs).find(t=>t.linkedBrowser.currentURI.spec.includes('/manage/manage.html'));t.linkedBrowser.reload();return true;");time.sleep(2);snap('after-reload')
for group in ['A','B']:
 try:q('group-'+group,context='content',uri='manage/manage.html',click='.tab[title*="/'+group+'/1"]')
 except Exception as e:print(kind,'group-row-blocked',group,str(e)[:120],flush=True)
 snap('after-group-'+group)
# Multiple real browser windows, no synthetic TabHide/TabShow or extension-cache mutations.
q('window2-open','window.qaW2=OpenBrowserWindow();return {window1:qaState()};');time.sleep(9)
q('window2-ready','return {window1:qaState(),window2:qaState(qaW2)};')
q('window2-workspace',"window.qaW2Base=qaW2.workspacesFuncs.getSelectedWorkspaceID();window.qaW2New=qaW2.workspacesFuncs.createNoNameWorkspace();return {window1:qaState(),window2:qaState(qaW2)};")
q('window2-return','qaW2.workspacesFuncs.changeWorkspace(qaW2Base);return {window1:qaState(),window2:qaState(qaW2)};')
q('window2-close','qaW2.close();return {window1:qaState()};');time.sleep(1);snap('after-window2')
# Real native keyboard navigation from neutral workspace; compare native lists and DOM display.
q('keyboard-away','workspacesFuncs.changeWorkspace('+json.dumps(ws['B'])+');window.focus();return qaState();');snap('keyboard-before')
env=dict(os.environ,DISPLAY=':'+display,LD_LIBRARY_PATH='/workspace/qa-linux-22/C/KAN-90/sysroot/usr/lib/x86_64-linux-gnu:/workspace/.floorp-cloud/sysroot/usr/lib/x86_64-linux-gnu')
subprocess.run(['/workspace/.floorp-cloud/bin/xdotool','key','Escape'],env=env,check=True);subprocess.run(['/workspace/.floorp-cloud/bin/xdotool','key','ctrl+Prior'],env=env,check=True);snap('keyboard-after')
# Restart while the alternate Workspace owns the hidden state; backend performs normal quit/start.
q('restart-away','workspacesFuncs.changeWorkspace('+json.dumps(ws['B'])+');return qaState();');snap('before-restart')
q('restart','return {pid:Services.appinfo.processID,buildID:Services.appinfo.appBuildID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required")};',restart=True)
q('restart-observe',(r/'observers.js').read_text());snap('after-restart')
q('restart-return','workspacesFuncs.changeWorkspace('+json.dumps(ws['A'])+');return qaState();');snap('restart-returned')
# Choose a healthy fixture context before disable/uninstall can close extension tabs.
q('select-fixture','return qaState();',uri='/B/1')
addon="const done=arguments[arguments.length-1];(async()=>{const {AddonManager}=ChromeUtils.importESModule('resource://gre/modules/AddonManager.sys.mjs');const a=await AddonManager.getAddonByID('simple-tab-groups@drive4ik');OP;done({present:!!(await AddonManager.getAddonByID('simple-tab-groups@drive4ik')),pid:Services.appinfo.processID});})().catch(e=>done({error:String(e)}));"
q('disable',addon.replace('OP','await a.disable()'),**{'async':True});snap('after-disable')
q('enable',addon.replace('OP','await a.enable()'),**{'async':True});time.sleep(3);snap('after-enable')
q('remove',addon.replace('OP','await a.uninstall()'),**{'async':True});snap('after-remove')
for label,target in [('removed-away',ws['B']),('removed-return',ws['A'])]:q(label,'workspacesFuncs.changeWorkspace('+json.dumps(target)+');return qaState();');snap(label+'-state')
q('restart-removed','return {pid:Services.appinfo.processID,sandbox:Services.prefs.getIntPref("security.sandbox.content.level"),signatureRequired:Services.prefs.getBoolPref("xpinstall.signatures.required")};',restart=True)
q('restart-removed-observe',(r/'observers.js').read_text());snap('after-restart-removed');q('restore-final','workspacesFuncs.changeWorkspace('+json.dumps(ws['A'])+');return qaState();');snap('final')
print('LIFECYCLE_DONE',kind,flush=True)
