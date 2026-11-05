import json,sys
from control import send
prefix=sys.argv[1] if len(sys.argv)>1 else 'existing'
q=lambda id,s='return true;',**k:send('floorp-on-v2',prefix+'-'+id,s,**k)
collect="const done=arguments[arguments.length-1];setTimeout(()=>done(qaCollect()),1800);"
q('B-start','qaStart();return qaSnap();');q('B-click',context='content',uri='manage/manage.html',click='.tab[title*="/B/1"]');d=q('B-after',collect,**{'async':True});assert all(t['hidden'] for t in d['state']['tabs'] if '/A/' in t['url']);print('B hiddenBy',[t['hiddenBy'] for t in d['state']['tabs'] if '/A/' in t['url']],flush=True)
ws=q('ids','return {A:qaWorkspaceA,B:qaWorkspaceB};')
for label,target in [('away',ws['B']),('return',ws['A'])]:
 q(label+'-popup',click='#workspaces-toolbar-button');q(label+'-start','qaStart();return qaSnap();');q(label+'-click',click='#workspace-'+target);d=q(label+'-after',collect,**{'async':True});print(label,json.dumps({'selected':d['state']['selected'],'A':[(t['id'],t['hidden'],t['hiddenBy']) for t in d['state']['tabs'] if '/A/' in t['url']],'B':[(t['id'],t['hidden'],t['hiddenBy']) for t in d['state']['tabs'] if '/B/' in t['url']],'events':[(e['event'],e['target']) for e in d['events']]}),flush=True)
s="""const done=arguments[arguments.length-1];(async()=>{const b=window.wrappedJSObject.browser,win=await b.windows.getCurrent(),tabs=await b.tabs.query({windowId:win.id});done({windowGroup:await b.sessions.getWindowValue(win.id,'groupId'),tabs:await Promise.all(tabs.map(async t=>({id:t.id,url:t.url,hidden:t.hidden,active:t.active,groupId:await b.sessions.getTabValue(t.id,'groupId')})))});})().catch(e=>done({error:String(e)}));"""
print('EXT',json.dumps(q('extension-final',s,context='content',uri='manage/manage.html',**{'async':True})),flush=True)
