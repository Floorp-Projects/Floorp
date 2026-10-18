# SPDX-License-Identifier: MPL-2.0
"""One short native observation experiment; no SDK, build or artifact upload."""
import argparse
import json
import os
from pathlib import Path
import platform
import shutil
import subprocess
import sys
import time

from checkpoint import owned_processes, verify_owned_boundary
from census_diagnostics import describe
from darwin_activity import probe
from process_census import BINDING_KEYS, prepare_abi
from support import Failure, Runner, clean_environment, require, write_json
from unittest_receipt import receipt as unittest_receipt

HERE = Path(__file__).resolve().parent


def cleanup_child(child):
    value={'leaderReaped':False,'exitCode':None,'groupCleanup':'unverified','cleanupFailure':None}
    operation='poll'
    try:
        if child.poll() is None:
            operation='terminate'
            child.terminate()
        operation='wait'
        try:
            child.wait(timeout=5)
        except subprocess.TimeoutExpired:
            operation='kill'
            child.kill()
            operation='wait-after-kill'
            child.wait(timeout=5)
        operation='group-probe'
        if Runner.group_alive(child.pid):
            value['cleanupFailure']={'operation':operation,'kind':'group-still-present'}
        else:
            value['groupCleanup']='verified-gone'
    except BaseException as error:
        kind=next((name for cls,name in ((PermissionError,'permission'),
                  (subprocess.TimeoutExpired,'timeout'),(ProcessLookupError,'process-lookup'),
                  (OSError,'os-error')) if isinstance(error,cls)),'exception')
        value['cleanupFailure']={'operation':operation,'kind':kind}
    value.update(leaderReaped=child.returncode is not None,exitCode=child.returncode)
    return value


def finish_result(result,runner,task,child):
    try:
        if child is not None:
            cleanup=result['failureChildCleanup']=cleanup_child(child)
            if (not cleanup['leaderReaped'] or cleanup['groupCleanup']!='verified-gone' or
                    cleanup['cleanupFailure'] is not None):
                result['status']='failed'
                runner.fail('known-child-cleanup','census-experiment-child-cleanup')
    finally:
        result.update(firstFailure=runner.failure,monitorFailure=runner.monitor_failure,stages=runner.records,
                      elapsedMinutes=round(runner.elapsed()/60,3))
        try:
            write_json(task/'census-experiment.json',result)
        except OSError:
            result['status']='failed'
            result['evidenceWriteError']=True
        finally:
            print(json.dumps({'censusExperiment':result}),flush=True)


def take_owned(value):
    value.update(status='collecting',candidateIdentities=None)
    try:
        result=owned_processes(value)
        value['status']='passed'
        return result
    except BaseException:
        value['status']='unavailable'
        raise


