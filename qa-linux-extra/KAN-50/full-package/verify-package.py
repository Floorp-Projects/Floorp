#!/usr/bin/env python3
"""Verify and safely extract a complete CI installer; never modify product resources."""
import argparse,hashlib,json,zipfile,tarfile,subprocess
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--zip',type=Path,required=True);p.add_argument('--zip-sha256',required=True);p.add_argument('--out',type=Path,required=True);p.add_argument('--expected-module',type=Path,required=True);p.add_argument('--patch-repo',type=Path,required=True);a=p.parse_args()
assert hashlib.sha256(a.zip.read_bytes()).hexdigest()==a.zip_sha256.removeprefix('sha256:')
a.out.mkdir(parents=True,exist_ok=False)
with zipfile.ZipFile(a.zip) as z:
 members=[i for i in z.infolist() if i.filename.endswith('.tar.xz')];assert len(members)==1,[i.filename for i in members]
 data=z.read(members[0]);archive=a.out/'floorp-linux-x86_64.tar.xz';archive.write_bytes(data)
extract=a.out/'extracted';extract.mkdir()
with tarfile.open(archive) as t:t.extractall(extract,filter='data')
binaries=list(extract.glob('*/floorp'));assert len(binaries)==1,binaries
root=binaries[0].parent;member='moz-src/toolkit/components/pictureinpicture/PictureInPicture.sys.mjs'
with zipfile.ZipFile(root/'omni.ja') as z:module=z.read(member)
expected=a.expected_module.read_bytes()
assert module==expected,'complete package does not contain exact tested source/extracted PiP repair'
assert b'topChromeWindow' in module
checks=[]
repo=a.patch_repo.resolve()
for kind,rel,patch in [('source','toolkit/components/pictureinpicture/PictureInPicture.sys.mjs','.github/patches/floorp-runtime/common/picture-in-picture-app-window.patch'),('extracted',member,'tools/patches/moz-src-toolkit-components-pictureinpicture-PictureInPicture.sys.patch')]:
 fixture=a.out/('patch-check-'+kind);q=fixture/rel;q.parent.mkdir(parents=True);q.write_bytes(module)
 result=subprocess.run(['git','apply','--reverse','--check',str(repo/patch)],cwd=fixture,capture_output=True,text=True);assert result.returncode==0,result.stderr
 checks.append({'patchKind':kind,'patch':patch,'reverseCheckReturnCode':result.returncode})
files=[]
for f in sorted(root.rglob('*')):
 if f.is_symlink():files.append({'path':str(f.relative_to(root)),'symlink':str(f.readlink())})
 elif f.is_file():files.append({'path':str(f.relative_to(root)),'bytes':f.stat().st_size,'sha256':hashlib.sha256(f.read_bytes()).hexdigest()})
(a.out/'product-files-before.json').write_text(json.dumps(files,indent=2)+'\n')
ini={}
for name in ['application.ini','platform.ini']:
 q=root/name
 if q.exists():ini[name]=q.read_text()
r={'artifactZipSha256':a.zip_sha256,'tarMember':members[0].filename,'tarSha256':hashlib.sha256(data).hexdigest(),'tarBytes':len(data),'productRoot':str(root),'executable':str(binaries[0]),'changedProductResources':False,'moduleMember':member,'moduleSha256':hashlib.sha256(module).hexdigest(),'sourcePatchMatchesFullPackage':True,'extractedPatchMatchesFullPackage':True,'patchReverseChecks':checks,'productFiles':len(files),'declaredIni':ini,'classification':'complete package-only installer, untouched extracted product; not diagnostic JS replacement'}
(a.out/'package-verification.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps(r,indent=2))
