from pathlib import Path
import json,hashlib,shutil,re
r=Path('/workspace/qa-linux-22/C-extra/KAN-37');d=Path('/workspace/Floorp-C-publish/qa-linux-extra/KAN-37');d.mkdir(parents=True,exist_ok=True)
def copy(p,q):q.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,q)
for f in ['report.md','summary.json','environment.json','product-verification.json','packaged-stock-policy.json','fixture-favicon.png']:copy(r/f,d/f)
for f in ['browser-session.py','control.py','native-fixture.py','paint-probe.js','run-native-paints.py','export-evidence.py','verify-evidence.py']:copy(r/f,d/'scripts'/f)
copy(r/'README.md',d/'scripts/README.md')
for f in ['report.md','evidence.json','floorp-modules_AboutNewTab.sys.mjs','floorp-modules_AboutNewTabRedirector.sys.mjs','floorp-modules_TelemetryReportingPolicy.sys.mjs','floorp-chrome_remote_content_shared_RecommendedPreferences.sys.mjs','floorp-chrome_browser_builtin-addons_newtab_lib_ActivityStream.sys.mjs']:copy(r/'source-analysis'/f,d/'source-analysis'/f)
qualified=json.loads((r/'summary.json').read_text())['qualifiedCurrentAndControlSessions']
for case in qualified:
 src=r/'trials'/case
 for p in src.iterdir():
  if p.name in ['ready.json','shutdown.json','test-definition.json','paint-results.json','occluded-paint-result.json'] or (p.name.startswith(('request-native-','result-native-','request-state-','result-state-','request-before-','result-before-','request-disable','result-disable','request-open-','result-open-','request-settings','result-settings','request-policy-','result-policy-')) and p.suffix=='.json') or p.suffix=='.png':copy(p,d/'raw'/case/p.name)
 # Narrow nativeconsole diagnostics; no headers, personalURLs or redirect tokens.
 lines=[]
 for line in (src/'browser.log').read_text(errors='replace').splitlines():
  if any(k in line for k in ['AboutNewTab','TelemetryReportingPolicy','ActivityStream','NewTab','ManifestMessages']) and any(k in line.lower() for k in ['error','warning','failed']):lines.append(re.sub(r'https?://[^\s\"<>]+','[url-redacted]',line.rstrip()))
 (d/'raw'/case/'native-console-lines.json').write_text(json.dumps(lines,indent=2)+'\n')
# Invalid/prequalification attempts: only small actual state/error/termination evidence, no product claims.
for case in ['floorp-seed','floorp-native','firefox-policy','floorp-reported-seed','floorp-reported-native','floorp-reported-native-v2']:
 src=r/'trials'/case;out=d/'diagnostics'/case;out.mkdir(parents=True,exist_ok=True)
 for name in ['ready.json','session-error.json','shutdown.json','result-before-quit.json','result-native-initial.json']:
  p=src/name
  if p.exists():copy(p,out/name)
 if case=='floorp-reported-native-v2':
  lines=[x.strip() for x in (src/'browser.log').read_text(errors='replace').splitlines() if 'CPD: got update status' in x];(out/'actual-update-status-lines.json').write_text(json.dumps(lines,indent=2)+'\n')
print('allowlist exported; profiles/binaries/cookies/full logs/temporary URLs excluded')
