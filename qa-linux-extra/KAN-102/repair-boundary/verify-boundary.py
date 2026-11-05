#!/usr/bin/env python3
"""Verify observations and rejection criteria; success does NOT qualify a product fix."""
from pathlib import Path
import argparse,json,re,hashlib
p=argparse.ArgumentParser();p.add_argument('root',type=Path);a=p.parse_args();r=a.root
def load(case,name):
 path=r/case/('result-'+name+'.json')
 if path.exists():return json.loads(path.read_text())
 return json.loads((r/case/'observations.json').read_text())['observations'][name]['value']
def names(case):
 path=r/case/'observations.json'
 if path.exists():return list(json.loads(path.read_text())['observations'])
 return [p.name.removeprefix('result-').removesuffix('.json') for p in (r/case).glob('result-*.json')]
fixtures=lambda s:[t for t in s['tabs'] if re.search(r'/[AB]/[123]$',t.get('url',''))]
selected=lambda s:next(t for t in s['tabs'] if t['id']==s['selected'])
shown=lambda t:not t['hidden'] and t['display']!='none' and t['width']>0 and t['height']>0
out={'key':'KAN-102','verificationPassed':True,'safeCandidateEstablished':False,'draftPr':None,'completeProductBuildStarted':False,'originalPersistentTabStripLeakReproduced':False,'variants':{},'lifecycle':{},'crossWorkspaceGroupSwitch':{}}
for case in ['native-owned-v2','css-layer-v2']:
 initial=load(case,'exp-initial');assert len(fixtures(initial['native']))==6
 same=[name for name in names(case) if name.startswith('exp-same-') and isinstance((data:=load(case,name)),dict) and 'native' in data];assert len(same)==3
 for name in same:
  d=load(case,name);s=d['native'];t=selected(s);g='/A/' if '/A/' in t['url'] else '/B/';assert len([x for x in fixtures(s) if shown(x)])==3;assert all(shown(x)==(g in x['url']) for x in fixtures(s))
 ret=load(case,'exp-existing-return');assert '/B/1' in selected(ret['native'])['url'];assert next(w['group'] for w in ret['extension']['windows'] if w['group'])==2
 if case=='native-owned-v2':
  assert ret['manage']['groups'][1]['count'].startswith('0/0');assert len(ret['manage']['fixtureRows'])==3
  out['variants'][case]={'sameWorkspaceUiSwitches':3,'selectedBPreserved':True,'managerRecovery':'FAIL (B0/0; only A fixture rows)','verdict':'REJECT as complete compatibility repair'}
 else:
  away=load(case,'exp-existing-away');foreign=[t for t in fixtures(away['native']) if not t['hidden'] and t['display']=='none'];assert len(foreign)==3 and all(t['id'] in away['native']['nativeVisible'] for t in foreign)
  assert len(ret['manage']['fixtureRows'])==6
  out['variants'][case]={'sameWorkspaceUiSwitches':3,'selectedBPreserved':True,'managerRecovery':'six fixture rows remain','nativeVisibility':'FAIL (three CSS-hidden foreign fixture tabs native-visible/API hidden=false)','verdict':'REJECT as same-contract Workspace repair'}
