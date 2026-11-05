from control import send
import json
case='floorp-on-v2';q=lambda id,s='return true;',**k:send(case,id,s,**k)
collect="const done=arguments[arguments.length-1];setTimeout(()=>done(qaCollect()),1800);"
q('B-start','qaStart();return qaSnap();');q('B-click',context='content',uri='manage/manage.html',click='.tab[title*="/B/1"]');d=q('B-after',collect,**{'async':True});print('B',[(t['id'],t['hidden']) for t in d['state']['tabs']],flush=True)
ws=q('ws-create-B','window.qaWorkspaceA=workspacesFuncs.getSelectedWorkspaceID();qaStart();window.qaWorkspaceB=workspacesFuncs.createNoNameWorkspace();return {A:qaWorkspaceA,B:qaWorkspaceB};');q('ws-away-after',collect,**{'async':True});print('AWAY',ws,flush=True)
q('ws-popup-open',click='#workspaces-toolbar-button');q('ws-return-start','qaStart();return qaSnap();');q('ws-return-click','return {workspace:workspacesFuncs.getSelectedWorkspaceID()};',click='#workspace-'+ws['A']);d=q('ws-return-after',collect,**{'async':True});print('RETURN',json.dumps({'workspace':d['state']['workspace'],'selected':d['state']['selected'],'tabs':[(t['id'],t['hidden'],t['display'],t['hiddenBy']) for t in d['state']['tabs']],'events':[(e['event'],e['target']) for e in d['events']],'samples':len(d['samples'])}),flush=True)
s="""const done=arguments[arguments.length-1];(async()=>{const b=window.wrappedJSObject.browser;const win=await b.windows.getCurrent();const tabs=await b.tabs.query({windowId:win.id});done({windowGroup:await b.sessions.getWindowValue(win.id,'groupId'),tabs:await Promise.all(tabs.map(async t=>({id:t.id,url:t.url,hidden:t.hidden,active:t.active,groupId:await b.sessions.getTabValue(t.id,'groupId')}))),groups:await b.runtime.sendMessage({action:'get-groups-list'})});})().catch(e=>done({error:String(e)}));"""
d=q('ws-return-extension',s,context='content',uri='manage/manage.html',**{'async':True});print('GROUPS',json.dumps(d)[:1800],flush=True)
q('ws-return-screen',shot='workspace-return.png')
