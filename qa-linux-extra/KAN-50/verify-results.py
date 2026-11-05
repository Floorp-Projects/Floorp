#!/usr/bin/env python3
from pathlib import Path
import json,hashlib,ast
R=Path(__file__).resolve().parent
cur=json.loads((R/'regression-current-v2/result.json').read_text());old=json.loads((R/'regression-old-v2/result.json').read_text())
assert cur['passed'] is True and old['passed'] is False
assert cur['fixtureSha256']==old['fixtureSha256']
for x in [cur,old]:
 assert x['cases']['normal']['afterOpen']['cloning'] is True
 assert x['cases']['normal']['afterClose']==[]
 assert x['runtime']['sandbox']==x['after']['sandbox']==6
 assert x['runtime']['pid']==x['after']['pid']
 assert x['cases']['panel']['playing']['frames']>0 and x['cases']['panel']['playing']['ready']==4
assert cur['cases']['panel']['afterOpen']['cloning'] is True
assert cur['cases']['panel']['afterClose']==[]
assert old['cases']['panel']['afterOpen']['cloning'] is False
assert len(old['cases']['panel']['afterClose'])==1
assert old['panelStructure']['gBrowserType']=='undefined'
assert cur['panelStructure']['mappedTab'] is True
assert any('parentWin.gBrowser is undefined' in e['message'] for e in old['runtimeErrors'])
manual=json.loads((R/'historical/trial/result.json').read_text());assert len(manual['panelRepeatPip'])==2 and len(manual['panelCloseFirst'])==len(manual['panelCloseSecond'])==1
ast.parse((R/'pip-regression.py').read_text())
out={'recordAuditPassed':True,'currentExitCode':0,'historicalExitCode':1,'historicalFailureExpected':'panel video was not visually cloned into PiP','sameVideoBytesSha256':cur['fixtureSha256'],'normalControlsPassed':2,'panelCurrentPassed':True,'panelHistoricalReproduced':True,'oldRepeatedBlankPlayers':2,'currentResidualRuntimeErrorRecorded':bool(cur['runtimeErrors']),'scriptSha256':hashlib.sha256((R/'pip-regression.py').read_bytes()).hexdigest(),'newProductCodeChanges':False,'commands':['dbus-run-session -- <marionette-python> pip-regression.py --binary <official12.21.1> --out <fresh-current> --display 209 --port 28752 --http-port 19052 --source-sha 984509d2053be7bae9027133bda7259cd706b208 --video-file <same-generated60sVP8>','dbus-run-session -- <marionette-python> pip-regression.py --binary <official12.16.4> --out <fresh-old> --display 210 --port 28753 --http-port 19053 --source-sha 811a5b821e1d9d47b40f22aee6df5db2254a54b1 --video-file <same-generated60sVP8>']}
(R/'verification.json').write_text(json.dumps(out,indent=2)+'\n');print(json.dumps(out,indent=2))
