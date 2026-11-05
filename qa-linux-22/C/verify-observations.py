"""Recompute QA claims from the exported small observation records (no browser run)."""
import json,re
from pathlib import Path
B=Path(__file__).parent;R=B/'raw'
def read(path):return json.loads((R/path).read_text())['observations']
def active(record):
 m=re.search(r'0x[0-9a-fA-F]+',record['stdout']);return int(m.group(),16) if m else None
checks=[]
baseline=read('KAN-71/baseline-observations.json')
for case in baseline:
 for trial in case['tests']:
  ids=[int(w['id']) for w in trial['browserWindows']]
  assert (active(trial['after']) in ids)==trial['focusedBrowser']
  assert trial['requestReceived'] and trial['originalProcessAlive']
checks.append({'name':'KAN71 baseline focus recomputed','trials':sum(len(c['tests']) for c in baseline)})
gui=[]
for p in sorted((R/'KAN-71/activation-followup').glob('*/observations.json')):
 case=json.loads(p.read_text())['observations'];assert case['version']['parentPid']==case['end']['parentPid'];assert case['version']['sandbox']==6
 for trial in case['tests']:
  assert trial['event']['eventTimestamp']>0 and trial['launcher']['startupIdPresent']
  stamp=trial['launcher']['startupTimestamp'];assert stamp>0
  assert any(x['kind']=='new' and x['startupTimestamp']==stamp for x in trial['startupMessages'])
  assert trial['requestReceived'] and trial['originalParentAlive'] and trial['windowManagerAlive']
  assert (active(trial['after']) in [int(i) for i in trial['browserWindowIds']])==trial['focusedBrowser']
 if 'hits' in case:assert any(x['path'].startswith('/cinnamon-') for x in case['hits'])
 gui.extend(case['tests'])
checks.append({'name':'KAN71 real GUI timestamps/X11/focus recomputed','trials':len(gui),'raises':sum(x['focusedBrowser'] for x in gui)})
group=read('KAN-64/workspace-observations.json');assert all(not t['hidden'] for t in group['before']['tabs']);assert all(t['hidden'] for t in group['after']['tabs']);assert group['after']['display']=='none';assert group['signature']['signedState']==2
control=read('KAN-64/firefox-observations.json');assert all(t['hidden'] for t in control['hide']['tabs'])
checks.append({'name':'KAN64 native hidden member state and Firefox control verified'})
portal=read('KAN-90/portal-followup/result.json');assert portal['before']['pid']==portal['after']['pid']==2;assert portal['before']['sandboxLevel']==portal['after']['sandboxLevel']==6
assert portal['portalCall']['rc']==0 and any(x['path']=='/from-real-portal' for x in portal['hits'])
assert portal['flatpakInstancesBefore']['stdout']==portal['flatpakInstancesAfter']['stdout']
def forks(p):return {x['pid'] for x in p['children'] if x['type']=='forkServer'}
assert forks(portal['processesBefore'])==forks(portal['processesAfter']) and forks(portal['processesBefore'])
trace=json.loads((R/'KAN-90/portal-followup/dbus-events.json').read_text())['trace'];assert 'member=OpenURI\n' in trace and 'member=Response\n   uint32 0' in trace
checks.append({'name':'KAN90 actual portal response/GET/parent/forkserver verified','forkServerPids':sorted(forks(portal['processesBefore']))})
web=[]
for key in ['KAN-20','KAN-21']:
 for p in (R/key).glob('*/observations.json'):
  case=json.loads(p.read_text())['observations'];responses=[x['status'] for x in case['navigation'] if x['status'] is not None];web.append({'case':p.parent.name,'httpStatuses':responses})
checks.append({'name':'Existing web records only; no network retest','cases':len(web),'root503Cases':sum(503 in c['httpStatuses'] for c in web)})
print(json.dumps({'passed':True,'checks':checks},indent=2))
