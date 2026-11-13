#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Native --dump-tests pre-execution resolver for browser/xpcshell pilot ONLY."""
import argparse
import json
import os
from pathlib import Path
import subprocess
from types import SimpleNamespace
import gate
import local_cohort as c


def entries(dump):
    tests=dump.get('active_tests')
    gate.require(isinstance(tests,list) and tests,'empty native resolver result')
    # Pinned native browser/xpcshell dump formats have a path per test. Retain
    # original records including disabled predicates, but canonicalize only for
    # raw-label differences caused by the native xpcshell include wrapper.
    names=[]
    for test in tests:
        gate.require(isinstance(test,dict) and isinstance(test.get('path'),str),'unsupported native dump record')
        name=test['path'].rsplit('/',1)[-1]
        gate.require(name.endswith('.js') and name not in names,'ambiguous/non-JS native identity')
        names.append(name)
    gate.unique(names,'native resolved file identities')
    return names


def resolve(record, output):
    gate.require(not output.exists(),'resolver output already exists')
    output.mkdir(parents=True)
    runner=c.load_runner();args=SimpleNamespace(**{k:Path(v) for k,v in record['paths'].items()})
    config=json.loads(c.CONFIG.read_text());runner.preflight(args,config)
    groups=[]
    keep={'PATH','HOME','LANG','LC_ALL','TMPDIR','MOZBUILD_STATE_PATH','DISPLAY','XAUTHORITY'}
    env={k:v for k,v in os.environ.items() if k in keep}
    env.update(MOZCONFIG=str(args.objdir/'.mozconfig'),MOZ_DISABLE_NONLOCAL_CONNECTIONS='1')
    # Native wrappers write only in this fresh directory. Original manifests are
    # untouched; dumps do not execute test bodies, but use the real native loader.
    for suite in config['suites']:
        if suite['id'] not in gate.PILOT_GROUPS:continue
        if suite['kind']=='xpcshell':
            wrapper=runner.xpcshell_wrapper(args,suite,output)
            runner.prepare_xpcshell_wrapper(args,wrapper)
            (output/(suite['id']+'.temp')).mkdir()
        dump=output/(suite['id']+'.dump.json')
        cmd=runner.command(args,config,suite,output)+['--dump-tests',str(dump)]
        with (output/(suite['id']+'.resolver.log')).open('w') as log:
            result=subprocess.run(cmd,cwd=args.runtime,env=env,stdout=log,stderr=subprocess.STDOUT,timeout=300)
        gate.require(result.returncode==0,'native resolver failed: '+suite['id'])
        data=json.loads(dump.read_text())
        groups.append({'id':suite['id'],'id_mode':'unique-file-basename','resolved_test_ids':entries(data),
                       'disabled_test_ids':[t['path'].rsplit('/',1)[-1] for t in data['active_tests'] if 'disabled' in t],
                       'dump_sha256':c.digest(dump),'resolver_command':cmd})
    gate.require(len(groups)==len(gate.PILOT_GROUPS),'incomplete pilot resolution')
    return {'schema':1,'scope':'browser-xpcshell','floorp_sha':record['floorp_sha'],
            'runtime_sha':record['runtime_sha'],'groups':groups,
            'excluded':['p1-sessionstore-marionette']}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--cohort',type=Path,required=True);p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();record=json.loads(a.cohort.read_text())
    c.verify(record,os.environ['FLOORP_SHA'],os.environ['RUNTIME_SHA'])
    plan=resolve(record,a.output/'resolver')
    target=a.output/'plan.json'
    with target.open('x') as f:json.dump(plan,f,indent=2)
    (a.output/'plan.sha256').write_text(c.digest(target)+'\n')

if __name__=='__main__':main()
