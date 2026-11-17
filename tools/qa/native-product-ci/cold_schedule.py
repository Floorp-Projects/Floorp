#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Cold packaged native-harness commands and dump-to-plan bridge.
No OBJDIR synthesis, expected-count inference, or QA3 four-case substitution.
Requires prior authenticated/deep producer validation and a verified PR product.
"""
import hashlib
import json
from pathlib import Path
import re
import tomllib
import gate
import resolve_native

PREFIX={'browser':'mochitest/browser/','xpcshell':'xpcshell/tests/'}
EXPECTED_MANIFESTS={
 'p0-tabs':'browser/components/tabbrowser/test/browser/tabs/browser.toml',
 'p1-sessionstore-browser':'browser/components/sessionstore/test/browser.toml',
 'p1-sessionstore-oldformat':'browser/components/sessionstore/test/browser_oldformat.toml',
 'p1-sessionstore-startup-telemetry':'browser/components/sessionstore/test/browser_startup_telemetry.toml',
 'p1-sessionstore-unit':'browser/components/sessionstore/test/unit/xpcshell.toml',
 'p2-contextualidentity-unit':'toolkit/components/contextualidentity/tests/unit/xpcshell.toml',
 'p2-contextualidentity-browser':'toolkit/components/contextualidentity/tests/browser/browser.toml',
}


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def manifests(config, support):
    root=Path(support).resolve(strict=True)
    groups=[s for s in config['suites'] if s['id'] in gate.PILOT_GROUPS]
    gate.require(len(groups)==len(gate.PILOT_GROUPS) and {s['id'] for s in groups}==set(gate.PILOT_GROUPS), 'incomplete cold browser/xpcshell scope')
    result={}
    for suite in groups:
        gate.require(suite['kind'] in PREFIX and not suite.get('test_paths'), 'subset or unsupported cold suite')
        path=suite['path']
        if suite['manifests']:
            gate.require(len(suite['manifests'])==1, 'ambiguous original manifest')
            path=path.rstrip('/')+'/'+suite['manifests'][0]
        gate.require(path==EXPECTED_MANIFESTS[suite['id']], 'suite path differs from approved Firefox manifest')
        gate.require(path.endswith('.toml') and not path.startswith('/') and '..' not in Path(path).parts, 'unsafe manifest identity')
        manifest=root/(PREFIX[suite['kind']]+path)
        gate.require(not manifest.is_symlink() and manifest.is_file() and manifest.resolve().is_relative_to(root), 'packaged native manifest missing/outside support')
        result[suite['id']]={'kind':suite['kind'],'manifest':str(manifest),
                            'manifest_sha256':digest(manifest),'relative_manifest':manifest.relative_to(root).as_posix()}
    return result


def command(group, python, support, product, output, prefs, *, dump=None, mochitest_root=None):
    support=Path(support).resolve();product=Path(product).resolve();output=Path(output).resolve()
    manifest=Path(group['manifest'])
    common=['--manifest',str(manifest),'--xre-path',str(product),'--utility-path',str(support/'bin'),
            '--testing-modules-dir',str(support/'modules'),'--log-raw',str(output/'native.jsonl')]
    if group['kind']=='browser':
        gate.require(mochitest_root is not None, 'mutable mochitest copy required')
        mutable=Path(mochitest_root).resolve()
        gate.require(not mutable.is_relative_to(support), 'mutable harness cannot overwrite frozen support')
        copied=mutable/manifest.relative_to(support/'mochitest')
        gate.require(copied.is_file() and digest(copied)==group['manifest_sha256'], 'mutable manifest copy changed')
        common[1]=str(copied)
        argv=[str(python),'-s',str(Path(__file__).with_name('cold_browser_entry.py')),
              '--flavor','browser','--appname',str(product/'floorp'),'--certificate-path',str(support/'certs'),
              '--extra-profile-file',str(support/'bin/plugins'),'--headless','--timeout','120',
              '--profile-path',str(output/'profile'),*common]
        for key,value in sorted(prefs.items()):argv+=['--setpref',f'{key}={value}']
    elif group['kind']=='xpcshell':
        # --app-binary runs assembled Floorp --xpcshell, not support/bin/xpcshell.
        # Preserve existing wrapper appdir adaptation separately; no test filtering.
        wrapper=Path(group.get('appdir_wrapper',''))
        gate.require(wrapper.is_file() and digest(wrapper)==group.get('appdir_wrapper_sha256'), 'validated xpcshell appdir wrapper required')
        common[1]=str(wrapper)
        argv=[str(python),'-s',str(support/'xpcshell/runxpcshelltests.py'),
              '--app-binary',str(product/'floorp'),'--verbose','--sequential','--no-logfiles',
              '--temp-dir',str(output/'tmp'),'--profile-name','firefox',*common]
    else:raise ValueError('unsupported cold harness')
    if dump is not None:argv+=['--dump-tests',str(dump)]
    return argv


def from_dumps(config, support, dumps, floorp_sha, runtime_sha, *, mochitest_root=None):
    gate.require(all(re.fullmatch('[0-9a-f]{40}',s) for s in (floorp_sha,runtime_sha)), 'missing exact source binding')
    located=manifests(config,support);groups=[]
    for suite in config['suites']:
        ident=suite['id']
        if ident not in located:continue
        path=Path(dumps)/(ident+'.dump.json');data=json.loads(path.read_text())
        names=resolve_native.entries(data)
        # Real dump paths must identify installed files beneath this suite, not
        # merely contain matching basenames from another source/support cohort.
        parent=Path(located[ident]['manifest']).parent.resolve()
        for row in data['active_tests']:
            candidate=Path(row['path']).resolve()
            if suite['kind']=='browser' and mochitest_root is not None:
                mutable=Path(mochitest_root).resolve()
                gate.require(candidate.is_relative_to(mutable), 'browser dump did not use the verified mutable harness')
                frozen=Path(support).resolve()/'mochitest'/candidate.relative_to(mutable)
                gate.require(frozen.is_file() and candidate.is_file() and digest(frozen)==digest(candidate), 'mutable test bytes changed')
                candidate=frozen.resolve()
            gate.require(candidate.is_file() and candidate.is_relative_to(parent), 'dump references foreign/missing native test')
        disabled=[t['path'].rsplit('/',1)[-1] for t in data['active_tests'] if 'disabled' in t]
        gate.require(set(names)-set(disabled), 'all-disabled native suite')
        group={'id':ident,'id_mode':'unique-file-basename','resolved_test_ids':names,
               'disabled_test_ids':disabled,'dump_sha256':digest(path),**located[ident]}
        if 'known_differences' in suite:
            gate.known_differences(suite['known_differences'],set(names),set(disabled))
            group['known_differences']=suite['known_differences']
        groups.append(group)
    return {'schema':1,'scope':'browser-xpcshell','floorp_sha':floorp_sha,'runtime_sha':runtime_sha,
            'origin':'authenticated-packaged-native-dumps','config_sha256':hashlib.sha256(json.dumps(config,sort_keys=True,separators=(',',':')).encode()).hexdigest(),'groups':groups,'excluded':['p1-sessionstore-marionette']}


def prepare_xpcshell_wrapper(group, output):
    from manifestparser import TestManifest
    original=Path(group['manifest'])
    gate.require(digest(original)==group['manifest_sha256'], 'original manifest changed')
    data=tomllib.loads(original.read_text())
    gate.require(data.get('DEFAULT',{}).get('firefox-appdir')=='browser', 'unreviewed native appdir')
    for section,values in data.items():
        gate.require(not section.startswith('include:'), 'nested native appdir include needs audit')
        for name in ('firefox-appdir','floorp-appdir'):
            gate.require(name not in values or values[name]=='browser', 'foreign appdir override')
    output=Path(output)
    with output.open('x') as stream:
        stream.write('[DEFAULT]\nfloorp-appdir = "browser"\n\n['+json.dumps('include:'+str(original))+']\n')
    before=TestManifest([str(original)],strict=True).tests
    after=TestManifest([str(output)],strict=True).tests
    gate.require(before and len(before)==len(after), 'wrapper changes test count')
    labels={'relpath','manifest_relpath','ancestor_manifest','floorp-appdir'}
    for old,new in zip(before,after):
        gate.require(new.get('floorp-appdir')=='browser' and all(old.get(k)==new.get(k) for k in old.keys()|new.keys() if k not in labels), 'wrapper changes native test metadata')
    return {**group,'appdir_wrapper':str(output),'appdir_wrapper_sha256':digest(output)}
