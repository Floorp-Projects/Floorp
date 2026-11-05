#!/usr/bin/env python3
"""Reproduce an isolated diagnostic bundle copy; NOT a complete source build."""
import argparse,hashlib,json,zipfile,shutil,subprocess,difflib
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('--variant',choices=['native-owned-v2','css-layer-v2'],required=True);p.add_argument('--base-product',type=Path,required=True);p.add_argument('--out',type=Path,required=True);a=p.parse_args();r=Path(__file__).resolve().parent;meta=json.loads((r/(a.variant+'-provenance.json')).read_text());a.out.mkdir(parents=True,exist_ok=False);member=meta['changedMembers'][0]
with zipfile.ZipFile(a.base_product/'omni.ja') as z:before=z.read(member)
assert hashlib.sha256(before).hexdigest()==meta['originalMemberSha256'];assert hashlib.sha256((a.base_product/'floorp').read_bytes()).hexdigest()==meta['binarySha256']
fixture=a.out/'patch-fixture';f=fixture/member;f.parent.mkdir(parents=True);f.write_bytes(before)
patch=(r/meta['bundleDiff']).resolve();subprocess.run(['git','apply','--check',str(patch)],cwd=fixture,check=True);subprocess.run(['git','apply',str(patch)],cwd=fixture,check=True);after=f.read_bytes();assert hashlib.sha256(after).hexdigest()==meta['changedMemberSha256']
product=a.out/'product';shutil.copytree(a.base_product,product)
with zipfile.ZipFile(a.base_product/'omni.ja') as zi,zipfile.ZipFile(product/'omni.new','w') as zo:
 for info in zi.infolist():zo.writestr(info,after if info.filename==member else zi.read(info))
(product/'omni.new').replace(product/'omni.ja')
with zipfile.ZipFile(a.base_product/'omni.ja') as zi,zipfile.ZipFile(product/'omni.ja') as zo:assert [n for n in zi.namelist() if zi.read(n)!=zo.read(n)]==[member]
assert hashlib.sha256((product/'floorp').read_bytes()).hexdigest()==meta['binarySha256']
print(json.dumps({'product':str(product),'classification':'one-resource diagnostic copy, not complete build','variant':a.variant,'changedMemberSha256':meta['changedMemberSha256']}))