def observe(baseline, observed, value=None):
    if value is None:
        value = {}
    try:
        verify_owned_boundary(baseline, value, observed)
    except Failure as error:
        if str(error) != 'checkpoint-owned-survivors':
            raise
    return value


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--task',type=Path,required=True)
    parser.add_argument('--expected-tests',type=int,required=True)
    parser.add_argument('--original-parent',action='store_true')
    parser.add_argument('--inventory-stress',action='store_true')
    parser.add_argument('--native-selection-repeats',type=int,default=0)
    parser.add_argument('--verify-sdk',action='store_true')
    args=parser.parse_args()
    require(0<=args.native_selection_repeats<=10,'native-selection-repeat-bound')
    task=args.task
    plan=json.loads((HERE/'plan.json').read_text())
    runner=Runner(task,float((task/'started').read_text()),plan['limits'])
    env=clean_environment(task)
    (task/'monitor-tmp').mkdir(mode=0o700)
    env.update(TMPDIR=str(task/'monitor-tmp'),PYTHONDONTWRITEBYTECODE='1')
    result={'status':'failed','observationOnly':True,'checkpointEligibilityNotEvaluated':True,
            'newArtifacts':0,'sdkAcquisitionRequested':args.verify_sdk,
            'sdkAcquired':None if args.verify_sdk else False,'floorpBuildPerformed':False,
            'productQualification':False,'background':[]}
    child=None
    stage='platform'
    try:
        require(sys.platform=='darwin' and platform.machine()=='x86_64' and
                platform.python_version()==plan['runner']['python'] and
                os.environ.get('ImageVersion')==plan['runner']['imageVersion'],'census-experiment-platform')
        stage='abi'
        abi=prepare_abi(task,runner,env)
        os.environ['FLOORP_CENSUS_ABI']=str(abi)
        env.update({key:os.environ[key] for key in (*BINDING_KEYS,'TASK_ROOT')})
        env['FLOORP_CENSUS_ABI']=str(abi)
        version=runner.run('system-version',['/usr/bin/sw_vers','-productVersion'],task,env,360,phase_deadline_minutes=15)
        require(version.read_text().strip()==plan['runner']['osVersion'],'census-experiment-os')
        if args.verify_sdk:
            stage='sdk-verification'
            acquired=json.loads((task/'sdk-download/receipt.json').read_text())
            pin=plan['sdk']
            require(acquired['artifact']==pin['artifact'] and acquired['run']==pin['run'] and
                    acquired['attempt']==pin['attempt'] and acquired['zipSHA256']==pin['zipSHA256'] and
                    acquired['tarSHA256']==pin['tarSHA256'] and acquired['bytes']==pin['tarBytes'] and
                    acquired['crcVerified'] is True,'census-experiment-sdk-acquisition')
            result['sdkAcquired']=True
            runner.run('sdk-materialize',[sys.executable,str(HERE/'materialize.py'),'sdk','--task',str(task)],
                       task,env,360,phase_deadline_minutes=15)
            materialized=json.loads((task/'sdk-materialized.json').read_text())
            require(materialized['canonicalSHA256']==pin['draftReaderTreeSHA256'],
                    'census-experiment-sdk-tree')
            result['sdkVerification']={'status':'passed','artifact':pin['artifact'],'run':pin['run'],
                'zipSHA256':pin['zipSHA256'],'tarSHA256':pin['tarSHA256'],
                'treeSHA256':materialized['canonicalSHA256'],'members':materialized['members'],
                'logicalBytes':materialized['logicalBytes'],'version':pin['version']}
        stage='tests'
        try:
            runner.run('checkpoint-tests',[sys.executable,'-m','unittest','discover','-v'],HERE,env,360,phase_deadline_minutes=15)
        finally:
            result['tests']=unittest_receipt(task/'checkpoint-tests.private.log')
        require(result['tests']['complete'] and result['tests']['status']=='passed' and
                result['tests']['total']==args.expected_tests and result['tests']['skipped']==0,'checkpoint-mac-tests')
        if args.native_selection_repeats:
            result['nativeSelectionRepeats']=[]
            for index in range(args.native_selection_repeats):
                stage='native-selection-repeat-'+str(index+1)
                try:
                    runner.run(stage,[sys.executable,'-m','unittest','-v',
                        'test_native_processes.NativeInventoryGuards.test_real_mac_child_identity_and_argc_without_environment_or_ps'],
                        HERE,env,360,phase_deadline_minutes=15)
                finally:
                    test=unittest_receipt(task/(stage+'.private.log'))
                    result['nativeSelectionRepeats'].append(test)
                require(test['complete'] and test['status']=='passed' and test['total']==1 and test['skipped']==0 and
                        len(test['nativeSelectionDiagnostics'])==1 and
                        test['nativeSelectionDiagnostics'][0]['status']=='passed','native-selection-repeat-failed')
        if args.inventory_stress:
            stage='inventory-stress'
            from inventory_stress import run_experiment
            result['inventoryStress']={}
            run_experiment(task,runner,env,result['inventoryStress'])
        stage='baseline'
        baseline_details=result['baseline']={}
        baseline=take_owned(baseline_details)
        if args.original_parent:
            stage='original-parent-control'
            from origin_control import run_control
            result['originalParentControl']={}
            run_control(task,result['originalParentControl'])
            stage='initial-metadata-origin'
            from darwin_origin import describe_origin
            roles=[(row,describe(row,baseline,baseline,set())) for row in sorted(baseline)]
            metadata=[row for row,hint in roles if hint.get('role')=='metadata-worker']
            result['initialMetadataOrigins']={'candidateIdentities':len(baseline),
                'roleVerifiedMetadataCandidates':len(metadata),
                'roleUnavailableCandidates':sum(hint['status']!='identity-verified' for _,hint in roles),
                'truncated':len(metadata)>16,
                'records':[describe_origin(row,set()) for row in metadata[:16]],
                'exclusionAuthorized':False}
        stage='known-child'
        ready=task/'known-child-ready'
        script=('import os,pathlib,time; '
                'pathlib.Path(os.environ["TASK_ROOT"],"known-child-ready").write_text("ready\\n"); time.sleep(30)')
        child=subprocess.Popen([sys.executable,'-c',script],env=env,
                               stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,start_new_session=True)
        ready_deadline=time.monotonic()+5
        while not ready.is_file() and child.poll() is None and time.monotonic()<ready_deadline:
            time.sleep(0.05)
        require(ready.is_file() and child.poll() is None,'census-experiment-child-ready')
        during_details={}
        result['knownChild']={'snapshot':during_details}
        during=take_owned(during_details)
        known=next((row for row in during-baseline if row[0]==child.pid),None)
        require(known is not None,'census-experiment-child-not-observed')
        runner.observed_processes=set()
        activity=runner.group_activity(child.pid)
        runner.observe_processes(activity)
        require(activity['sampleComplete'] and known[:3] in runner.observed_processes,
                'census-experiment-child-sampler')
        known_hint=describe(known,during,baseline,runner.observed_processes)
        result['knownChild'].update(hint=known_hint,
                                  sampler={key:value for key,value in activity.items() if not key.startswith('_')})
        require(known_hint['status']=='identity-verified' and known_hint['parentRelation']=='caller' and
                known_hint['role']=='python','census-experiment-child-binding')
        result['knownChild']['boundaryObservation']={}
        observe(baseline,runner.observed_processes,result['knownChild']['boundaryObservation'])
        require(result['knownChild']['boundaryObservation']['status']=='rejected' and
                result['knownChild']['boundaryObservation']['newCandidateIdentities']>=1,
                'census-experiment-child-rejection')
        result['knownChild']['cleanup']=cleanup_child(child)
        require(result['knownChild']['cleanup']['leaderReaped'] and
                result['knownChild']['cleanup']['groupCleanup']=='verified-gone' and
                result['knownChild']['cleanup']['cleanupFailure'] is None,'census-experiment-child-cleanup')
        child=None
        stage='background'
        began=time.monotonic()
        for index in range(13):
            require(runner.elapsed()<15*60,'census-experiment-deadline')
            require(shutil.disk_usage(task).free>=plan['limits']['minFreeDuringBuildGiB']*1024**3,
                    'census-experiment-free-disk')
            own=probe().read(os.getpid(),os.getpgrp())
            require(own['status']=='measured' and own['rssMiB']<=plan['limits']['maxOwnedRSSMiB'],
                    'census-experiment-self-monitor')
            value={'index':index,'elapsedSeconds':round(time.monotonic()-began,3)}
            result['background'].append(value)
            observe(baseline,runner.observed_processes,value)
            print(json.dumps({'censusBackground':value}),flush=True)
            if index<12:
                time.sleep(10)
        result['status']='passed'
    except BaseException as error:
        runner.fail(stage,str(error) if isinstance(error,Failure) else type(error).__name__)
    finally:
        finish_result(result,runner,task,child)
    return 0 if result['status']=='passed' and runner.failure is None else 1


if __name__=='__main__':
    raise SystemExit(main())
