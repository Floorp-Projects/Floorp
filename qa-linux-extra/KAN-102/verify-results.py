#!/usr/bin/env python3
import json,hashlib,argparse
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('root',type=Path);a=p.parse_args();root=a.root
load=lambda c,n:json.loads((root/c/('result-'+n+'.json')).read_text())
results=[]
for case,names in [('floorp-on-v2',['B-after','existing2-B-after']),('floorp-off',['B-after','A-after']),('firefox',['B-after','A-after'])]:
 before=json.loads((root/case/'ready.json').read_text());install=load(case,'install');after=load(case,'final-protection');assert install['signedState']==after['addon']['signedState']==2 and install['signatureRequired'] and after['signatureRequired'];assert before['pid']==after['pid'] and before['sandbox']==after['sandbox']==6
 for name in names:
  d=load(case,name);assert 'error' not in d
  target='B' if 'B-' in name else 'A';tabs=[t for t in d['state']['tabs'] if '/A/' in t['url'] or '/B/' in t['url']];assert len(tabs)==6
  active=[t for t in tabs if '/'+target+'/' in t['url']];inactive=[t for t in tabs if t not in active];assert len(active)==len(inactive)==3
  assert all(not t['hidden'] and t['display']!='none' and t['visibility']=='visible' and t['width']>0 and t['height']>0 for t in active)
  assert all(t['hidden'] and t['display']=='none' and t['width']==0 and t['height']==0 and t['hiddenBy']=='simple-tab-groups@drive4ik' for t in inactive)
  assert d['state']['selected'] in [t['id'] for t in active]
  overlaps=[s for s in d['samples'] if any('/A/' in t['url'] and not t['hidden'] for t in s['state']['tabs']) and any('/B/' in t['url'] and not t['hidden'] for t in s['state']['tabs'])]
  results.append({'case':case,'action':name,'settledGroup':target,'activeFixtureVisible':3,'inactiveFixtureVisible':0,'selectedInTarget':True,'transientBothGroupsSampled':bool(overlaps),'rawSha256':hashlib.sha256((root/case/('result-'+name+'.json')).read_bytes()).hexdigest()})
conflicts=[]
for native,ext,ui in [('ws-return-after','ws-return-extension','before-manage-reload'),('existing2-return-after','existing2-extension-final','existing2-manage-ui')]:
 d=load('floorp-on-v2',native);e=load('floorp-on-v2',ext);u=load('floorp-on-v2',ui)
 assert d['state']['selected']==3 and e['windowGroup']==1
 fixtures=[t for t in e['tabs'] if '/A/' in t['url'] or '/B/' in t['url']];assert len(fixtures)==6
 assert all(t['groupId']==(1 if '/A/' in t['url'] else 2) for t in fixtures)
 assert u['tabCount']<6
 conflicts.append({'raw':native,'groupBefore':'B/2','groupAfter':'A/1','selectedBefore':6,'selectedAfter':3,'fixtureTabsStillPresent':6,'nativeSessionMembershipsRemain':True,'manageUi':u})
reload=load('floorp-on-v2','after-manage-reload');assert reload['tabCount']==6
out={'key':'KAN-102','sixUiSwitchObservations':results,'settledTabStripLeakReproduced':False,'workspaceInteractionReproduced':True,'workspacePaths':conflicts,'normalManageReloadRecoveredSixFixtureRows':True,'classification':'bounded Workspace/STG selection and manage-UI desynchronization; original persistent inactive-tab leak not reproduced; two visibility-manager conflict, isolated safe fix unestablished','samePidAndProtectionsAllThreeProfiles':True,'invalidatedTrials':['initial command IPC JSON race','existing-B-click NoSuchElement; no group switch occurred; excluded'],'dataLossEstablished':False,'safeStandaloneProductFixEstablished':False}
print(json.dumps(out,indent=2))
