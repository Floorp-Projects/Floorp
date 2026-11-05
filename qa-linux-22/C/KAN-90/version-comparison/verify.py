#!/usr/bin/env python3
from pathlib import Path
import json,re,hashlib,tarfile
R=Path(__file__).resolve().parent
out=[]
for v in ['12.17.2','12.21.1']:
 p=R/v/'result.json';r=json.loads(p.read_text());trace=(R/v/'dbus-monitor.log').read_text()
 assert not r.get('error'),r.get('error')
 sender=re.search(r'sender=(:[\d.]+).*member=OpenURI',trace).group(1)
 assert r['dbusSenders'][sender]['flatpakAppId']=='org.mozilla.thunderbird'
 assert re.search(r'destination='+re.escape(sender)+r'.*member=Response\n   uint32 0',trace)
 assert any(h['path']=='/from-thunderbird-origin' for h in r['hits'])
 assert r['floorpBefore']['pid']==r['floorpEnd']['pid']
 def fork(k):return [c['pid'] for c in r[k]['children'] if c['type']=='forkServer']
 assert len(fork('floorpProcessesBefore'))==1
 assert fork('floorpProcessesBefore')==fork('floorpProcessesAfter')
 assert r['floorpBefore']['sandbox']==6
 assert r['thunderbirdRuntime']['sandbox']==5
 assert r['thunderbirdRuntime']['mimeHandler']==r['thunderbirdRuntime']['openUri']==2
 assert r['thunderbirdRuntime']['flatpakId']=='org.mozilla.thunderbird'
 assert not r['thunderbirdRuntime']['gtkUsePortal'] and not r['thunderbirdRuntime']['remotingOverride']
 log=(R/v/'floorp.log').read_text();assert not re.search(r'SIGSEGV|Segmentation fault',log,re.I)
 raw={k:r[k] for k in ['sourceSha','variant','browserDistribution','environment','syntheticInput','floorpStart','floorpBefore','floorpEnd','floorpProcessesBefore','floorpProcessesAfter','thunderbirdRuntime','handlerInfo','hits','dbusSenders','flatpakInstancesBefore','flatpakInstancesAfter']}
 raw['sourceFileSha256']=hashlib.sha256(p.read_bytes()).hexdigest()
 d=R/'raw'/v;d.mkdir(parents=True,exist_ok=True);(d/'observations.json').write_text(json.dumps(raw,indent=2)+'\n');(d/'dbus-monitor.log').write_text(trace)
 out.append({'version':v,'passed':True,'sender':sender,'senderProxyPid':r['dbusSenders'][sender]['stdout'].strip(),'parentBefore':r['floorpBefore']['pid'],'parentAfter':r['floorpEnd']['pid'],'forkServerBefore':fork('floorpProcessesBefore')[0],'forkServerAfter':fork('floorpProcessesAfter')[0],'portalResponse':0,'httpGet':True,'floorpSandboxBefore':6,'thunderbirdSandbox':5,'runtimeVersion':r['floorpStart']['version'],'runtimeBuildID':r['floorpStart']['buildID'],'noObservedSegv':True})
# Verify tested current executable/libxul against the retained, hash-verified release archive.
a=Path('/workspace/qa-linux-22/C/KAN-64/floorp.tar.xz');digest=hashlib.sha256(a.read_bytes()).hexdigest();assert digest=='7dcedcbfef747819153b0b9c541a9c1f4f4965b0720f16d5d237d39b0c30d613'
checks={}
with tarfile.open(a) as t:
 for name in ['floorp','libxul.so']:
  member=next(m for m in t.getmembers() if m.name.endswith('/'+name) and m.isfile())
  archiveHash=hashlib.sha256(t.extractfile(member).read()).hexdigest();localHash=hashlib.sha256((Path('/workspace/qa-linux-22/C/products/floorp')/name).read_bytes()).hexdigest();assert archiveHash==localHash
  checks[name]=localHash
summary={'trials':out,'currentArchiveSha256':digest,'currentTestedFilesMatchArchive':checks,'limitations':['Both Floorp versions are official GitHub Linux archives, not historical Floorp Flatpak comparison','Debian13.6 Cloud kernel/Xvfb/Openbox/private DBus+fixture system bus; not native Fedora44 desktop','One mail-link trial per product version; no universal crash-freedom claim','Floorp sandbox6 measured before each link; no sandbox preference written; Thunderbird default5 unchanged'],'published':False,'jiraCommentPosted':False}
(R/'verification.json').write_text(json.dumps(summary,indent=2)+'\n');print(json.dumps(summary,indent=2))
