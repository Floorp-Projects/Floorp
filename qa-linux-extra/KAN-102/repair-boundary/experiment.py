import json,sys,time
from pathlib import Path
sys.path.insert(0,'/workspace/qa-linux-22/C-extra/KAN-102');from control import send
root=Path('/workspace/qa-linux-22/C-extra/KAN-102/repair-boundary');kind=sys.argv[1];case='repair-boundary/'+kind;q=lambda id,s='return true;',**k:send(case,'exp-'+id,s,**k)
q('observe',(root/'observers.js').read_text())
cap="const done=arguments[arguments.length-1];setTimeout(async()=>done({native:qaState(),extension:await qaExt(),manage:await qaManage(),events:qaEvents}),1300);"
def snap(id):
 d=q(id,cap,**{'async':True});print(kind,id,json.dumps({'ws':d['native']['workspace'],'selected':d['native']['selected'],'shown':[t['url'].split('/')[-2:] for t in d['native']['tabs'] if t['url'].startswith('http://127.0.0.1') and t['display']!='none'],'groups':d['manage'].get('groups'),'nativeVisible':d['native']['nativeVisible']}),flush=True);return d
snap('initial')
for group in ['B','A','B']:
 q('same-'+group+'-'+str(time.time_ns()),context='content',uri='manage/manage.html',click='.tab[title*="/'+group+'/1"]')
 snap('same-'+group+'-'+str(time.time_ns()))
ws=q('create','window.qaA=workspacesFuncs.getSelectedWorkspaceID();window.qaB=workspacesFuncs.createNoNameWorkspace();return {A:qaA,B:qaB};');snap('new-away')
q('popup-return',click='#workspaces-toolbar-button');q('return',click='#workspace-'+ws['A']);snap('new-return')
# Passive refresh: create another Workspace in current window exercises visibility refresh.
# Existing popup roundtrip is independent from this create setup.
for label,target in [('existing-away',ws['B']),('existing-return',ws['A'])]:
 q(label+'-popup',click='#workspaces-toolbar-button');q(label+'-click',click='#workspace-'+target);snap(label)
(root/(kind+'-workspaces.json')).write_text(json.dumps(ws,indent=2)+'\n')
print('PHASE1_DONE',kind,flush=True)
