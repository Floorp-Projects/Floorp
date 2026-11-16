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


# Files the native harness itself (re)writes inside snapshotted objdir test
# trees during resolution and runs. They are derived outputs, not build
# inputs: Python bytecode caches (only in the _tests trees, where mach loads
# harness modules; harness processes also redirect caches out of the tree with
# PYTHONPYCACHEPREFIX, so an in-tree .pyc is never read), and mochitest's
# generated run manifest and pywebsocket log next to runtests.py
# (testing/mochitest/runtests.py). dist/bin and the product copy are digested
# without any exclusion. Everything else must stay byte-identical.
EPHEMERAL_DIRS=frozenset({'__pycache__'})
EPHEMERAL_SUFFIXES=('.pyc',)
EPHEMERAL_TREE_FILES={'_tests/testing/mochitest':frozenset({'tests.json','websock.log'})}


def digest(path):
    h=hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(1024*1024), b''):h.update(block)
    return h.hexdigest()


def _raise(error):raise error


def _inside(path, roots):
    return any(path==r or r in path.parents for r in roots)


def _walk(root, allowed=(), ephemeral=frozenset(), skip_bytecode=False):
    """Digest a build tree by content.

    Directory symlinks are followed only into `root` or the explicitly trusted
    same-worker `allowed` roots. Mozilla objdirs link test and support
    directories back into the source checkout; such a link's content is
    digested as if it were a real directory, so a dereferencing copy of the
    tree has the same entries. A directory link that leaves the trusted roots
    or loops is rejected with its tree-relative path.

    File symlinks are content-digested wherever they point (dist/bin links
    into the checkout and toolchains); they are not confined to trusted roots.
    The text of every symlink, file or directory, is recorded so a retarget to
    identical content is still detected.

    `ephemeral` names at the tree root, and with `skip_bytecode` Python
    bytecode caches, are skipped as harness-written derived outputs.
    Unreadable directories fail the walk.
    """
    root=Path(root).resolve()
    gate.require(root.is_dir(), 'missing tree: '+str(root))
    roots=[root]+[Path(a).resolve() for a in allowed]
    entries={};links={};chains={str(root):frozenset({root})}
    # Read targets and record bytes; never rewrite links or claim this
    # snapshot can be extracted on another worker.
    for base, dirs, files in os.walk(root, followlinks=True, onerror=_raise):
        chain=chains.pop(base)
        if skip_bytecode:dirs[:]=[d for d in dirs if d not in EPHEMERAL_DIRS]
        for directory in dirs:
            path=Path(base)/directory;rel=str(path.relative_to(root))
            real=path.resolve()
            if path.is_symlink():
                gate.require(_inside(real, roots), 'directory symlink leaves trusted build roots: '+rel+' -> '+os.readlink(path))
                links[rel]=os.readlink(path)
            gate.require(real not in chain, 'directory symlink cycle: '+rel)
            chains[str(path)]=chain|{real}
        for name in files:
            path=Path(base)/name;rel=str(path.relative_to(root))
            if (skip_bytecode and name.endswith(EPHEMERAL_SUFFIXES)) or rel in ephemeral:continue
            # os.walk lists broken links (file or directory) here.
            gate.require(path.is_file(), 'missing/special build material: '+rel)
            if path.is_symlink():links[rel]=os.readlink(path)
            entries[rel]={'sha256':digest(path),'executable':bool(path.stat().st_mode & 0o111)}
    gate.require(entries, 'empty build material tree')
    return entries, links


def tree(root, allowed=(), ephemeral=frozenset(), skip_bytecode=False):
    return _walk(root, allowed, ephemeral, skip_bytecode)[0]


def differences(before, after, limit=20):
    """Human-readable paths that differ between two snapshots."""
    out=[]
    for key in sorted(set(before)|set(after)):
        if before.get(key)==after.get(key):continue
        if key in ('files','source_diffs'):
            a=before.get(key,{});b=after.get(key,{})
            for name in sorted(set(a)|set(b)):
                if a.get(name)!=b.get(name):
                    state='added' if name not in a else 'removed' if name not in b else 'changed'
                    out.append(f'{key}:{name} {state}')
        elif key in ('trees','directory_links'):
            for top in sorted(set(before.get(key,{}))|set(after.get(key,{}))):
                a=before.get(key,{}).get(top,{});b=after.get(key,{}).get(top,{})
                for rel in sorted(set(a)|set(b)):
                    if a.get(rel)!=b.get(rel):
                        state='added' if rel not in a else 'removed' if rel not in b else 'changed'
                        out.append(f'{key}:{top}/{rel} {state}')
        else:out.append(key+' changed')
    return out[:limit]+([f'... {len(out)-limit} more'] if len(out)>limit else [])


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
    trees={};directory_links={}
    for p in REQUIRED_OBJECT_TREES:
        # Same-worker objdir links may point into this worker's Runtime
        # checkout (HEAD SHA and tracked diff recorded; untracked files are
        # not) which contains the Floorp checkout, or the objdir itself.
        trees[str(args.objdir/p)],directory_links[str(args.objdir/p)]=_walk(
            args.objdir/p,(args.runtime,args.objdir),EPHEMERAL_TREE_FILES.get(p,frozenset()),p.startswith('_tests/'))
    trees[str(args.resources)],directory_links[str(args.resources)]=_walk(args.resources)
    return {'schema':'floorp-local-native-cohort/1','paths':paths,'floorp_sha':floorp_sha,
            'runtime_sha':runtime_sha,'source_diffs':{str(root):hashlib.sha256(subprocess.check_output(['git','-C',str(root),'diff','--binary','HEAD','--'])).hexdigest() for root in (args.runtime,args.floorp)},'runner_sha256':digest(RUNNER),'config_sha256':digest(CONFIG),
            'files':files,'trees':trees,'directory_links':directory_links,'provenance':provenance}


def verify(record, floorp_sha, runtime_sha):
    gate.require(record.get('schema')=='floorp-local-native-cohort/1','unsupported local cohort')
    current=snapshot(record['paths'],floorp_sha,runtime_sha)
    gate.require(current==record,'native cohort changed after producer snapshot: '+'; '.join(differences(record,current)))
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
