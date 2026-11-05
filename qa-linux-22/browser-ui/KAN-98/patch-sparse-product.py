from pathlib import Path
import zipfile,hashlib,json
r=Path(__file__).parents[1];p=r/'products/floorp-sparse-fixed/omni.ja';tmp=p.with_suffix('.tmp');changes=[]
with zipfile.ZipFile(p) as src,zipfile.ZipFile(tmp,'w') as dst:
 for info in src.infolist():
  data=src.read(info.filename)
  if info.filename=='chrome/noraneko/nora-content/assets/js/index15.js':
   s=data.decode();a=s.index('function calculateMeanDirection(trail)');b=s.index('\nfunction ',a+1);section=s[a:b];
   expected='for(let i = trail.length - 1; i >= 1; i--)';assert section.count(expected)==1
   changes.append({'path':info.filename,'beforeSha256':hashlib.sha256(data).hexdigest()});section=section.replace(expected,'for (let i = 1; i < trail.length; i++)');data=(s[:a]+section+s[b:]).encode();changes[-1]['afterSha256']=hashlib.sha256(data).hexdigest()
  dst.writestr(info,data)
tmp.replace(p);(r/'KAN-98/sparse-product-patch.json').write_text(json.dumps(changes,indent=2));(r/'products/floorp-sparse-fixed/floorp-sparse-fixed').symlink_to('floorp')
