# SPDX-License-Identifier: MPL-2.0
"""SDK-free reproduction of the pinned native harness's process-census workload.

This exercises descendant sampling, not browser startup or Shim authentication.
It deliberately keeps the census children in the monitored process group.
"""

import argparse
import importlib.util
import json
import os
from pathlib import Path
import stat
import subprocess
import sys
import time

from support import Failure, Runner, clean_environment, digest, require, write_json


def verify_harness(harness, plan, prepared=False):
    changed = {}
    if prepared:
        pin = plan['harness']['startupPatch']
        changed[pin['target']] = pin['patchedSHA256']
        changed.update({row['target']: row['patchedSHA256'] for row in pin.get('additionalFiles', [])})
        recording = plan['harness'].get('failureDiagnosticsPatch', {})
        changed.update({row['target']: row['patchedSHA256'] for row in recording.get('targets', [])})
    for row in plan['harness']['files']:
        require(digest(harness / row['path']) == changed.get(row['path'], row['sha256']), 'census-harness-binding')
    if prepared and plan['harness']['startupPatch'].get('inventoryHelper'):
        helper = plan['harness']['startupPatch']['inventoryHelper']
        require(digest(harness / helper['target']) == helper['sha256'], 'census-inventory-helper-binding')
    if prepared:
        for row in plan['harness'].get('failureDiagnosticsPatch', {}).get('helpers', []):
            require(digest(harness / row['target']) == row['sha256'], 'census-recording-helper-binding')


def workload(harness, task):
    spec = importlib.util.spec_from_file_location('census_runtime', harness / 'tools/app-shim/test-runtime.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    from native_processes import NativeProcesses
    inventory = NativeProcesses()
    counts = {'hostAll': 0, 'hostSingle': 0, 'shimPIDInventory': 0}
    for _ in range(12):
        module.HostProcess._process_rows(executable=str(Path(sys.executable).resolve()))
        counts['hostAll'] += 1
        module.HostProcess._process_rows(os.getpid())
        counts['hostSingle'] += 1
        require(len(inventory.pids()) <= 4096, 'census-pid-inventory-limit')
        counts['shimPIDInventory'] += 1
        time.sleep(0.25)
    write_json(task / 'census-workload.json', counts)


def record_observation(activity, observations):
    observations['samples'] += 1
    observations['completeSamples'] += int(activity['sampleComplete'])
    snapshots = list(activity.get('partialSamples', []))
    # On terminal incompleteness group_activity already appends the final
    # attempt to partialSamples; a recovered complete attempt is separate.
    if activity['sampleComplete'] or not snapshots:
        snapshots.append(activity)
    for snapshot in snapshots:
        inventory = snapshot.get('inventorySummary', {})
        observations['systemPsObserved'] += inventory.get('roles', {}).get('system-ps', 0)
        observations['differentOwnerObserved'] += inventory.get('differentOwner', 0)
        observations['psTaskDeniedSamples'] += int(any(
            row.get('kind') == 'permission' and row.get('inventory', {}).get('role') == 'system-ps'
            for row in snapshot.get('formatExamples', [])))


def require_observation(observations, platform):
    require(observations['completeSamples'] >= 2, 'census-insufficient-complete-samples')
    if platform == 'darwin':
        require(observations['systemPsObserved'] == 0, 'census-unexpected-system-ps')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--harness', required=True, type=Path)
    parser.add_argument('--task', required=True, type=Path)
    parser.add_argument('--worker', action='store_true')
    parser.add_argument('--prepared', action='store_true')
    args = parser.parse_args()
    plan = json.loads(Path(__file__).with_name('plan.json').read_text())
    verify_harness(args.harness, plan, args.prepared)
    if args.worker:
        workload(args.harness, args.task)
        return 0
    args.task.mkdir(mode=0o700, parents=True, exist_ok=False)
    (args.task / 'tmp').mkdir(mode=0o700)
    runner = Runner(args.task, time.time(), plan['limits'])
    env = clean_environment(args.task)
    result = {'status': 'failed', 'platform': sys.platform,
              'scope': 'PINNED_NATIVE_HARNESS_PROCESS_CENSUS_ONLY',
              'browserStartup': 'NOT_TESTED', 'shimAuthentication': 'NOT_TESTED',
              'harnessCommit': plan['harness']['commit'], 'harnessHashesVerified': True,
              'sdkAcquired': False, 'newArtifacts': 0, 'processSelectionChanged': False}
    observations = {'samples': 0, 'completeSamples': 0, 'systemPsObserved': 0,
                    'differentOwnerObserved': 0, 'psTaskDeniedSamples': 0}
    try:
        from pipeline import prepare_harness
        result['harnessPreparation'] = prepare_harness(plan, args.harness, Path(__file__).resolve().parents[3],
                                                       runner, env)
        metadata = Path('/bin/ps').stat()
        result['systemPsFile'] = {'ownedByRoot': metadata.st_uid == 0,
                                  'setuidBit': bool(metadata.st_mode & stat.S_ISUID)}
        if sys.platform == 'darwin':
            from darwin_activity import probe
            own = probe().read(os.getpid(), os.getpgrp())
            require(own['status'] == 'measured', 'census-self-counter-unavailable')
            result['selfCounter'] = {key: own[key] for key in ('status', 'rssMiB', 'cpuSeconds', 'state')}
            runner.run('census-zombie-preflight', [sys.executable, '-m', 'unittest',
                       'test_guards.DarwinCounterGuards.test_real_mac_unreaped_child_is_gone_while_signal_zero_succeeds'],
                       Path(__file__).parent, env, plan['limits']['jobMinutes'])
            result['nativeZombiePreflight'] = 'passed'
        else:
            result['qualification'] = 'PORTABLE_ONLY_NO_MAC_PROOF'
        sample = runner.group_activity
        def observe(pgid):
            activity = sample(pgid)
            record_observation(activity, observations)
            return activity
        runner.group_activity = observe
        runner.run('harness-census-preflight', [sys.executable, str(Path(__file__).resolve()), '--worker',
                   '--harness', str(args.harness.resolve()), '--task', str(args.task.resolve()), '--prepared'],
                   args.task, env, plan['limits']['jobMinutes'])
        counts = json.loads((args.task / 'census-workload.json').read_text())
        require(counts == {'hostAll': 12, 'hostSingle': 12, 'shimPIDInventory': 12}, 'census-workload-incomplete')
        require_observation(observations, sys.platform)
        result['workload'] = counts
        result['status'] = 'passed'
    except BaseException as error:
        code = str(error) if isinstance(error, Failure) else type(error).__name__
        runner.fail('harness-census-preflight', code)
        result['errorCode'] = code
    finally:
        result.update(observations=observations, stages=runner.records,
                      monitorDiagnostics=runner.monitor_diagnostics, monitorFailure=runner.monitor_failure,
                      firstFailure=runner.failure)
        write_json(args.task / 'census-summary.json', result)
        print(json.dumps({'nativeMonitorPreflight': result}), flush=True)
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
