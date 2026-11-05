"""Verify the complete canonical generated set, then supply both compared candidates."""
import hashlib,json,shutil
from pathlib import Path
root=Path(__file__).parent
comparison=json.loads((root/'combined-module-comparison.json').read_text())
assert comparison['moduleInputsIdentical'] and not comparison['dependencyDifferences']
outputs=json.loads((root/'outputs.json').read_text())
assert len(outputs)==comparison['combinedSelectedModules']==155
expected={row['module']+'.xpt' for row in outputs}
source=root/'generated/xpt'
assert {p.name for p in source.glob('*.xpt')}==expected
for row in outputs:
 p=source/(row['module']+'.xpt')
 assert p.stat().st_size==row['bytes'] and hashlib.sha256(p.read_bytes()).hexdigest()==row['sha256']
for stage in ['/workspace/product3033','/workspace/full-product-3031-3034']:
 target=Path(stage)/'obj-artifact-build-output/dist/xpt_artifacts'
 target.mkdir(parents=True,exist_ok=True)
 assert not ({p.name for p in target.glob('*.xpt')}-expected)
 for name in sorted(expected):shutil.copy2(source/name,target/name)
 assert {p.name for p in target.glob('*.xpt')}==expected
print('All155 canonical XPT modules verified and supplied to both compared candidates.')
