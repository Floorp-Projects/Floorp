"""Collect XPIDL contexts from unchanged artifact config; run canonical processor."""
import sys,json,hashlib,importlib.util,time
from pathlib import Path
from collections import defaultdict
from mozbuild.backend.configenvironment import ConfigEnvironment
from mozbuild.frontend.reader import BuildReader
SOURCE=Path('/workspace/product3033/runtime');OBJ=Path('/workspace/product3033/obj-artifact-build-output');OUT=Path(__file__).parent
config=ConfigEnvironment.from_config_status(str(OBJ/'config.status'))
modules=defaultdict(list);count=0
for ctx in BuildReader(config).read_topsrcdir():
 count+=1
 if ctx.get('XPIDL_SOURCES'):
  module=ctx['XPIDL_MODULE']
  assert module,ctx.main_path
  for source in ctx['XPIDL_SOURCES']:
   modules[module].append(str(source.full_path))
manifest={'sourceCommit':'8965ffc346d426ccdf919bd18585e4d4a0c21654','configSha256':hashlib.sha256((OBJ/'config.status').read_bytes()).hexdigest(),'compileEnvironment':config.substs.get('COMPILE_ENVIRONMENT'),'contextCount':count,'modules':[]}
for name,inputs in sorted(modules.items()):
 assert len(inputs)==len(set(inputs)),name
 manifest['modules'].append({'module':name,'inputs':[{'path':str(Path(p).relative_to(SOURCE)),'sha256':hashlib.sha256(Path(p).read_bytes()).hexdigest()} for p in sorted(inputs)]})
(OUT/'inputs.json').write_text(json.dumps(manifest,indent=2));print('Collected',len(modules),'modules',sum(map(len,modules.values())),'IDLs',flush=True)
if '--generate' not in sys.argv:sys.exit()
sys.path.insert(0,str(SOURCE/'xpcom/idl-parser'))
spec=importlib.util.spec_from_file_location('canonical_xpidl_processor',SOURCE/'python/mozbuild/mozbuild/action/xpidl-process.py');proc=importlib.util.module_from_spec(spec);spec.loader.exec_module(proc)
stage=OUT/'generated';header=stage/'include';rust=stage/'xpcrs';xpt=stage/'xpt';deps=stage/'deps'
for d in [header,rust/'rt',rust/'bt',xpt,deps]:d.mkdir(parents=True,exist_ok=True)
inc=sorted({str(Path(p).parent) for inputs in modules.values() for p in inputs})
results=[]
for name,inputs in sorted(modules.items()):
 start=time.monotonic();print('Generating',name,len(inputs),flush=True)
 proc.process([],inc,str(SOURCE/'dom/bindings/Bindings.conf'),str(header),str(rust),str(xpt),str(deps),name,sorted(inputs))
 p=xpt/(name+'.xpt');results.append({'module':name,'seconds':round(time.monotonic()-start,3),'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()})
 (OUT/'outputs.json').write_text(json.dumps(results,indent=2))
assert len(results)==len(modules)
print('All modules generated',len(results),flush=True)
