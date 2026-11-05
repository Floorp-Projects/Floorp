"""Build diagnostic copies from the unchanged official product; never overwrite one."""
from pathlib import Path
import argparse,shutil,zipfile,hashlib,json
ASSET='chrome/noraneko/nora-content/assets/js/index15.js'
def patch_asset(data,variant='combined'):
 s=data.decode();a=s.index('getTotalMovement() {');b=s.index('clearPreventionTimeout()',a)
 old=s[a:b]
 assert old.count('return Math.sqrt(dx * dx + dy * dy);')==1
 new='''getTotalMovement() {
        if (this.mouseTrail.length < 2) return 0;
        const startPoint = this.mouseTrail[0];
        let maxDistance = 0;
        for (const point of this.mouseTrail) {
            maxDistance = Math.max(maxDistance, Math.hypot(point.x - startPoint.x, point.y - startPoint.y));
        }
        return maxDistance;
    }
    '''
 s=s[:a]+new+s[b:]
 if variant=='combined':
  a=s.index('function calculateMeanDirection(trail)');b=s.index('\nfunction ',a+1)
  section=s[a:b];old='for(let i = trail.length - 1; i >= 1; i--)';assert section.count(old)==1
  section=section.replace(old,'for (let i = 1; i < trail.length; i++)');s=s[:a]+section+s[b:]
 return s.encode()
def main():
 r=Path(__file__).parents[1];p=argparse.ArgumentParser();p.add_argument('--variant',choices=['combined','activation-only'],default='combined');p.add_argument('--destination');args=p.parse_args()
 name=args.destination or ('floorp-combined-fixed' if args.variant=='combined' else 'floorp-gesture-fixed')
 src=r/'products/floorp';dest=r/'products'/name
 if dest.exists():raise SystemExit('Destination exists; use a fresh diagnostic destination.')
 shutil.copytree(src,dest,symlinks=True);archive=dest/'omni.ja';temp=archive.with_suffix('.qa.tmp');changes=[]
 with zipfile.ZipFile(archive) as z,zipfile.ZipFile(temp,'w') as out:
  for info in z.infolist():
   data=z.read(info.filename)
   if info.filename==ASSET:
    before=hashlib.sha256(data).hexdigest();assert before=='d279577085f8c7bd3bd74d90a69592b8e495c85f0bd67c2dc85edcb17b4cb442'
    data=patch_asset(data,args.variant);changes.append({'path':ASSET,'beforeSha256':before,'afterSha256':hashlib.sha256(data).hexdigest()})
   out.writestr(info,data)
 temp.replace(archive);(dest/name).symlink_to('floorp')
 (r/f'KAN-98/{name}-patch.json').write_text(json.dumps({'source':'unchanged official Floorp12.21.1','variant':args.variant,'changes':changes},indent=2))
if __name__=='__main__':main()
