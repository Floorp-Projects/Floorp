#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Same-worker native build/product assembly. No publication or remote writes."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import zipfile
import build_recipe
import local_cohort


def run(script, cwd, env, logfile):
    with logfile.open('w') as stream:
        result=subprocess.run(['bash',str(script)],cwd=cwd,env=env,stdout=stream,stderr=subprocess.STDOUT)
    if result.returncode:raise subprocess.CalledProcessError(result.returncode,['bash',str(script)])


def environment_after_phase(name, env, runtime):
    result = env.copy()
    # The official configure script uses MOZCONFIG as an unexported template
    # selector. Exporting it beforehand would make mach read that template,
    # not the amended root mozconfig. Only pin the root after configure passes.
    if name == 'Mach configure':
        config = runtime / 'mozconfig'
        if not config.is_file():
            raise ValueError('Mach configure did not produce root mozconfig')
        result['MOZCONFIG'] = str(config)
    return result


def stage_product(objdir, output, injected_xhtml=None):
    # mach package stages its actual packaged resource tree here; never substitute
    # dist/bin or an unrelated previously released archive when packaging failed.
    source=objdir/'dist/floorp'
    for name in ('floorp','application.ini','omni.ja','browser/omni.ja','browser/buildid2'):
        if not (source/name).is_file():raise ValueError('Missing final packaged product: '+name)
    if injected_xhtml is not None:
        with zipfile.ZipFile(source/'browser/omni.ja') as archive:
            packaged = archive.read('chrome/browser/content/browser/browser.xhtml')
        if packaged != injected_xhtml:
            raise ValueError('packaged browser XHTML differs from actual injection target')
    shutil.copytree(source,output,symlinks=False)
    modules=objdir/'_tests/modules'
    if not modules.is_dir():raise ValueError('same-OBJDIR testing modules missing')
    shutil.copytree(modules,output/'testing-common',dirs_exist_ok=False)
    if local_cohort.tree(modules)!=local_cohort.tree(output/'testing-common'):
        raise ValueError('testing module copy mismatch')
    return output/'floorp'


def tool_versions(env):
    result={}
    for name in ('git','node','deno','rustc','python3'):
        try:
            response=subprocess.run([name,'--version'],env=env,text=True,capture_output=True,timeout=30)
            result[name]={'exit_code':response.returncode,'stdout':response.stdout,'stderr':response.stderr}
        except (OSError,subprocess.TimeoutExpired) as error:
            result[name]={'unavailable':str(error)}
    return result


def build(args):
    runtime=args.runtime.resolve();floorp=runtime/'noraneko'
    # Fail before bootstrap/download/build when either input is not the requested
    # source. All legitimate workflow patches are recorded after this boundary.
    for root,expected in ((runtime,args.runtime_sha),(floorp,args.floorp_sha)):
        if local_cohort.git_sha(root)!=expected:raise ValueError('source mismatch')
        subprocess.run(['git','-C',str(root),'diff','--exit-code','HEAD','--'],check=True)
    output=args.output.resolve();output.mkdir(parents=True,exist_ok=False)
    recipe=output/'recipe';steps=build_recipe.write(floorp/'.github/workflows/package.yml',recipe)
    # No inherited cloud/API token or GitHub command-file channels in build code.
    keep={'PATH','HOME','LANG','LC_ALL','TMPDIR','MOZBUILD_STATE_PATH','RUSTUP_HOME','CARGO_HOME'}
    env={k:v for k,v in os.environ.items() if k in keep}
    env.update(GITHUB_WORKSPACE=str(runtime),GITHUB_ENV=str(output/'phase.env'),
               INPUT_PLATFORM='Linux-x86_64',CANON_PLATFORM='Linux-x86_64',
               IS_BETA='false',CI='true',MOZ_BUILD_DATE=datetime.now(timezone.utc).strftime('%Y%m%d%H%M%S'),
               DENO_EXECUTABLE_PATH=shutil.which('deno') or '',LIBGL_ALWAYS_SOFTWARE='1')
    subprocess.run(['node',str(Path(__file__).with_name('test-injection-target.mjs')),str(floorp)],env=env,check=True)
    (output/'tool-versions-before.json').write_text(json.dumps(tool_versions(env),indent=2))
    results=[]
    try:
        for step in steps:
            script=recipe/step['script']
            if local_cohort.digest(script)!=step['sha256']:raise ValueError('recipe changed')
            row=dict(step,status='running');results.append(row)
            (output/'build-results.json').write_text(json.dumps(results,indent=2))
            run(script,runtime,env,output/(step['script']+'.log'))
            env=environment_after_phase(step['name'],env,runtime)
            if step['name']=='Inject Noraneko':
                target=runtime/'obj-artifact-build-output/dist/bin'
                log=(output/(step['script']+'.log')).read_text()
                if 'CI platform detected, setting binPath to: '+str(target) not in log:
                    raise ValueError('injector did not confirm the intended native product target')
                xhtml=target/'browser/chrome/browser/content/browser/browser.xhtml'
                injected=xhtml.read_bytes()
                if b'chrome://noraneko-startup/content/chrome_root.js' not in injected:
                    raise ValueError('Floorp startup was not injected into actual native product')
                (output/'injected-browser.xhtml').write_bytes(injected)
            row['status']='success'
            # The official configure phase exports only this value for later
            # phases. Do not source arbitrary generated shell/GITHUB_ENV data.
            if (output/'phase.env').exists():
                for line in (output/'phase.env').read_text().splitlines():
                    key,sep,value=line.partition('=')
                    if not sep or key!='UPDATE_CHANNEL' or not value or '\n' in value:
                        raise ValueError('unreviewed build environment export')
                    env[key]=value
        obj=runtime/'obj-artifact-build-output'
        binary=stage_product(obj,output/'product',(output/'injected-browser.xhtml').read_bytes())
        # Create the checkout-owned common env using the native command; import
        # probe in v3 fails if this checkout's dependencies are not available.
        probe=output/'create-common-env.sh'
        probe.write_text('./mach python --virtualenv common -c "import sys; print(sys.executable)"\n')
        run(probe,runtime,env,output/'common-python.log')
        candidates=[Path(line.strip()) for line in (output/'common-python.log').read_text().splitlines()
                    if line.startswith('/') and line.strip().endswith('/bin/python')]
        if len(candidates)!=1 or not candidates[0].is_file():raise ValueError('cannot identify exact common Python')
        paths={'floorp':str(floorp),'runtime':str(runtime),'objdir':str(obj),'binary':str(binary),
               'resources':str(binary.parent),'modules':str(binary.parent/'testing-common'),
               'python':sys.executable,'xpcshell_python':str(candidates[0])}
        receipt=local_cohort.snapshot(paths,args.floorp_sha,args.runtime_sha)
        (output/'cohort.json').write_text(json.dumps(receipt,indent=2))
        (output/'paths.json').write_text(json.dumps(paths,indent=2))
    finally:
        (output/'tool-versions-after.json').write_text(json.dumps(tool_versions(env),indent=2))
        (output/'build-results.json').write_text(json.dumps(results,indent=2))

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--runtime',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    p.add_argument('--floorp-sha',required=True);p.add_argument('--runtime-sha',required=True)
    build(p.parse_args())
