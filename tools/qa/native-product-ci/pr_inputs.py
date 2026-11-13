#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Exact same-repository pilot PR, head revision and lock-derived Runtime inputs."""
import hashlib
import json
import os
from pathlib import Path
import re
import subprocess

REPOSITORY='Floorp-Projects/Floorp'
RUNTIME='Floorp-Projects/Floorp-Runtime'
BRANCH='qa/mozilla-native-product-pilot'
PR_NUMBER=3138


def select(event, name, repository, checkout_sha, lock_bytes):
    if name!='pull_request' or repository!=REPOSITORY:raise ValueError('not the authorized PR event/repository')
    if event.get('action') not in {'opened','synchronize','reopened','ready_for_review'}:raise ValueError('unsupported PR action')
    pr=event.get('pull_request',{})
    if event.get('number')!=PR_NUMBER:raise ValueError('not the pilot PR')
    head=pr.get('head',{});base=pr.get('base',{})
    if head.get('repo',{}).get('full_name')!=REPOSITORY or head.get('repo',{}).get('fork') is not False:raise ValueError('fork/foreign source forbidden')
    if base.get('repo',{}).get('full_name')!=REPOSITORY or base.get('ref')!='main':raise ValueError('unexpected target')
    if head.get('ref')!=BRANCH:raise ValueError('unexpected pilot branch')
    floorp=head.get('sha','')
    if not re.fullmatch('[0-9a-f]{40}',floorp) or checkout_sha!=floorp:raise ValueError('checkout must equal exact PR head, not merge SHA')
    lock=json.loads(lock_bytes)
    if lock.get('schemaVersion')!=1:raise ValueError('unsupported lock schema')
    source=lock.get('source',{})
    runtime=source.get('commit','');tree=source.get('tree','')
    if source.get('repository')!=RUNTIME or not re.fullmatch('[0-9a-f]{40}',runtime) or not re.fullmatch('[0-9a-f]{40}',tree):raise ValueError('immutable Runtime source missing')
    artifacts=[a for a in lock.get('artifacts',[]) if a.get('platform')=='linux' and a.get('architecture')=='x86_64']
    if len(artifacts)!=1:raise ValueError('ambiguous/missing Linux Runtime version')
    version=artifacts[0].get('version','')
    if not re.fullmatch('[0-9]+\.[0-9]+(?:\.[0-9]+)?',version):raise ValueError('invalid Runtime version')
    return {'floorp_sha':floorp,'runtime_sha':runtime,'runtime_tree':tree,'runtime_version':version,
            'lock_sha256':hashlib.sha256(lock_bytes).hexdigest()}


def main():
    event=json.loads(Path(os.environ['GITHUB_EVENT_PATH']).read_text())
    head=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
    pins=select(event,os.environ['GITHUB_EVENT_NAME'],os.environ['GITHUB_REPOSITORY'],head,Path('floorp-runtime.lock.json').read_bytes())
    with open(os.environ['GITHUB_OUTPUT'],'a') as f:
        for k,v in pins.items():f.write(k+'='+v+'\n')
    Path('native-pilot-inputs.json').write_text(json.dumps(pins,indent=2))

if __name__=='__main__':main()
