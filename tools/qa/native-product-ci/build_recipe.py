#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Compile reviewed Linux native pilot phases from the normal package workflow.
No scripts execute during compilation. Reject unknown workflow substitutions.
"""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
import yaml

PHASES=('Apply upstream patches','Apply Floorp common Runtime patches',
        'Setup Noraneko','Write Versions','Copy branding files from runtime',
        'Mach configure','Build Noraneko','Inject Noraneko','Apply Firefox Preferences Override')
REPLACEMENTS={'${{ inputs.platform }}':'Linux-x86_64','${{inputs.platform}}':'Linux-x86_64','${{ github.repository }}':'Floorp-Projects/Floorp'}


def extract(path):
    document=yaml.safe_load(path.read_text())
    steps=[s for job in document['jobs'].values() for s in job.get('steps',[]) if 'run' in s]
    result={}
    for name in PHASES:
        matches=[s['run'] for s in steps if s.get('name')==name]
        if len(matches)!=1:raise ValueError('missing/ambiguous normal phase: '+name)
        text=matches[0]
        for key,value in REPLACEMENTS.items():text=text.replace(key,value)
        if '${{' in text:raise ValueError('unreviewed expression in '+name)
        result[name]=text
    # Same normal packaging frontend/branding/prefs, but a native test-enabled
    # debug OBJDIR. Do not use an artifact-only build to implement a C++ change.
    config=result['Mach configure']
    for old,new in [('ac_add_options --disable-tests','ac_add_options --enable-tests'),
                    ('ac_add_options --enable-artifact-builds','ac_add_options --enable-debug')]:
        if config.count(old)!=1:raise ValueError('changed native configuration boundary: '+old)
        config=config.replace(old,new)
    result['Mach configure']=config
    result['Native bootstrap']='''./.github/workflows/scripts/setup-rust.sh linux x86_64 ""
./mach --no-interactive bootstrap --application-choice browser
'''
    result['Native compile and install tests']='''./mach configure
./mach build --jobs="$(nproc)"
./mach build install-tests
'''
    # Follow normal Linux updater handling; native packaging retains the upstream
    # --error-if-empty checks (normal artifact packaging relaxes them).
    result['Native final package']='''test -f obj-artifact-build-output/dist/bin/updater
patchelf --add-rpath '$ORIGIN' obj-artifact-build-output/dist/bin/updater
./mach package
'''
    ordered=['Apply upstream patches','Apply Floorp common Runtime patches','Native bootstrap',
             'Setup Noraneko','Write Versions','Copy branding files from runtime','Mach configure',
             'Build Noraneko','Native compile and install tests','Inject Noraneko',
             'Apply Firefox Preferences Override','Native final package']
    return [(name,result[name]) for name in ordered]


def write(workflow, output):
    output.mkdir(parents=True,exist_ok=False)
    index=[]
    for n,(name,script) in enumerate(extract(workflow)):
        path=output/f'{n:02d}.sh'
        path.write_text('#!/usr/bin/env bash\nset -euo pipefail\n'+script)
        subprocess.run(['bash','-n',str(path)],check=True)
        index.append({'name':name,'script':path.name,'sha256':hashlib.sha256(path.read_bytes()).hexdigest()})
    (output/'recipe.json').write_text(json.dumps({'source_workflow_sha256':hashlib.sha256(workflow.read_bytes()).hexdigest(),'steps':index},indent=2))
    return index

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--workflow',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();write(a.workflow,a.output)
