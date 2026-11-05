from pathlib import Path
import os,re,json,hashlib,tarfile,subprocess,zipfile,stat
R=Path(__file__).parent;S=Path('/workspace/kan30-full');N=S/'noraneko';dist=S/'obj-artifact-build-output/dist';D=dist/'bin';P=dist/'floorp'
def digest(p):
 h=hashlib.sha256()
 with open(p,'rb') as f:
  for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
 return h.hexdigest()
assets=[];mounts=[]
for mount in sorted((N/'_dist/noraneko').iterdir()):
 jar=mount/'jar.mn'
 if not jar.is_file():continue
 records=[]
 for line in jar.read_text().splitlines():
  m=re.match(r'^\s+(\S+)\s+\(([^)]+)\)\s*$',line)
  if not m:continue
  target,source=m.groups();src=mount/source;dst=D/'chrome/noraneko'/target
  row={'mount':mount.name,'path':target,'sourceSha256':digest(src),'assembledSha256':digest(dst)};row['matches']=row['sourceSha256']==row['assembledSha256'];assert row['matches'],row;assets.append(row);records.append(row)
 mounts.append({'name':mount.name,'assetCount':len(records)})
(R/'production-assets.json').write_text(json.dumps({'mounts':mounts,'entries':assets},indent=2))
archive=dist/'floorp-12.21.1@159.0.1.en-US.linux-x86_64.tar.xz';archive_sha=digest(archive);members=[]
with tarfile.open(archive,'r:xz') as tar:
 for member in tar:
  rel=Path(member.name);assert rel.parts[0]=='floorp' and '..' not in rel.parts
  dst=dist/rel
  if member.isfile():
   f=tar.extractfile(member);h=hashlib.sha256()
   for b in iter(lambda:f.read(1024*1024),b''):h.update(b)
   row={'path':member.name,'type':'file','archiveSha256':h.hexdigest(),'stagedSha256':digest(dst),'modeArchive':oct(member.mode),'modeStaged':oct(dst.stat().st_mode&0o777)};row['matches']=row['archiveSha256']==row['stagedSha256'];assert row['matches'],row
  elif member.isdir():assert dst.is_dir();row={'path':member.name,'type':'directory','matches':True}
  elif member.issym():assert dst.is_symlink() and os.readlink(dst)==member.linkname;row={'path':member.name,'type':'symlink','matches':True}
  else:raise AssertionError(member.name)
  members.append(row)
(R/'archive-stage-comparison.json').write_text(json.dumps({'archive':str(archive),'archiveSha256':archive_sha,'scope':'normal standard-packager staged payload, not independently tar-extracted','members':members,'allMatching':all(x['matches'] for x in members)},indent=2))
xpt=dist/'floorp-12.21.1@159.0.1.en-US.linux-x86_64.xpt_artifacts.zip';gen=Path('/workspace/qa-linux-22/browser-ui/full-product/xpt-generation/generated/xpt')
with zipfile.ZipFile(xpt) as z:
 xs=[{'name':n,'matches':hashlib.sha256(z.read(n)).hexdigest()==digest(gen/Path(n).name)} for n in z.namelist() if n.endswith('.xpt')]
assert len(xs)==155 and all(x['matches'] for x in xs)
summary={'sourceBaseline':'29a3eb23145da90706d8a94441373278521cc737','existingPr':2934,'existingPrHead':'b7866aaa4905be4d86fb3dff1237d6a70c483f10','candidateSourceSha':subprocess.check_output(['git','-C',str(N),'rev-parse','HEAD'],text=True).strip(),'candidateKind':'current main plus exact existing PR one-line change; not old PR full-source build','runtimeSha':'8965ffc346d426ccdf919bd18585e4d4a0c21654','runtimeView':'private namespace read-only bind of existing matching patched source; own config/object and frontend','productionAssetCount':len(assets),'productionAssetMatches':sum(x['matches'] for x in assets),'mounts':mounts,'buildid2':(D/'browser/buildid2').read_text().strip(),'artifactBuildNativeCompilation':False,'strictChecks':'canonical packager.mk byte compare before mach package; error-if-empty preserved','strictPackageExit':0,'package':str(archive),'packageSha256':archive_sha,'archiveMembersCompared':len(members),'archiveStageAllMatching':True,'runnable':str(P/'floorp'),'runnableKind':'standard-packager staged payload, verified against every archive member; not independent extraction','expectedGeckoVersion':'159.0.1','nativeFloorpSha256':digest(P/'floorp'),'nativeLibxulSha256':digest(P/'libxul.so'),'xptZipSha256':digest(xpt),'xptModules':155,'xptAllMatchingCanonicalGenerated':True,'securityPreferencesOverrideApplied':False,'sourceCppRustChanges':False,'remainingLimits':['No native recompilation','Original Runtime CI XPT companion bytes unavailable; source-generated matching IDLs used','No signing/release/publication','No UI regression executed by this child; parent runs final limited real-UI tests']}
(R/'summary.json').write_text(json.dumps(summary,indent=2));print(json.dumps(summary,indent=2))
