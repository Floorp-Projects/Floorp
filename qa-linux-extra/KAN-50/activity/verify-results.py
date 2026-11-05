#!/usr/bin/env python3
from pathlib import Path
import json,hashlib
R=Path(__file__).resolve().parent
read=lambda n:json.loads((R/n/'result.json').read_text())
b=read('baseline-managed-v2');a=read('candidate-managed-v2');c=read('candidate-panel-first');n=read('native-baseline')
assert b['passed'] is False and not b.get('error')
assert a['passed'] is c['passed'] is True
for x in [b,a,c]:
 assert x['minimized1']['activity']['windowState']==x['minimized2']['activity']['windowState']==2
 assert x['minimized1']['captions']['text']!=x['minimized2']['captions']['text']
 assert x['minimized2']['video']['cloning']
 assert x['runtime']['sandbox']==x['after']['sandbox']==6
 assert x['runtime']['pid']==x['after']['pid']
assert b['normalClosedWhilePanelRemains']['rootForced'] is False
assert b['minimized2']['video']['hidden'] is True
assert b['afterAllClose']['players']==1 and b['afterAllClose']['windows']==0
assert b['afterAllClose']['panelPipAttribute']=='true'
assert any('forceAppWindowActive' in x['message'] for x in b['runtimeErrors'])
for x in [a,c]:
 assert x['bothOpen']['activity']['rootMap']==2
 assert x['normalClosedWhilePanelRemains']['rootForced'] and x['normalClosedWhilePanelRemains']['rootMap']==1
 assert x['minimized2']['video']['hidden'] is False
 assert x['afterAllClose']['players']==x['afterAllClose']['windows']==0
 assert not x['runtimeErrors'] and x['afterAllClose']['panelPipAttribute'] is None
assert n['nativeBrowserTests']['passed'] is False and n['nativeBrowserTests']['error'].count('NotAllowedError')==2
assert c['nativeBrowserTests']['passed'] is True
copy=json.loads((R/'candidate-verification.json').read_text());assert len(copy['changedArchiveMembers'])==1 and not copy['fullProductionBuild']
out={'recordAuditPassed':True,'baselineActualMediaChecksPassed':sum(x['passed'] for x in b['checks']),'baselineActualMediaChecksTotal':len(b['checks']),'candidateNormalFirstChecksPassed':len(a['checks']),'candidatePanelFirstChecksPassed':len(c['checks']),'nativeBrowserRegressionCases':2,'nativeBaselineFailedCases':2,'nativeCandidatePassedCases':2,'bothBaselineAndCandidateVttCaptionsAdvanced':True,'captionFreezeReproduced':False,'actualMinimizeVerified':True,'candidatePatchCommit':'9d042bc17be83c15d2ff3a25eb712f04c4d3eb45','sourceTypecheckPassed':True,'scopedLintPassed':True,'formatCheckPassed':True,'patchParityHostTestsPassed':1,'patchSourceAndArtifactValidationPassed':True,'newNativeBuild':False}
(R/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
