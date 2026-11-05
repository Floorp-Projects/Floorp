from pathlib import Path
import shutil,zipfile,json,hashlib
r=Path(__file__).parent;b=r.parents[1];result=[]
for source,name in [('floorp','floorp-pr2877-baseline-tests'),('floorp-pr2877','floorp-pr2877-candidate-tests')]:
 src=b/'products'/source;dest=b/'products'/name;assert not dest.exists(),dest;shutil.copytree(src,dest,symlinks=True);p=dest/'omni.ja';tmp=p.with_suffix('.tmp');changes=[]
 with zipfile.ZipFile(p) as z,zipfile.ZipFile(tmp,'w') as out:
  for info in z.infolist():
   data=z.read(info.filename);s=data.decode() if info.filename.endswith('.js') else ''
   exports={'chrome/noraneko/nora-content/assets/js/index10.js':('export { _FlexOrder as default };','export { _FlexOrder as default, gFlexOrder };'),'chrome/noraneko/nora-content/assets/js/index16.js':('export { _PanelSidebar as default };','export { _PanelSidebar as default, PanelNavigator };')}
   if info.filename in exports:
    before,after=exports[info.filename];assert s.count(before)==1;s=s.replace(before,after);changes.append(info.filename);data=s.encode()
   if info.filename=='chrome/noraneko/nora-content/assets/js/data.js':assert 'setIsPanelSidebarEnabled' in s;data=(s+'\nexport { setIsPanelSidebarEnabled };\n').encode();changes.append(info.filename)
   out.writestr(info,data)
 tmp.replace(p);(dest/name).symlink_to('floorp');result.append({'source':source,'testProduct':str(dest),'diagnosticExportsOnly':changes,'archiveSha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(r/'test-copy-hashes.json').write_text(json.dumps(result,indent=2));print(json.dumps(result,indent=2))
