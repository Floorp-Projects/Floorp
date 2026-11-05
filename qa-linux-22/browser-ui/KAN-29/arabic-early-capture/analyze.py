"""Analyze private captures; publish only technical aggregate data and public code labels."""
from pathlib import Path
import json,collections,hashlib,re
R=Path(__file__).resolve().parent;O=Path('/workspace/kan29-arabic-early/private')
out={'scope':'One fresh English setup and first requested-ar startup; no repeated Arabic runs','phases':{}}
for phase,pid in [('english',120558),('arabic',120948)]:
 p=O/phase/f'profile_0_{pid}.json';o=json.loads(p.read_text());t=o['threads'][0];ss=t['stringTable'];fr=t['frameTable']['data'];st=t['stackTable']['data'];counts=collections.Counter();bins={};last=[];post=0;formatHits=0
 for s in t['samples']['data']:
  i=s[0];labs=[]
  while i is not None:
   l=ss[fr[st[i][1]][0]]
   if not l.startswith('0x')and l!='(root)':labs.append(l)
   i=st[i][0]
  b=int(s[1]//1000);bins.setdefault(b,{'samples':0,'syncLocalization':0,'contextMenuSeed':0});bins[b]['samples']+=1
  if 'Localization.formatMessagesSync'in labs:bins[b]['syncLocalization']+=1
  if any(l.startswith('seedInitialCatalog ')for l in labs):bins[b]['contextMenuSeed']+=1
  if s[1]>=3000:
   post+=1;formatHits+=int('Localization.formatMessagesSync'in labs);counts.update(set(labs))
  last=labs
 # Filter public symbol names; raw URLs/addresses/paths and complete traces remain private.
 def safe(l):return not re.search(r'(https?://|file:|/workspace/|/home/)',l)
 malloc=next(c for c in o['counters']if c['name']=='malloc');total=0;curve=[]
 for r in malloc['samples']['data']:total+=r[1];curve.append({'ms':r[0],'emittedCumulativeBytes':total})
 lastSec={str(n):next((r['emittedCumulativeBytes']for r in reversed(curve)if r['ms']<=n*1000),None)for n in range(1,8)}
 out['phases'][phase]={'pid':pid,'parentSamples':len(t['samples']['data']),'post3sSamples':post,'post3sLocalizationFormatMessagesSyncSamples':formatHits,'perSecond':bins,'post3sInclusiveSymbolicTop':[{'frame':l,'samples':v}for l,v in counts.most_common(30)if safe(l)],'lastSymbolicStack':[l for l in last if safe(l)][:30],'mallocEmittedCumulativeBySecond':lastSec,'mallocEmittedLast':curve[-1],'mallocCaveat':'cumulative emitted malloc counter, not JS/native/DOM allocation attribution; startup first emitted value and profiler overhead apply','metaFeatures':o['meta']['configuration']['features']}
(R/'stack-analysis.json').write_text(json.dumps(out,indent=2)+'\n')
print(json.dumps({k:{'samples':v['post3sSamples'],'formatSync':v['post3sLocalizationFormatMessagesSyncSamples']}for k,v in out['phases'].items()}))
