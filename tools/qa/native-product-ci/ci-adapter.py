#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Invoke reviewed v3 runner on an already provisioned SAME-WORKER cohort.
No download, credential forwarding, arbitrary commands, fallback, or synthetic pass.
"""
import argparse
import ast
import shlex
import json
import os
from pathlib import Path
import subprocess
import sys
import gate
import local_cohort as cohort


def clean_environment(env):
    # Allow-list intentionally excludes GitHub/API tokens, cloud credentials,
    # git credential helpers and CI output command channels.
    keep={'PATH','HOME','LANG','LC_ALL','TMPDIR','DISPLAY','XAUTHORITY',
          'XDG_RUNTIME_DIR','DBUS_SESSION_BUS_ADDRESS','MOZBUILD_STATE_PATH'}
    out={k:v for k,v in env.items() if k in keep}
    out['MOZ_DISABLE_NONLOCAL_CONNECTIONS']='1'
    return out


def command(record, logs, scope="full"):
    paths=record['paths']
    cmd=[sys.executable,str(cohort.RUNNER),'--config',str(cohort.CONFIG)]
    for key in ('runtime','objdir','binary','resources','modules','python','xpcshell_python'):
        cmd += ['--'+key.replace('_','-'),paths[key]]
    if scope == 'browser-xpcshell':
        for ident in gate.PILOT_GROUPS:cmd += ['--suite',ident]
    return cmd+['--logs',str(logs)]  # all 8 groups, no dry-run/subset/chunk/retry


def audit_xpcshell(raw, record):
    """Audit native logCommand(name, argv) against each active test identity.

    Runtime159 emits '%s | full command: %r'. Counts alone cannot prove every
    test used this product. Preserve exact native IDs; no basename pairing here.
    """
    expected_binary = str(Path(record['paths']['binary']).resolve())
    expected_resources = str(Path(record['paths']['resources']).resolve())
    active, executed, launches = set(), set(), {}
    for line in raw.read_text().splitlines():
        event = json.loads(line)
        action=event.get('action')
        if action == 'test_start':
            identity=event.get('test')
            gate.require(isinstance(identity,str) and identity not in active and identity not in executed, 'invalid/duplicate xpcshell start')
            active.add(identity)
        elif action == 'test_end':
            identity=event.get('test')
            gate.require(identity in active, 'xpcshell end without start')
            active.remove(identity)
            if event.get('status') != 'SKIP':executed.add(identity)
        elif action == 'log' and isinstance(event.get('message'),str):
            name, separator, encoded = event['message'].partition(' | full command: ')
            if not separator:continue
            gate.require(name in active and name not in launches, 'argv not bound to one active test / duplicate launch')
            try:argv=ast.literal_eval(encoded)
            except (ValueError,SyntaxError) as error:raise ValueError('invalid native argv record') from error
            gate.require(isinstance(argv,list) and argv and all(isinstance(v,str) for v in argv), 'invalid actual xpcshell argv')
            gate.require(argv[0] == expected_binary and '--xpcshell' in argv, 'foreign executable for native test')
            def flag(key):
                gate.require(argv.count(key)==1, 'missing/duplicate actual xpcshell flag: '+key)
                index=argv.index(key)
                gate.require(index+1<len(argv), 'truncated xpcshell argv')
                return argv[index+1]
            gate.require(str(Path(flag('-g')).resolve())==expected_resources, 'xpcshell used foreign XRE')
            gate.require(str(Path(flag('-a')).resolve())==str(Path(expected_resources)/'browser'), 'xpcshell used foreign app resources')
            launches[name]=argv
    gate.require(not active and executed and set(launches)==executed, 'actual xpcshell argv missing for an executed test / unexpected launch')
    return launches


def convert(run, plan, raw_root, evidence):
    required = gate.PILOT_GROUPS if plan.get("scope") == "browser-xpcshell" else gate.GROUPS
    gate.require({x['id'] for x in run['plan']}==set(required) and len(run['plan'])==len(required),'runner did not plan all groups')
    rows=[]
    for result in run['results']:
        ident=result['id']
        gate.require(ident in gate.GROUPS,'foreign runner group')
        path=raw_root/(ident+'.jsonl')
        rows.append({'id':ident,'exit_code':result['exit_code'],
                     'raw_log':str(path.relative_to(evidence)), 'raw_sha256':cohort.digest(path)})
    return {'schema':1,'finished':bool(run.get('finished')),
            'interrupted':bool(run.get('interrupted')),
            'manual_cleanup_required':bool(run.get('manual_cleanup_required')),
            'plan_sha256':cohort.hashlib.sha256(json.dumps(plan,sort_keys=True,separators=(',',':')).encode()).hexdigest(),
            'groups':rows}


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--evidence',type=Path,required=True)
    p.add_argument('--scope',choices=['full','browser-xpcshell'],default='full')
    p.add_argument('--cohort',type=Path,required=True)
    p.add_argument('--plan',type=Path,required=True)
    p.add_argument('--plan-sha256',required=True)
    a=p.parse_args()
    record=json.loads(a.cohort.read_text())
    cohort.verify(record,os.environ['FLOORP_SHA'],os.environ['RUNTIME_SHA'])
    gate.require(gate.sha(a.plan_sha256) and cohort.digest(a.plan)==a.plan_sha256,'pre-resolved plan digest mismatch')
    plan=json.loads(a.plan.read_text())
    gate.require(plan.get('floorp_sha')==record['floorp_sha'] and plan.get('runtime_sha')==record['runtime_sha'],'foreign resolver plan')
    required = gate.PILOT_GROUPS if a.scope == 'browser-xpcshell' else gate.GROUPS
    gate.require(plan.get('scope','full') == a.scope, 'plan/scope mismatch')
    gate.require(plan.get('schema')==1 and len(plan.get('groups',[]))==len(required) and {g['id'] for g in plan['groups']}==set(required),'incomplete native resolver plan')
    for g in plan['groups']:gate.unique(g.get('resolved_test_ids'),'resolved tests')
    a.evidence.mkdir(parents=True,exist_ok=False)
    (a.evidence/'plan.json').write_text(json.dumps(plan))
    (a.evidence/'cohort.json').write_text(json.dumps(record,indent=2))
    raw=a.evidence/'native'
    cmd=command(record,raw,a.scope)
    (a.evidence/'adapter-command.json').write_text(json.dumps(cmd))
    completed=subprocess.run(cmd,env=clean_environment(os.environ))
    try:
        cohort.verify(record,os.environ['FLOORP_SHA'],os.environ['RUNTIME_SHA'])
        (a.evidence/'cohort-post.json').write_text(json.dumps({'unchanged':True}))
    except Exception as error:
        (a.evidence/'cohort-post.json').write_text(json.dumps({'unchanged':False,'error':str(error)}))
        raise
    # Preserve original logs even when conversion/strict full-coverage fails.
    run=json.loads((raw/'run.json').read_text())
    argv_evidence={}
    for ident in ('p1-sessionstore-unit','p2-contextualidentity-unit'):
        logfile=raw/(ident+'.jsonl')
        argv_evidence[ident]=audit_xpcshell(logfile,record)
        expected=next(g['resolved_test_ids'] for g in plan['groups'] if g['id']==ident)
        mode=next(g.get('id_mode','exact') for g in plan['groups'] if g['id']==ident)
        disabled=next(g.get('disabled_test_ids',[]) for g in plan['groups'] if g['id']==ident)
        executed=gate.raw_summary(logfile,set(expected),mode,disabled)
        gate.require(len(argv_evidence[ident])==executed, 'xpcshell launch evidence does not cover every executed test')
    (a.evidence/'actual-xpcshell-argv.json').write_text(json.dumps(argv_evidence,indent=2))
    report=convert(run,plan,raw,a.evidence)
    (a.evidence/'report.json').write_text(json.dumps(report,indent=2))
    gate.aggregate(plan,report,a.evidence,{'execute':{'result':'success' if completed.returncode==0 else 'failure'}},a.scope)
    return completed.returncode

if __name__=='__main__':
    try:sys.exit(main())
    except (OSError,ValueError,KeyError,TypeError,subprocess.CalledProcessError) as error:
        print('Native adapter BLOCKED/FAILED: '+str(error),file=sys.stderr);sys.exit(2)
