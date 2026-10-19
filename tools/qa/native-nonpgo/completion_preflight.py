# SPDX-License-Identifier: MPL-2.0
"""Build-free Intel validation of the versioned completion and recording contract."""
import argparse
import json
import os
from pathlib import Path
import platform
import sys

from checkpoint import BOUNDARY_CONTRACT
from process_census import BINDING_KEYS, prepare_abi
from support import Failure, Runner, clean_environment, require, write_json
from unittest_receipt import receipt as test_receipt

HERE = Path(__file__).resolve().parent
FAILURE_CODES = {'completion-preflight-platform', 'completion-preflight-contract',
                 'completion-preflight-os', 'completion-preflight-tests',
                 'completion-preflight-cleanup'}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--expected-qa-tests', type=int, required=True)
    parser.add_argument('--expected-recording-tests', type=int, required=True)
    args = parser.parse_args()
    task = args.task.resolve()
    plan = json.loads((HERE / 'plan.json').read_text())
    runner = Runner(task, float((task / 'started').read_text()), plan['limits'])
    env = clean_environment(task)
    env.update(PYTHONDONTWRITEBYTECODE='1', PYTHONWARNINGS='error::ResourceWarning')
    env['PATH'] += ':/usr/local/bin'
    result = {'status': 'failed', 'boundaryContract': BOUNDARY_CONTRACT,
        'checkpointAdmissionChanged': True, 'activeOwnedResourceLimitsChanged': False,
        'postBuildGlobalUIDWaitRemoved': True, 'hostileSameUIDImmutabilityRequired': False,
        'sdkAcquired': False, 'floorpBuildPerformed': False, 'newArtifacts': 0,
        'nativeBrowserAuthenticationTested': False, 'fullBuildQualification': 'NOT_TESTED'}
    stage = 'platform'
    try:
        (task / 'tmp').mkdir(mode=0o700, exist_ok=False)
        require(sys.platform == 'darwin' and platform.machine() == 'x86_64' and
                platform.python_version() == plan['runner']['python'] and
                os.environ.get('ImageVersion') == plan['runner']['imageVersion'],
                'completion-preflight-platform')
        require(plan['checkpoint']['boundaryContract'] == BOUNDARY_CONTRACT and
                0 < args.expected_qa_tests < 1000 and 0 < args.expected_recording_tests < 200,
                'completion-preflight-contract')
        stage = 'abi'
        abi = prepare_abi(task, runner, env)
        os.environ['FLOORP_CENSUS_ABI'] = str(abi)
        env.update({key: os.environ[key] for key in (*BINDING_KEYS, 'TASK_ROOT')})
        env['FLOORP_CENSUS_ABI'] = str(abi)
        stage = 'system-version'
        log = runner.run(stage, ['/usr/bin/sw_vers', '-productVersion'], task, env, 360,
                         phase_deadline_minutes=15)
        require(log.read_text().strip() == plan['runner']['osVersion'], 'completion-preflight-os')
        suites = (
            ('completion-tests', [sys.executable, '-m', 'unittest', '-v', 'test_checkpoint',
                                 'test_process_census', 'test_guards', 'test_shim_failure_patch',
                                 'test_completion_preflight', 'test_unittest_receipt',
                                 'test_qa_core', 'test_darwin_inventory', 'test_inventory_timeout',
                                 'test_helper_import_preflight'],
                                 HERE, args.expected_qa_tests),
            ('failure-recording-tests', [sys.executable, '-m', 'unittest', 'discover', '-v',
                 '-s', str(HERE.parents[1] / 'app-shim'), '-p', 'test_shim_failure_diagnostics.py'],
                 HERE, args.expected_recording_tests))
        for stage, command, cwd, expected in suites:
            try:
                runner.run(stage, command, cwd, env, 360, phase_deadline_minutes=15)
            finally:
                try:
                    result[stage] = test_receipt(task / (stage + '.private.log'),
                                                 recording=stage == 'failure-recording-tests')
                except Exception:
                    result[stage] = {'complete':False, 'status':'unavailable',
                                     'code':'test-receipt-unavailable'}
            tests = result[stage]
            require(tests['complete'] and tests['status'] == 'passed' and
                    tests['total'] == expected and tests['skipped'] == 0,
                    'completion-preflight-tests')
        require(runner.failure is None and runner.monitor_failure is None and not runner.groups and
                all(row['exitCode'] == 0 and row['groupCleanup'] == 'verified-gone' and
                    row['leaderReap'] == {'status':'reaped','exitCode':0} and
                    row['cleanupFailure'] is None for row in runner.records),
                'completion-preflight-cleanup')
        result['status'] = 'passed'
    except Exception as error:
        code = str(error) if isinstance(error, Failure) and str(error) in FAILURE_CODES else \
            'completion-preflight-stage-failure'
        runner.fail(stage, code)
    finally:
        result.update(firstFailure=runner.failure, monitorFailure=runner.monitor_failure,
                      monitorDiagnostics=runner.monitor_diagnostics, stages=runner.records)
        try:
            write_json(task / 'completion-preflight.json', result)
        except OSError:
            runner.fail('evidence-write', 'completion-preflight-evidence-write')
            result.update(status='failed', evidencePersistence=False, firstFailure=runner.failure)
        print(json.dumps({'qaCompletionPreflight': result}), flush=True)
    return 0 if result['status'] == 'passed' and runner.failure is None else 1


if __name__ == '__main__':
    raise SystemExit(main())
