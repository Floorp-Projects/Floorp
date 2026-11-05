"""Compare every archive member's content/type/mode against an existing product."""
import sys,hashlib,json,tarfile,os,stat
from pathlib import Path
archive=Path(sys.argv[1]);tree=Path(sys.argv[2]);out=Path(sys.argv[3]);members={};actual={}
def sha(f):return hashlib.file_digest(f,'sha256').hexdigest()
with tarfile.open(archive,'r:xz') as t:
 for m in t:
  parts=Path(m.name).parts
  if len(parts)<2:continue
  p=str(Path(*parts[1:]));d={'mode':m.mode,'type':'file' if m.isfile() else 'dir' if m.isdir() else 'link' if m.issym() else str(m.type)}
  if m.isfile():d['sha256']=sha(t.extractfile(m))
  if m.issym():d['target']=m.linkname
  members[p]=d
for p in tree.rglob('*'):
 s=p.lstat();d={'mode':stat.S_IMODE(s.st_mode),'type':'link' if p.is_symlink() else 'dir' if p.is_dir() else 'file'}
 if p.is_symlink():d['target']=os.readlink(p)
 elif p.is_file():
  with p.open('rb') as f:d['sha256']=sha(f)
 actual[str(p.relative_to(tree))]=d
keys=set(members)|set(actual);differences=[{'path':p,'archive':members.get(p),'tree':actual.get(p)} for p in sorted(keys) if members.get(p)!=actual.get(p)]
result={'archive':str(archive),'tree':str(tree),'archiveEntryCount':len(members),'treeEntryCount':len(actual),'identical':not differences,'differences':differences,'archiveManifest':members,'treeManifest':actual}
out.write_text(json.dumps(result,indent=2));print(json.dumps({k:v for k,v in result.items() if k not in ['archiveManifest','treeManifest']}))
