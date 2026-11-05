"""Verify native-vs-Flatpak observations; no browser or network execution."""
import json,re
from pathlib import Path
O=Path(__file__).parent
RAW=O.parents[1]/'raw/KAN-90/thunderbird-origin'
if not RAW.exists():RAW=O
def load(case,name):return json.loads((RAW/case/name).read_text())
def forks(d,key):return {p['pid'] for p in d[key]['children'] if p['type']=='forkServer'}
native=load('mime-handler-1','observations.json')['observations']
nt=load('mime-handler-1','dbus-events.json')['trace']
assert native['thunderbirdRuntime']['mimeHandler']==1
assert native['thunderbirdRuntime']['openUri']==2
assert native['handlerInfo']['preferredAction']==native['handlerInfo']['useSystemDefault']==4
assert 'member=OpenURI\n' not in nt
assert any(x['path']=='/from-thunderbird-origin' for x in native['hits'])
native_pid=native['thunderbirdRuntime']['pid']
native_senders=[s for s,v in native['dbusSenders'].items() if re.search(r'uint32 '+str(native_pid)+r'\b',v['stdout'])]
assert native_senders and any('sender='+s in nt for s in native_senders)
data=load('flatpak-159-default','observations.json')['observations'];trace=load('flatpak-159-default','dbus-events.json')['trace']
tb=data['thunderbirdRuntime'];assert tb['version']=='159.0'
assert tb['mimeHandler']==tb['openUri']==2 and tb['gtkUsePortal']==tb['remotingOverride']==''
assert tb['flatpakId']=='org.mozilla.thunderbird'
call=re.search(r'method call[^\n]*sender=(:[\d.]+)[^\n]*member=OpenURI\n\s+string ""\n\s+string "([^"]+)"',trace)
assert call and call.group(2)=='http://127.0.0.1:19094/from-thunderbird-origin'
sender=call.group(1);identity=data['dbusSenders'][sender]
assert identity['flatpakAppId']=='org.mozilla.thunderbird' and identity['processName']=='xdg-dbus-proxy'
assert re.search(r'signal[^\n]*destination='+re.escape(sender)+r'[^\n]*interface=org.freedesktop.portal.Request; member=Response\n\s+uint32 0',trace)
assert any(x['path']=='/from-thunderbird-origin' for x in data['hits'])
assert data['floorpBefore']['pid']==data['floorpEnd']['pid']==2
assert data['floorpBefore']['sandbox']==6 and tb['sandbox']==5
assert data['flatpakInstancesBefore']==data['flatpakInstancesAfter']
assert forks(data,'floorpProcessesBefore')==forks(data,'floorpProcessesAfter') and forks(data,'floorpProcessesBefore')
assert any(x=='http://127.0.0.1:19094/from-thunderbird-origin' for x in data['floorpEnd']['tabs'])
print(json.dumps({'passed':True,'native140':{'mimeHandler':1,'systemDefaultAction':4,'openUriObserved':False,'schemeSupportSenderPid':native_pid},'flatpak159':{'openUriSender':sender,'senderProxy':identity,'portalResponse':0,'httpGet':True,'parentPidBefore':data['floorpBefore']['pid'],'parentPidAfter':data['floorpEnd']['pid'],'forkserverPids':sorted(forks(data,'floorpProcessesBefore')),'hostInstancesUnchanged':True,'floorpSandboxBefore':6,'thunderbirdSandbox':5,'shippedPortalPrefs':[2,2]}},indent=2))
