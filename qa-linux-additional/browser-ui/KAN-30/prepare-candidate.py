from pathlib import Path
import shutil,zipfile,hashlib,json,os
R=Path(__file__).parent;src=Path(os.environ.get('QA_OFFICIAL_PRODUCT_DIR','/workspace/qa-linux-22/browser-ui/products/floorp'));dest=R/'candidate-pr2934'
assert not dest.exists();dest.mkdir()
for p in src.iterdir():
 if p.name in ['omni.ja','floorp','floorp-bin']:shutil.copy2(p,dest/p.name)
 else:(dest/p.name).symlink_to(p,target_is_directory=p.is_dir())
p=dest/'omni.ja';tmp=dest/'omni.tmp';target='chrome/noraneko/nora-content/assets/js/index9.js';before='this.listen(window, "blur", ()=>{';after='this.listen(window, "deactivate", ()=>{'
with zipfile.ZipFile(p)as z,zipfile.ZipFile(tmp,'w')as out:
 for info in z.infolist():
  data=z.read(info.filename)
  if info.filename==target:
   b=data.decode();assert b.count(before)==1;data=b.replace(before,after).encode()
  out.writestr(info,data)
tmp.replace(p)
with zipfile.ZipFile(src/'omni.ja')as a,zipfile.ZipFile(p)as b:
 changed=[n for n in a.namelist()if a.read(n)!=b.read(n)];assert changed==[target]
 result={'existingPr':2934,'existingPrHead':'b7866aaa4905be4d86fb3dff1237d6a70c483f10','sourceMain':'29a3eb23145da90706d8a94441373278521cc737','fixture':'diagnostic copy of official product; exactly existing PR window blur->deactivate computation, no forced UI state','onlyChangedEntry':changed,'beforeModuleSha256':hashlib.sha256(a.read(target)).hexdigest(),'afterModuleSha256':hashlib.sha256(b.read(target)).hexdigest(),'originalArchiveSha256':hashlib.sha256((src/'omni.ja').read_bytes()).hexdigest(),'candidateArchiveSha256':hashlib.sha256(p.read_bytes()).hexdigest(),'binarySha256':hashlib.sha256((dest/'floorp').read_bytes()).hexdigest()}
 (R/'candidate-patch.json').write_text(json.dumps(result,indent=2));print(json.dumps(result))
