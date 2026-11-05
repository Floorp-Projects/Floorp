import json,tarfile,zipfile,hashlib,io,stat
from pathlib import Path
R=Path(__file__).parent;S=Path('/workspace/full-product-3031-3034');D=S/'obj-artifact-build-output/dist/bin';A=S/'obj-artifact-build-output/dist/floorp-12.21.1@159.0.1.en-US.linux-x86_64.tar.xz'
assets=json.loads((R.parent/'3031-3034/production-assets.json').read_text())['entries'];archives={};native=[]
with tarfile.open(A,'r:xz') as t:
 for m in t:
  if not m.isfile():continue
  rel=str(Path(*Path(m.name).parts[1:]));f=t.extractfile(m);head=f.read(4)
  if head==b'\x7fELF':
   h=hashlib.sha256(head)
   for block in iter(lambda:f.read(1048576),b''):h.update(block)
   p=D/rel
   with p.open('rb') as df:dh=hashlib.file_digest(df,'sha256').hexdigest()
   native.append({'path':rel,'packageSha256':h.hexdigest(),'assembledSha256':dh,'matches':dh==h.hexdigest(),'archiveMode':m.mode,'assembledMode':stat.S_IMODE(p.stat().st_mode)})
  elif rel in ['omni.ja','browser/omni.ja']:archives[rel]=zipfile.ZipFile(io.BytesIO(head+f.read()))
records=[]
for a in assets:
 path='chrome/noraneko/'+a['path'];matches=[]
 for name,z in archives.items():
  if path in z.namelist():matches.append((name,hashlib.sha256(z.read(path)).hexdigest()))
 record={'path':path,'assembledSha256':a['distSha256'],'packageEntries':[{'archive':n,'sha256':h} for n,h in matches],'matches':len(matches)==1 and matches[0][1]==a['distSha256']};records.append(record)
result={'package':str(A),'packageSha256':hashlib.file_digest(A.open('rb'),'sha256').hexdigest(),'assetsCompared':len(records),'assetsMatching':sum(x['matches'] for x in records),'nativeCount':len(native),'nativeMatching':sum(x['matches'] for x in native),'assets':records,'native':native};(R/'combined-strict-product-audit.json').write_text(json.dumps(result,indent=2));print({k:v for k,v in result.items() if k not in ['assets','native']});assert all(x['matches'] for x in records+native)
