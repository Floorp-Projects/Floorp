#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Snapshot/verify SAME-WORKER native build inputs; not a relocatable OBJDIR archive.
Run after the normal native build, test installation and Floorp product assembly.
This does not certify GitHub producer authenticity or build the browser.
"""
import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import subprocess
from types import SimpleNamespace
import gate

HERE = Path(__file__).resolve().parent
RUNNER = HERE.parent/'native-product-tests/run.py'
CONFIG = RUNNER.with_name('suites.json')
REQUIRED_OBJECT_FILES = ('config.status','.mozconfig','.mozconfig.json','mozinfo.json',
                         'all-tests.pkl','test-defaults.pkl')
REQUIRED_OBJECT_TREES = ('dist/bin','_tests/modules','_tests/testing/mochitest','_tests/xpcshell')
REQUIRED_SOURCE_FILES = ('mach','testing/xpcshell/runxpcshelltests.py',
                         'testing/mochitest/runtests.py','testing/marionette/mach_commands.py')


def digest(path):return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def tree(root):
    root=Path(root).resolve()
    gate.require(root.is_dir(), 'missing tree: '+str(root))
    entries={}
    # Build trees legitimately contain symlinks. Read targets, record bytes, never
    # rewrite links or claim this snapshot can be extracted on another worker.
    for base, dirs, files in os.walk(root, followlinks=False):
        for directory in dirs:
            gate.require(not (Path(base)/directory).is_symlink(), 'directory symlink requires producer audit')
        for name in files:
            path=Path(base)/name
            gate.require(path.is_file(), 'missing/special build material')
            entries[str(path.relative_to(root))]={'sha256':digest(path),'executable':bool(path.stat().st_mode & 0o111)}
    gate.require(entries, 'empty build material tree')
    return entries


def git_sha(root):
    sha=subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip()
    gate.require(gate.sha(sha,40),'invalid checkout SHA')
    return sha


def load_runner():
    spec=importlib.util.spec_from_file_location('floorp_native_runner_v3',RUNNER)
    module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
    return module


def snapshot(paths, floorp_sha, runtime_sha):
    for key in ('runtime','floorp','objdir','binary','resources','modules','python','xpcshell_python'):
        gate.require(key in paths and Path(paths[key]).is_absolute(),'absolute build path required: '+key)
    args=SimpleNamespace(**{k:Path(v) for k,v in paths.items()})
    gate.require(git_sha(args.runtime)==runtime_sha and git_sha(args.floorp)==floorp_sha,'source SHA mismatch')
    config=json.loads(CONFIG.read_text())
    gate.require({s['id'] for s in config['suites']}==set(gate.GROUPS),'runner groups differ')
    provenance=load_runner().preflight(args,config)
    files={}
    for root,names in ((args.objdir,REQUIRED_OBJECT_FILES),(args.runtime,REQUIRED_SOURCE_FILES+gate.MANIFESTS)):
        for name in names:files[str(root/name)]=digest(root/name)
    trees={str(args.objdir/p):tree(args.objdir/p) for p in REQUIRED_OBJECT_TREES}
    trees[str(args.resources)]=tree(args.resources)
    return {'schema':'floorp-local-native-cohort/1','paths':paths,'floorp_sha':floorp_sha,
            'runtime_sha':runtime_sha,'source_diffs':{str(root):hashlib.sha256(subprocess.check_output(['git','-C',str(root),'diff','--binary','HEAD','--'])).hexdigest() for root in (args.runtime,args.floorp)},'runner_sha256':digest(RUNNER),'config_sha256':digest(CONFIG),
            'files':files,'trees':trees,'provenance':provenance}


def verify(record, floorp_sha, runtime_sha):
    gate.require(record.get('schema')=='floorp-local-native-cohort/1','unsupported local cohort')
    current=snapshot(record['paths'],floorp_sha,runtime_sha)
    gate.require(current==record,'native cohort changed after producer snapshot')
    return current


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--paths',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    p.add_argument('--floorp-sha',required=True);p.add_argument('--runtime-sha',required=True)
    a=p.parse_args()
    result=snapshot(json.loads(a.paths.read_text()),a.floorp_sha,a.runtime_sha)
    with a.output.open('x') as f:json.dump(result,f,indent=2)

if __name__=='__main__':main()