for case in ['baseline','native-owned-v2','css-layer-v2']:
 d=load(case,'life-keyboard-after');s=d['native'];t=selected(s)
 if case=='css-layer-v2':assert t['workspace']!=s['workspace'] and not t['hidden'] and t['display']=='none' and t['width']==0 and t['id'] in s['nativeVisible']
 else:assert t['pinned'] and shown(t)
 before=load(case,'life-window2-ready');after=load(case,'life-window2-return');assert before['window1']['workspace']==after['window1']['workspace'];preserved=before['window1']['selected']==after['window1']['selected']
 if case!='baseline':assert preserved
 ready=json.loads((r/case/'ready.json').read_text());restart=load(case,'life-restart');removed_restart=load(case,'life-restart-removed');final=load(case,'boundary-final-safety');assert len({ready['pid'],restart['pid'],removed_restart['pid']})==3 and final['pid']==removed_restart['pid']
 for d2 in [ready,restart,removed_restart,final]:assert d2['sandbox']==6 and d2['signatureRequired']
 for phase in ['life-after-disable','life-after-remove','life-removed-return-state','life-after-restart-removed','life-final']:
  data=load(case,phase);fs=fixtures(data['native']);assert len(fs)==6 and all(shown(t) for t in fs), (case,phase)
 assert final['addon'] is None
 assert json.loads((r/case/'shutdown.json').read_text())['exit']==0
 pre=load(case,'life-before-restart');post=load(case,'life-after-restart');assert len(fixtures(post['native']))==6
 for phase in ['before','after']:
  ex=(pre if phase=='before' else post)['extension'];members=[t for t in ex['tabs'] if re.search(r'/[AB]/[123]$',t.get('url',''))]
  assert len(members)==6
 out['lifecycle'][case]={'multipleWindow1SelectionPreserved':preserved,'keyboardSelected':{'id':t['id'],'pinned':t['pinned'],'workspace':t['workspace'],'currentWorkspace':s['workspace'],'display':t['display'],'url':t['url']},'twoRealNormalRestartsPidChain':[ready['pid'],restart['pid'],removed_restart['pid']],'sixFixtureTabRecordsRestored':True,'disableRemoveAndRemovedRestartSixFixturesRecovered':True,'sandbox':6,'signaturesRequired':True,'normalFinalExit':0,'beforeRestartFixtureGroupMappingCount':sum(1 for t in pre['extension']['tabs'] if re.search(r'/[AB]/[123]$',t.get('url','')) and t.get('group')),'afterRestartFixtureGroupMappingCount':sum(1 for t in post['extension']['tabs'] if re.search(r'/[AB]/[123]$',t.get('url','')) and t.get('group'))}
for case in ['native-cross','css-cross']:
 data=load(case,'cross-after-switch');s=data['native'];t=selected(s);assert '/B/1' in t['url'] and t['workspace']!=s['workspace'] and not t['hidden']
 if case=='css-cross':assert t['display']=='none' and t['width']==0 and t['id'] in s['nativeVisible']
 else:assert shown(t)
 final=load(case,'boundary-final-safety');assert final['sandbox']==6 and final['signatureRequired'] and final['addon']['signedState']==2 and not final['addon']['isActive'];assert len(fixtures(final['native']))==6 and all(shown(x) for x in fixtures(final['native']));assert json.loads((r/case/'shutdown.json').read_text())['exit']==0
 out['crossWorkspaceGroupSwitch'][case]={'selectedTab':t,'currentWorkspace':s['workspace'],'fixtureGroupRows':len(data['manage']['fixtureRows']),'afterDisableReturnSixFixturesRecovered':True,'normalFinalExit':0}
case='baseline-cross';blocked=load(case,'cross-blocked-evidence');error=load(case,'cross-group-B-click');assert 'Unable to locate element' in error['error'];assert not blocked['manage']['fixtureRows']
final=load(case,'boundary-final-safety');assert final['sandbox']==6 and final['signatureRequired'] and final['addon']['signedState']==2 and not final['addon']['isActive'];assert len(fixtures(final['native']))==6 and all(shown(x) for x in fixtures(final['native']));assert json.loads((r/case/'shutdown.json').read_text())['exit']==0
out['crossWorkspaceGroupSwitch'][case]={'action':'BLOCKED by actual missing STG B row; no switch executed','fixtureRowsBeforeAttempt':0,'afterDisableReturnSixFixturesRecovered':True,'normalFinalExit':0}
out['requiredDecision']='Define Workspace/STG selection and visibility UX: same native hide slot cannot honor both current contracts without coordinated change; no selective event masking or extension-private storage repair.'
print(json.dumps(out,indent=2))
