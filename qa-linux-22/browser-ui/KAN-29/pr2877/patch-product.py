from pathlib import Path
import json,hashlib,zipfile,shutil,sys
r=Path(__file__).parent;b=r.parents[1];src=b/'products/floorp';dest=b/'products/floorp-pr2877';assert not dest.exists(),dest
shutil.copytree(src,dest,symlinks=True)
p=dest/'omni.ja';tmp=p.with_suffix('.tmp');changed=[];target='chrome/noraneko/nora-content/assets/js/index10.js'
with zipfile.ZipFile(p) as z,zipfile.ZipFile(tmp,'w') as out:
 for info in z.infolist():
  data=z.read(info.filename)
  if info.filename==target:
   old=data.decode();a=old.index('  function updateHoverOffset()');end=old.index('  function observePanelWidths()',a);section=old[a:end]
   edits=[('const atEnd = untrack(orders).floorpSidebar > 0;','const atEnd = untrack(orders).floorpSidebar > 0;\n    const onRight = atEnd !== (getComputedStyle(browser)?.direction === "rtl");'),('let occupiedEdge = atEnd ?','let occupiedEdge = onRight ?'),('const rect = element.getBoundingClientRect();','const rect = element.getBoundingClientRect();\n      if (rect.width <= 0 || rect.height <= 0) continue;'),('occupiedEdge = atEnd ?','occupiedEdge = onRight ?'),('Math.max(0, atEnd ? browserRect.right','Math.max(0, onRight ? browserRect.right')]
   for before,after in edits:
    assert section.count(before)==1,(before,section.count(before));section=section.replace(before,after)
   data=(old[:a]+section+old[end:]).encode();changed.append({'path':info.filename,'beforeSha256':hashlib.sha256(old.encode()).hexdigest(),'afterSha256':hashlib.sha256(data).hexdigest()});(r/'candidate-flex-module.js').write_bytes(data)
  out.writestr(info,data)
tmp.replace(p);(dest/'floorp-pr2877').symlink_to('floorp')
with zipfile.ZipFile(src/'omni.ja') as z,zipfile.ZipFile(p) as n:
 differences=[name for name in z.namelist() if z.read(name)!=n.read(name)];assert differences==[target],differences
result={'sourceSha':'984509d2053be7bae9027133bda7259cd706b208','prHead':'f43b22d34c0728e0ed800b8016d81b3212190954','product':str(dest),'changes':changed,'originalArchiveSha256':hashlib.sha256((src/'omni.ja').read_bytes()).hexdigest(),'candidateArchiveSha256':hashlib.sha256(p.read_bytes()).hexdigest(),'binarySha256':hashlib.sha256((dest/'floorp').read_bytes()).hexdigest(),'onlyChangedEntry':differences};(r/'product-patch.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
