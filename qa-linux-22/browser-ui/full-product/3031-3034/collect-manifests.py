from pathlib import Path
import os,re,json,hashlib,tarfile,subprocess
R=Path(__file__).parent;S=Path('/workspace/full-product-3031-3034');N=S/'noraneko';D=S/'obj-artifact-build-output/dist/bin'
def digest(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
 return h.hexdigest()
entries=[];mounts=[]
for mount in sorted((N/'_dist/noraneko').iterdir()):
 if not mount.is_dir():continue
 jar=mount/'jar.mn'
 if not jar.exists():continue
 records=[]
 for line in jar.read_text().splitlines():
  match=re.match(r'^\s+(\S+)\s+\(([^)]+)\)\s*$',line)
  if not match:continue
  target,source=match.groups();src=mount/source;dst=D/'chrome/noraneko'/target
  record={'mount':mount.name,'path':target,'sourceExists':src.is_file(),'distExists':dst.is_file()}
  if src.is_file():record['sourceSha256']=digest(src)
  if dst.is_file():record['distSha256']=digest(dst)
  record['matches']=record.get('sourceSha256')==record.get('distSha256') and src.is_file() and dst.is_file();records.append(record);entries.append(record)
 mounts.append({'mount':mount.name,'manifest':str(jar.relative_to(N)),'entries':len(records),'matchingEntries':sum(x['matches'] for x in records)})
(R/'production-assets.json').write_text(json.dumps({'mounts':mounts,'entries':entries},indent=2))
artifact=R.parent/'shared/floorp-linux-x86_64-moz-artifact.tar.xz';native=[]
with tarfile.open(artifact,'r:xz') as tar:
 for path in ['floorp/floorp','floorp/libxul.so']:
  f=tar.extractfile(path);h=hashlib.sha256()
  for block in iter(lambda:f.read(1024*1024),b''):h.update(block)
  destination=D/Path(path).name;sha=digest(destination);native.append({'path':path,'artifactMemberSha256':h.hexdigest(),'distSha256':sha,'matches':h.hexdigest()==sha})
source={'baseline':'984509d2053be7bae9027133bda7259cd706b208','pr3031':'dbafd082f1f315a49575446e7094177450b1882b','pr3034':'b21a81120d2354d90ea433410173cac208aabbf8','localCombinedHead':subprocess.check_output(['git','-C',str(N),'rev-parse','HEAD'],text=True).strip(),'runtimeHead':subprocess.check_output(['git','-C',str(S/'runtime'),'rev-parse','HEAD'],text=True).strip(),'runtimeInput':json.loads((R.parent/'shared/runtime-input.json').read_text()),'runtimeArtifactSha256':digest(artifact),'nativeMembers':native,'productionAssetMounts':mounts,'productionAssetCount':len(entries),'productionAssetMatches':sum(x['matches'] for x in entries),'buildid2Source':(N/'_dist/buildid2').read_text().strip(),'buildid2Product':(D/'browser/buildid2').read_text().strip(),'productRoot':str(D),'xhtmlSha256':digest(D/'browser/chrome/browser/content/browser/browser.xhtml'),'noranekoManifestSha256':digest(D/'chrome/noraneko.manifest'),'strictPackageStatus':'failed: empty auxiliary XPT archive; strict check retained','regression':json.loads((R/'returning-29721.json').read_text())}
(R/'summary.json').write_text(json.dumps(source,indent=2));print(json.dumps({'combinedHead':source['localCombinedHead'],'mounts':mounts,'assetMatches':source['productionAssetMatches'],'assetCount':len(entries),'native':native,'buildid2':source['buildid2Product']},indent=2))
