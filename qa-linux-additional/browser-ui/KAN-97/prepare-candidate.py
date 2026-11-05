from pathlib import Path
import shutil,zipfile,hashlib,json
R=Path(__file__).parent;src=Path('/workspace/qa-linux-22/browser-ui/products/floorp');dst=Path('/workspace/kan97-runtime/candidate');dst.mkdir()
for p in src.iterdir():
 if p.name in ['omni.ja','floorp','floorp-bin']:shutil.copy2(p,dst/p.name)
 else:(dst/p.name).symlink_to(p,target_is_directory=p.is_dir())
helper=(R/'helper-compiled.js').read_text();archive=dst/'omni.ja';tmp=dst/'omni.tmp';target='chrome/noraneko/nora-content/assets/js/gestures.js';old='const actor = browser.browsingContext?.currentWindowGlobal?.getActor("NRMouseGestureScroll");';new='''const selectedContext = browser.browsingContext;
        if (!selectedContext) return;
        const context = getScrollTargetContext(selectedContext, Services.focus.focusedContentBrowsingContext);
        const actor = context.currentWindowGlobal?.getActor("NRMouseGestureScroll");'''
with zipfile.ZipFile(archive) as z,zipfile.ZipFile(tmp,'w') as out:
 for info in z.infolist():
  data=z.read(info.filename)
  if info.filename==target:
   code=data.decode();assert code.count(old)==1;assert 'function getScrollTargetContext(' not in code;code=code.replace('const sendScrollCommand =',helper+'\nconst sendScrollCommand =',1).replace(old,new);data=code.encode()
  out.writestr(info,data)
tmp.replace(archive)
with zipfile.ZipFile(src/'omni.ja') as a,zipfile.ZipFile(archive) as b:
 changed=[n for n in a.namelist() if a.read(n)!=b.read(n)];assert changed==[target]
 meta={'fixture':'compiled-module diagnostic copy; NOT a full production build/package','sourceBase':'29a3eb23145da90706d8a94441373278521cc737','onlyChangedEntry':changed,'beforeModuleSha256':hashlib.sha256(a.read(target)).hexdigest(),'afterModuleSha256':hashlib.sha256(b.read(target)).hexdigest(),'archiveBefore':hashlib.sha256((src/'omni.ja').read_bytes()).hexdigest(),'archiveAfter':hashlib.sha256(archive.read_bytes()).hexdigest(),'helperSourceSha256':hashlib.sha256(Path('/workspace/floorp-kan97/browser-features/chrome/common/mouse-gesture/utils/scroll-target.ts').read_bytes()).hexdigest(),'nativeBinaryCopyMatches':hashlib.sha256((src/'floorp').read_bytes()).hexdigest()==hashlib.sha256((dst/'floorp').read_bytes()).hexdigest(),'unchangedResources':'shared static dependencies via existing diagnostic-copy recipe; no build is run against this product; hash audit after execution'}
 (R/'candidate-patch.json').write_text(json.dumps(meta,indent=2));print(json.dumps(meta))
