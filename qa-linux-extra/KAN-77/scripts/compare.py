from pathlib import Path
import json,collections
r=Path('/workspace/qa-linux-22/C-extra/KAN-77')
def load(c,n):return json.loads((r/'trials'/c/n).read_text())
def phases(c):return {x['phase']:x for x in load(c,'measurements.json')}
b=phases('baseline');c=phases('candidate');rows=[]
for name,paths in [('background-B',['/manifest/A.json']),('subframes-four',['/manifest/A.json']),('other-window-C',['/manifest/A.json']),('two-window-background',['/manifest/A.json','/manifest/C.json']),('select-B',['/manifest/B.json']),('reload-B',['/manifest/B.json'])]:
 vals=[sum(p[name]['counts'].get(k,0) for k in paths) for p in [b,c]];rows.append({'phase':name,'countedPaths':paths,'baseline':vals[0],'candidate':vals[1],'candidatePwaUI':c[name]['state']['value']['pwa']})
for name in ['background-B','subframes-four','other-window-C','two-window-background']:assert next(x for x in rows if x['phase']==name)['candidate']==0
for name in ['select-B','reload-B']:assert next(x for x in rows if x['phase']==name)['candidate']>0;assert c[name]['state']['value']['pwa']['actionPresent']
for case in ['baseline','candidate']:
 p=phases(case)
 for name in ['no-manifest','iframe-only','ineligible-about']:assert not p[name]['state']['value']['pwa']['actionPresent']
 assert load(case,'shutdown.json')=={'requestedNormalQuit':True,'exitCode':0}
initial={}
for case in ['baseline','candidate']:
 a=load(case,'http-requests.json');index=phases(case)['initial-settled']['fromIndex'];initial[case]=dict(collections.Counter(x['path'] for x in a[:index]));assert initial[case]['/manifest/A.json']>0
panel=c['panel-metadata']['actionResult']['value'];assert panel['panelState']=='open' and panel['title']=='KAN77 A'
report={'sourceHead':'f04ff611c3634515064776e2d58e76648f87269b','initialCounts':initial,'rows':rows,'candidatePanel':panel,'noInjectedObserverNotifications':True,'negativeUIAbsentBoth':['no-manifest','iframe-only','ineligible-about']};(r/'comparison.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps(report,indent=2))
