from pathlib import Path
import json,shutil,hashlib,statistics
r=Path('/workspace/qa-linux-22/C-extra/KAN-86');dst=Path('/workspace/Floorp-C-publish/qa-linux-extra/KAN-86');dst.mkdir(parents=True,exist_ok=True)
def copy(p,to):to.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(p,to)
for name in ['browser-session.py','control.py','export-evidence.py']:copy(r/name,dst/'scripts'/name)
for p in (r/'source-analysis').iterdir():
 if p.suffix in ('.tsx','.mts','.ts','.cpp') or p.name in ['report.md','source-hashes.json','matrix-delta.json']:copy(p,dst/'source-analysis'/p.name)
for name in ['verification.json','12.18.0/hashes.txt','12.18.0/linux-meta.json','12.18.0/release-manifest-set-v2.json']:copy(r/'acquisition'/name,dst/'acquisition'/name)
for name in ['product-verification.json','patch/update.log','patch/update.status']:copy(r/'update-chain'/name,dst/'update-chain'/name)
copy(r/'old-updater-channels.txt',dst/'update-chain/old-updater-channels.txt')
cases=['old-seed-v2','mar-to18','mar18-normal-restart','current-fresh','current-update-fixture','current-prompt-save'];summary=[]
for case in cases:
 src=r/'trials'/case
 for p in src.iterdir():
  if p.name in ['ready.json','shutdown.json','cpu-samples.json','test-definition.json'] or (p.name.startswith(('result-','request-')) and p.suffix=='.json') or p.suffix=='.png':copy(p,dst/'raw'/case/p.name)
 a=json.loads((src/'cpu-samples.json').read_text());ready=json.loads((src/'ready.json').read_text());shutdown=json.loads((src/'shutdown.json').read_text())
 summary.append({'case':case,'ready':ready,'shutdown':shutdown,'sampleCount':len(a),'monitoredSeconds':sum(x['intervalSeconds'] for x in a),'cpuPercentSumMax':max(x['cpuPercentSum'] for x in a),'cpuPercentSumMedian':statistics.median(x['cpuPercentSum'] for x in a)})
 # Browser logs are reviewed locally; export only attributable errors without paths/headers.
 log=(src/'browser.log').read_text(errors='replace');lines=[x for x in log.splitlines() if any(s in x.lower() for s in ['error:','warning:','crash','hang'])]; (dst/'raw'/case/'error-lines.txt').write_text('\n'.join(lines)+'\n')
(dst/'raw/summary.json').write_text(json.dumps(summary,indent=2)+'\n')
# Initial 5s automation handshake was invalid, not a qualified product trial.
copy(r/'trials/old-seed/session-error.json',dst/'raw/harness-invalid-initial-handshake.json')
files=[{'path':str(p.relative_to(dst)),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(dst.rglob('*')) if p.is_file() and p.name!='export-manifest.json']
(dst/'export-manifest.json').write_text(json.dumps({'allowlistOnly':True,'excluded':['profiles','cookies','credential/environment values','binaries','signed download URLs','full browser logs'],'files':files},indent=2)+'\n')
print(json.dumps([{'case':s['case'],'seconds':round(s['monitoredSeconds'],1),'exit':s['shutdown']} for s in summary],indent=2))
