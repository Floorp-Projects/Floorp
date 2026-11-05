import sys,hashlib,json,stat,os
from pathlib import Path
def manifest(root):
 result={}
 for p in root.rglob('*'):
  s=p.lstat();d={'mode':stat.S_IMODE(s.st_mode),'type':'link' if p.is_symlink() else 'dir' if p.is_dir() else 'file'}
  if p.is_symlink():d['target']=os.readlink(p)
  elif p.is_file():
   with p.open('rb') as f:d['sha256']=hashlib.file_digest(f,'sha256').hexdigest()
  result[str(p.relative_to(root))]=d
 return result
a,b=map(Path,sys.argv[1:3]);ma,mb=manifest(a),manifest(b);diff=[{'path':p,'a':ma.get(p),'b':mb.get(p)} for p in sorted(set(ma)|set(mb)) if ma.get(p)!=mb.get(p)];out={'a':str(a),'b':str(b),'countA':len(ma),'countB':len(mb),'identical':not diff,'differences':diff,'manifestA':ma,'manifestB':mb};Path(sys.argv[3]).write_text(json.dumps(out,indent=2));print({k:v for k,v in out.items() if not k.startswith('manifest')})
