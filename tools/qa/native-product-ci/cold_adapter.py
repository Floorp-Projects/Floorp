#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Bridge cold native execution records into the existing strict pilot gate.
No host-test PASS can be used as native evidence. Does not launch tests itself.
Caller must authenticate inputs/product and own process cleanup before finalizing.
"""
import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import gate

HERE=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('same_worker_adapter',HERE/'ci-adapter.py')
legacy=importlib.util.module_from_spec(spec);spec.loader.exec_module(legacy)


def finalize(plan, execution, product, evidence):
    evidence=Path(evidence).resolve();product=Path(product).resolve()
    gate.require(plan.get('origin')=='authenticated-packaged-native-dumps', 'cold native resolver plan required')
    gate.require(execution.get('schema')=='floorp-cold-native-execution-v1', 'host/synthetic run is not a cold native execution record')
    plan_hash=hashlib.sha256(json.dumps(plan,sort_keys=True,separators=(',',':')).encode()).hexdigest()
    gate.require(execution.get('plan_sha256')==plan_hash, 'execution/plan binding mismatch')
    gate.require(execution.get('product_unchanged') is True and execution.get('support_unchanged') is True,
                 'changed cold product/support')
    rows=[];argv={}
    groups=plan.get('groups',[])
    gate.require(len(groups)==len(gate.PILOT_GROUPS) and {g['id'] for g in groups}==set(gate.PILOT_GROUPS), 'missing cold native groups')
    results=execution.get('results',[])
    gate.require(len(results)==len(groups) and {r['id'] for r in results}==set(gate.PILOT_GROUPS), 'missing/duplicate native results')
    record={'paths':{'binary':str(product/'floorp'),'resources':str(product)}}
    for result in results:
        ident=result['id'];raw=evidence/'native'/(ident+'.jsonl')
        gate.require(raw.is_file() and not raw.is_symlink(), 'missing native raw log')
        if ident in ('p1-sessionstore-unit','p2-contextualidentity-unit'):
            argv[ident]=legacy.audit_xpcshell(raw,record)
        rows.append({'id':ident,'exit_code':result['exit_code'], 'raw_log':raw.relative_to(evidence).as_posix(),
                     'raw_sha256':hashlib.sha256(raw.read_bytes()).hexdigest()})
    report={'schema':1,'finished':execution.get('finished') is True,
            'interrupted':execution.get('interrupted') is not False,
            'manual_cleanup_required':execution.get('owned_cleanup_complete') is not True,
            'plan_sha256':plan_hash,'groups':rows}
    verdict=gate.aggregate(plan,report,evidence,{'execute':{'result':'success' if execution.get('all_processes_finished') is True else 'failure'}},'browser-xpcshell')
    # Only returns after every native raw log/exit/coverage check succeeds.
    return report,argv,verdict


def main():
    p=argparse.ArgumentParser()
    for key in ('plan','execution','product','evidence'):
        p.add_argument('--'+key,type=Path,required=True)
    p.add_argument('--plan-sha256',required=True)
    a=p.parse_args()
    gate.require(hashlib.sha256(a.plan.read_bytes()).hexdigest()==a.plan_sha256, 'pre-resolved native plan bytes changed')
    plan=json.loads(a.plan.read_text());execution=json.loads(a.execution.read_text())
    report,argv,verdict=finalize(plan,execution,a.product,a.evidence)
    for name,value in (('plan',plan),('report',report),('actual-xpcshell-argv',argv),('verdict',verdict)):
        with (a.evidence/(name+'.json')).open('x') as stream:json.dump(value,stream,indent=2)

if __name__=='__main__':main()
