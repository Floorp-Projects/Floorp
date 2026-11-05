import json,hashlib
from pathlib import Path
from collections import defaultdict
from mozbuild.backend.configenvironment import ConfigEnvironment
from mozbuild.frontend.reader import BuildReader
R=Path('/workspace/qa-linux-22/browser-ui/full-product/xpt-generation');S=Path('/workspace/kan30-full/runtime');O=S.parent/'obj-artifact-build-output'
config=ConfigEnvironment.from_config_status(str(O/'config.status'));modules=defaultdict(list)
for ctx in BuildReader(config).read_topsrcdir():
 if ctx.get('XPIDL_SOURCES'):
  for f in ctx['XPIDL_SOURCES']:modules[ctx['XPIDL_MODULE']].append(Path(f.full_path))
actual=[{'module':m,'inputs':[{'path':str(p.relative_to(S)),'sha256':hashlib.sha256(p.read_bytes()).hexdigest()} for p in sorted(ps)]} for m,ps in sorted(modules.items())]
baseline=json.loads((R/'inputs.json').read_text())['modules'];deps=json.loads((R/'dependencies.json').read_text());differences=[]
for dep in deps:
 p=S/dep['path']
 if not p.is_file() or hashlib.sha256(p.read_bytes()).hexdigest()!=dep['sha256']:differences.append(dep['path'])
result={'combinedSelectedModules':len(actual),'combinedSelectedIdls':sum(len(m['inputs']) for m in actual),'moduleInputsIdentical':actual==baseline,'sourceGeneratorAndDependencyHashesCompared':len(deps),'dependencyDifferences':differences}
(Path('/workspace/qa-linux-additional/browser-ui/KAN-30/full-product/xpt-comparison.json')).write_text(json.dumps(result,indent=2));print(json.dumps(result));assert actual==baseline and not differences
