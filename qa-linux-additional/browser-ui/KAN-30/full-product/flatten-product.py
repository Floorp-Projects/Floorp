import os,shutil,json
from pathlib import Path
root=Path('/workspace/kan30-full/obj-artifact-build-output/dist/bin');converted=[]
# Same dereference result as workflow rsync -aL, but at most one link is copied at a time.
for directory,dirs,files in os.walk(root,followlinks=False):
 for name in dirs+files:
  p=Path(directory)/name
  if not p.is_symlink():continue
  source=p.resolve(strict=True);tmp=p.with_name(p.name+'.qa-flatten-tmp')
  if source.is_dir():shutil.copytree(source,tmp,symlinks=False);p.unlink();tmp.rename(p)
  else:shutil.copy2(p,tmp,follow_symlinks=True);os.replace(tmp,p)
  converted.append(str(p.relative_to(root)))
left=[str(Path(d)/n) for d,ds,fs in os.walk(root) for n in ds+fs if (Path(d)/n).is_symlink()];assert not left,left
Path(__file__).with_name('flatten-result.json').write_text(json.dumps({'method':'one-link-at-a-time dereference; equivalent payload to workflow rsync -aL','converted':converted,'remainingSymlinks':left},indent=2))
