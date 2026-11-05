#!/usr/bin/env python3
from pathlib import Path
import json
R=Path(__file__).resolve().parent
def read(n):return json.loads((R/n).read_text())
a=read('result-install.json');assert a['signedState']==2 and a['isActive'] and not a['appDisabled']
for f in ['result-normal-result.json','result-panel-result.json']:
 x=read(f);assert x['sponsoredDisplay']=='none' and x['keepDisplay']=='block' and x['imageNaturalWidth']==0
for f in ['result-normal-picker3.json','result-panel-picker-result2.json']:
 x=read(f);assert any(i['width']>500 and i['height']>700 for i in x['iframes'])
p=read('result-final-metadata.json');assert p['protections']['contentSandbox']==6 and p['protections']['signatureRequired'] and p['tabs']['normal']!=p['tabs']['panel']
assert not any(x['path'].startswith('/network-ad') for x in read('requests.json'))
reach=read('http-reachability.json');assert '403' in reach[0]['error'] and reach[1]['status']==200
assert read('cleanup.json')['remainingOwnedProcesses']==[]
x={'recordAuditPassed':True,'signedExtension':a,'normalAndPanelCosmeticNetworkFixturePassed':True,'normalAndPanelPickerLaunchPassed':True,'pickerSelectSaveTested':False,'liveYouTubeValidated':False,'liveYouTubeFailure':'Cloud proxy CONNECT403; browser neterror also recorded','productFixIdentified':False}
(R/'verification.json').write_text(json.dumps(x,indent=2)+'\n');print(json.dumps(x))
