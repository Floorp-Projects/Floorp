import json,sys
from control import send
case=sys.argv[1];q=lambda id,s='return true;',**k:send(case,id,s,**k)
collect="const done=arguments[arguments.length-1];setTimeout(()=>done(qaCollect()),1500);"
for i,g in enumerate(['B','A']):
 q(g+'-start','qaStart();return qaSnap();');q(g+'-click',context='content',uri='manage/manage.html',click='.tab[title*="/'+g+'/1"]');d=q(g+'-after',collect,**{'async':True});fixture=[t for t in d['state']['tabs'] if '/A/' in t['url'] or '/B/' in t['url']];wrong=[t['id'] for t in fixture if ('/'+g+'/') not in t['url'] and (not t['hidden'] or t['display']!='none' or t['width']!=0)];missing=[t['id'] for t in fixture if ('/'+g+'/') in t['url'] and (t['hidden'] or t['display']=='none' or not t['width'])];assert not wrong and not missing,(wrong,missing);print(case,g,'PASS',len(d['events']),'events',len(d['samples']),'semantic states',flush=True)
s="""const done=arguments[arguments.length-1];(async()=>{const b=window.wrappedJSObject.browser,win=await b.windows.getCurrent(),tabs=await b.tabs.query({windowId:win.id});done({windowGroup:await b.sessions.getWindowValue(win.id,'groupId'),tabs:await Promise.all(tabs.map(async t=>({id:t.id,url:t.url,hidden:t.hidden,active:t.active,groupId:await b.sessions.getTabValue(t.id,'groupId')})))});})().catch(e=>done({error:String(e)}));"""
q('extension-final',s,context='content',uri='manage/manage.html',**{'async':True});q('final-screen',shot='final.png');print('DONE',case,flush=True)
