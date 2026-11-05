from pathlib import Path
import zipfile,hashlib,json
r=Path(__file__).parent;b=r.parents[1];records=[]
for name in ['floorp-pr2877-baseline-tests','floorp-pr2877-candidate-tests']:
 p=b/'products'/name/'omni.ja';tmp=p.with_suffix('.tmp')
 with zipfile.ZipFile(p) as z,zipfile.ZipFile(tmp,'w') as out:
  for info in z.infolist():
   if not info.filename.startswith('chrome/noraneko/nora-content/qa-pr2877-'):out.writestr(info,z.read(info.filename))
  for kind in ['flex','panel']:
   content=(r/'tests'/(kind+'.js')).read_bytes()+b'\nrunAllTests().then(()=>{window.__qa2877Result={passed:true,progress:window.__NORA_TEST_PROGRESS__}}).catch(e=>{window.__qa2877Result={passed:false,error:String(e),stack:e.stack,progress:window.__NORA_TEST_PROGRESS__}});\n';out.writestr('chrome/noraneko/nora-content/qa-pr2877-'+kind+'.js',content)
 tmp.replace(p);records.append({'product':name,'withTestModuleArchiveSha256':hashlib.sha256(p.read_bytes()).hexdigest()})
(r/'test-module-hashes.json').write_text(json.dumps(records,indent=2))
