import argparse,hashlib,json,zipfile,tarfile,re,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zip',type=Path,required=True);p.add_argument('--digest',required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();assert hashlib.sha256(a.zip.read_bytes()).hexdigest()==a.digest.removeprefix('sha256:');a.out.mkdir(parents=True,exist_ok=False)
with zipfile.ZipFile(a.zip) as z:
 members=[x for x in z.namelist() if x.endswith('.tar.xz')];assert len(members)==1;data=z.read(members[0]);tar=a.out/'floorp-linux-x86_64.tar.xz';tar.write_bytes(data)
with tarfile.open(tar) as t:t.extractall(a.out/'extracted',filter='data')
roots=list((a.out/'extracted').glob('*/floorp'));assert len(roots)==1;root=roots[0].parent;snips=[]
for ja in [root/'omni.ja',root/'browser/omni.ja']:
 with zipfile.ZipFile(ja) as z:
  for n in z.namelist():
   if n.endswith('NRProgressiveWebAppParent.sys.mjs') or (n.endswith('.js') and b'nora-pwa-check-page-has-manifest' in z.read(n)):
    text=z.read(n).decode();(a.out/Path(n).name).write_text(text)
    for m in re.finditer('nora-pwa-check-page-has-manifest|isTopLevel|currentWindowGlobal',text):snips.append({'resource':n,'offset':m.start(),'snippet':text[max(0,m.start()-600):m.end()+600]})
assert any('currentWindowGlobal' in x['snippet'] and 'NRProgressiveWebAppParent' in x['resource'] for x in snips)
sourceMatches=[]
with zipfile.ZipFile(root/'omni.ja') as z:
 for name in z.namelist():
  if not name.endswith('.js.map'):continue
  sm=json.loads(z.read(name))
  for i,src in enumerate(sm.get('sources',[])):
   if src.endswith(('SsbPageAction.tsx','ssbManager.ts')):
    rel='browser-features/chrome/common/pwa/'+src.rsplit('/',1)[1];expected=subprocess.check_output(['git','-C','/workspace/Floorp-manifest-linux','show','f04ff611c3634515064776e2d58e76648f87269b:'+rel]).decode();assert sm['sourcesContent'][i]==expected;sourceMatches.append({'path':rel,'sourceMap':name,'matchesExactHead':True,'sha256':hashlib.sha256(expected.encode()).hexdigest()})
assert len(sourceMatches)==2
assert any('isTopLevel' in x['snippet'] and 'assets/js/' in x['resource'] for x in snips)
assert any('nora-pwa-check-page-has-manifest' in x['snippet'] and 'selectedBrowser' in x['snippet'] and 'assets/js/' in x['resource'] for x in snips)
files=[]
for f in sorted(root.rglob('*')):
 if f.is_symlink():files.append({'path':str(f.relative_to(root)),'symlink':str(f.readlink())})
 elif f.is_file():files.append({'path':str(f.relative_to(root)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(a.out/'product-files-before.json').write_text(json.dumps(files,indent=2)+'\n');report={'sourceSha':'f04ff611c3634515064776e2d58e76648f87269b','artifactDigest':a.digest,'tarMember':members[0],'tarSha256':hashlib.sha256(data).hexdigest(),'productRoot':str(root),'changedResources':False,'fileEntries':len(files),'sourceMapMatches':sourceMatches,'guardExcerpts':snips};(a.out/'package-verification.json').write_text(json.dumps(report,indent=2)+'\n');print(json.dumps({k:v for k,v in report.items() if k!='guardExcerpts'},indent=2))
