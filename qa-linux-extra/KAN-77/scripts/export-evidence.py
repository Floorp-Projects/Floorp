from pathlib import Path
import json,shutil,hashlib
r=Path('/workspace/qa-linux-22/C-extra/KAN-77');d=Path('/workspace/Floorp-C-publish/qa-linux-extra/KAN-77');d.mkdir(parents=True,exist_ok=True)
def copy(p,q):q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
for name in ['browser-session.py','control.py','run-phases.py','compare.py','verify-package.py','verify-evidence.py','export-evidence.py']:copy(r/name,d/'scripts'/name)
copy(r/'README.md',d/'scripts/README.md')
for name in ['assignment.json','dispatch-request.json','comparison.json','baseline-product-hashes.json','environment.json']:copy(r/name,d/name)
for name in ['runtime-build-manifest-v2.json','verification.json','floorp-runtime.lock.json','baseline-pwa-source-comparison.json','package-runtime-source/runtime-source-provenance.json']:copy(r/'provenance'/name,d/'provenance'/Path(name).name)
for name in ['package-verification.json','product-files-before.json']:copy(r/'complete-package-final'/name,d/'provenance'/name)
copy(r/'product-hash-after.json',d/'provenance/product-hash-after.json')
for p in (r/'source-analysis').iterdir():
 if p.name in ['report.md','evidence.json']:copy(p,d/'source-analysis'/p.name)
for case in ['baseline','baseline-ui','candidate','candidate-ui']:
 src=r/'trials'/case
 for p in src.iterdir():
  if p.name in ['ready.json','shutdown.json','http-requests.json','measurements.json','test-definition.json'] or (p.name.startswith(('result-','request-')) and p.suffix=='.json') or p.suffix=='.png':copy(p,d/'raw'/case/p.name)
 # console messages are diagnostic only, not a product-failure classifier.
 text=(src/'browser.log').read_text(errors='replace');lines=[x.rstrip() for x in text.splitlines() if ('[SsbPageAction]' in x or '[SiteSpecificBrowserManager]' in x) and ('error' in x.lower() or 'failed' in x.lower())];(d/'raw'/case/'pwa-error-lines.json').write_text(json.dumps(lines,indent=2)+'\n')
# Emit only selected public run metadata; exclude commit authors and download redirects.
for name in ['package-run','package-jobs','package-artifacts','standard-ci']:
 p=r/(name+'.json');x=json.loads(p.read_text())
 if name in ['package-run','standard-ci']:x={k:x.get(k) for k in ['id','html_url','head_sha','head_branch','path','status','conclusion','created_at','updated_at','run_attempt','display_title']}
 elif name=='package-artifacts':x={'artifacts':[{k:a.get(k) for k in ['id','name','digest','size_in_bytes','expired','workflow_run']} for a in x['artifacts']]}
 elif name=='package-jobs':x={'jobs':[{k:j.get(k) for k in ['id','name','status','conclusion','started_at','completed_at','steps']} for j in x['jobs']]}
 (d/'provenance'/(name+'.json')).write_text(json.dumps(x,indent=2)+'\n')
print('allowlist exported; profiles, binaries, cookies, complete logs and signed download URLs excluded')
