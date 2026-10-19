# SPDX-License-Identifier: MPL-2.0
"""Short observations only: this cannot authorize a checkpoint or change a gate."""
import argparse
import json
import os
from pathlib import Path
import platform
import sys

from process_census import BINDING_KEYS, prepare_abi
from support import Failure, Runner, clean_environment, require, write_json
from unittest_receipt import receipt as test_receipt

HERE = Path(__file__).resolve().parent
RECEIPT_CAP = 1024 * 1024
FAILURE_CODES = {
    'boundary-diagnostic-platform', 'boundary-diagnostic-os',
    'boundary-diagnostic-test-count', 'boundary-diagnostic-tests',
    'boundary-diagnostic-receipt-bound', 'boundary-diagnostic-receipt-schema',
    'boundary-diagnostic-receipt-contract', 'boundary-diagnostic-stage-completion',
}


def collect(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= RECEIPT_CAP,
            'boundary-diagnostic-receipt-bound')
    value = json.loads(path.read_text())
    require(isinstance(value, dict), 'boundary-diagnostic-receipt-schema')
    return value


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--expected-tests', type=int, required=True)
    args = parser.parse_args()
    task = args.task.resolve()
    plan = json.loads((HERE / 'plan.json').read_text())
    runner = Runner(task, float((task / 'started').read_text()), plan['limits'])
    env = clean_environment(task)
    env.update(TMPDIR=str(task / 'tmp'), PYTHONDONTWRITEBYTECODE='1',
               PYTHONWARNINGS='error::ResourceWarning')
    result = {'status': 'failed', 'observationOnly': True,
              'productionGateChanged': False, 'resourceContractChanged': False,
              'checkpointEligibilityEstablished': False, 'newArtifacts': 0,
              'sdkAcquired': False, 'floorpBuildPerformed': False,
              'productQualification': False, 'longExecution': 'ON_HOLD'}
    stage = 'platform'
    try:
        (task / 'tmp').mkdir(mode=0o700, exist_ok=False)
        require(sys.platform == 'darwin' and platform.machine() == 'x86_64' and
                platform.python_version() == plan['runner']['python'] and
                os.environ.get('ImageVersion') == plan['runner']['imageVersion'],
                'boundary-diagnostic-platform')
        require(0 < args.expected_tests < 200, 'boundary-diagnostic-test-count')
        stage = 'abi'
        abi = prepare_abi(task, runner, env)
        os.environ['FLOORP_CENSUS_ABI'] = str(abi)
        env.update({key: os.environ[key] for key in (*BINDING_KEYS, 'TASK_ROOT')})
        env['FLOORP_CENSUS_ABI'] = str(abi)
        stage = 'system-version'
        path = runner.run(stage, ['/usr/bin/sw_vers', '-productVersion'], task, env, 360,
                          phase_deadline_minutes=15)
        require(path.read_text().strip() == plan['runner']['osVersion'],
                'boundary-diagnostic-os')
        stage = 'diagnostic-tests'
        try:
            runner.run(stage, [sys.executable, '-m', 'unittest', '-v',
                       'test_readonly_boundary_experiment', 'test_task_api_experiment',
                       'test_checkpoint', 'test_boundary_diagnostic'],
                       HERE, env, 360, phase_deadline_minutes=15)
        finally:
            result['tests'] = test_receipt(task / (stage + '.private.log'))
        test = result['tests']
        require(test['complete'] and test['status'] == 'passed' and
                test['total'] == args.expected_tests and test['skipped'] == 0,
                'boundary-diagnostic-tests')
        for stage, script, key, receipt in (
                ('task-api-observation', 'task_api_experiment.py', 'taskAPI', 'task-api-experiment.json'),
                ('readonly-boundary-observation', 'readonly_boundary_experiment.py',
                 'readonlyBoundary', 'readonly-boundary-experiment.json')):
            try:
                runner.run(stage, [sys.executable, str(HERE / script), '--task', str(task)],
                           task, env, 360, phase_deadline_minutes=15)
            finally:
                path = task / receipt
                if path.is_file():
                    result[key] = collect(path)
            value = result.get(key, {})
            common = (value.get('status') == 'completed' and
                      value.get('productionGateChanged') is False and value.get('firstFailure') is None)
            if key == 'taskAPI':
                contract = (value.get('observationOnly') is True and
                            value.get('checkpointEligibilityNotEvaluated') is True and
                            value.get('actualDarwinExecution') is True and
                            value.get('experimentCompleted') is True and
                            value.get('comparisonFallbackAuthority') is False and
                            value.get('resourceLimitsChanged') is False and
                            value.get('repairEstablished') is False and
                            value.get('firstFailure') is None)
            else:
                contract = (value.get('diagnosticProcedure') == 'completed' and
                            value.get('qualification') == 'NO_GO' and
                            value.get('productionGateReplacement') is False and
                            value.get('checkpointEligibilityEstablished') is False and
                            value.get('sameUIDImmutabilityEstablished') is False and
                            value.get('newArtifactsStored') == 0)
            require(common and contract, 'boundary-diagnostic-receipt-contract')
        require(runner.failure is None and runner.monitor_failure is None and
                not runner.groups, 'boundary-diagnostic-stage-completion')
        result['status'] = 'passed'
    except Exception as error:
        # Runner retains the first stage/monitor failure. Unbounded exception text
        # from utilities, paths or native libraries is never copied into evidence.
        runner.fail(stage, str(error) if isinstance(error, Failure) and str(error) in FAILURE_CODES
                    else 'boundary-diagnostic-stage-failed')
    finally:
        result.update(firstFailure=runner.failure, monitorFailure=runner.monitor_failure,
                      monitorDiagnostics=runner.monitor_diagnostics, stages=runner.records,
                      elapsedMinutes=round(runner.elapsed() / 60, 3))
        path = task / 'boundary-diagnostic.json'
        try:
            write_json(path, result)
            require(path.stat().st_size <= RECEIPT_CAP, 'boundary-diagnostic-receipt-bound')
        except Exception:
            result['status'] = 'failed'
            result['evidenceWriteFailure'] = True
            if result['firstFailure'] is None:
                result['firstFailure'] = {'stage': 'evidence-write',
                    'code': 'boundary-diagnostic-evidence-write-failed',
                    'elapsedSeconds': round(runner.elapsed(), 2)}
        finally:
            print(json.dumps({'boundaryDiagnostic': result}), flush=True)
    return 0 if result['status'] == 'passed' and runner.failure is None else 1


if __name__ == '__main__':
    raise SystemExit(main())
