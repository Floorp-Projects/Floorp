from pathlib import Path
import json,hashlib,shutil,re
r=Path('/workspace/qa-linux-22/C-extra/KAN-19');d=Path('/workspace/Floorp-C-publish/qa-linux-extra/KAN-19');d.mkdir(parents=True,exist_ok=True)
def cp(p,q):q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
for name in ['report.md','summary.json','product-hash-baseline.json','product-hash-final.json']:cp(r/name,d/name)
for name in ['browser-session.py','control.py','run-pastes.py','verify-evidence.py','export-evidence.py']:cp(r/name,d/'scripts'/name)
cp(r/'README.md',d/'scripts/README.md')
for name in ['index.html','editor.ts','package.json','package-lock.json']:cp(r/'fixture'/name,d/'fixture'/name)
for name in ['verification.json','floorp-12.18.1/hashes.txt','floorp-12.18.1/linux-meta.json']:cp(r/'acquisition'/name,d/'acquisition'/name)
# Mozilla fullchecksum list is large; retain the exact verified member line.
text=(r/'acquisition/firefox-156.0.1/SHA256SUMS').read_text();(d/'acquisition/firefox-156.0.1-SHA256SUMS-excerpt.txt').write_text(next(x for x in text.splitlines() if x.endswith('linux-x86_64/en-US/firefox-156.0.1.tar.xz'))+'\n')
for name in ['report.md','evidence.json']:
 p=r/'source-analysis/floorp'/name
 if p.exists():cp(p,d/'source-analysis/floorp'/name)
for name in ['initRoosterEditor.ts','EditorCustomPastePlugin.ts','EditorCustomPastePlugin.test.ts','transformLinkify.ts','linkifyInstance.ts','browser-helpers.ts','rooster-lock-excerpts.json','fixture-version-comparison.json','upstream-firefox-paste-search.json','upstream-duplicate-search.json']:
 p=r/'source-analysis/proton'/name
 if p.exists():cp(p,d/'source-analysis/proton'/name)
for c in ['floorp-current','floorp-reported','firefox-current','firefox-reported']:
 src=r/'trials'/c
 for p in src.iterdir():
  if p.suffix=='.png' or p.name in ['ready.json','shutdown.json','test-definition.json','paste-results.json'] or (p.suffix=='.json' and p.name.startswith(('request-','result-'))):cp(p,d/'raw'/c/p.name)
print('allowlist only: no accounts/profiles/binaries/node_modules/full logs/temporaryURLs')
