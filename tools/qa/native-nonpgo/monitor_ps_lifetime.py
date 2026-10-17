# SPDX-License-Identifier: MPL-2.0
"""Controlled ps pipe lifetime; this is not a native-browser reproduction."""

import argparse
import ctypes
import errno
import json
import os
from pathlib import Path
import select
import selectors
import signal
import stat
import subprocess
import sys
import time

from support import Failure, Runner, clean_environment, require, write_json


def publish(path, value):
    temporary = path.with_suffix('.tmp')
    write_json(temporary, value)
    temporary.replace(path)


def wait_file(path, seconds=5):
    deadline = time.monotonic() + seconds
    while not path.is_file():
        require(time.monotonic() < deadline, 'ps-handshake-timeout')
        time.sleep(0.01)
    return json.loads(path.read_text())


def ps_command(pid):
    return ['/bin/ps', '-ww', '-p', str(pid), '-o', ','.join(['pid='] * 16384)]


def bounded_drain(child, limits=(2 * 1024**2, 1024**2), capture=None):
    counts = [0, 0]
    deadline = time.monotonic() + 5
    with selectors.DefaultSelector() as selector:
        for index, stream in enumerate((child.stdout, child.stderr)):
            selector.register(stream, selectors.EVENT_READ, index)
        while selector.get_map():
            require(time.monotonic() < deadline, 'ps-drain-timeout')
            for key, _ in selector.select(0.1):
                data = os.read(key.fd, 65536)
                if not data:
                    selector.unregister(key.fileobj)
                    continue
                counts[key.data] += len(data)
                require(counts[key.data] <= limits[key.data], 'ps-output-limit')
                if capture is not None and key.data == 0:
                    capture.append(data)
    child.wait(timeout=5)
    return counts


class DarwinSiginfo(ctypes.Structure):
    _fields_ = [(key, ctypes.c_int) for key in ('signo', 'errno', 'code', 'pid', 'uid', 'status')]
    _fields_ += [('address', ctypes.c_void_p), ('value', ctypes.c_void_p),
                ('band', ctypes.c_long), ('padding', ctypes.c_ulong * 7)]


def child_status(pid):
    if hasattr(os, 'waitid') and hasattr(os, 'WNOWAIT'):
        info = os.waitid(os.P_PID, pid, os.WNOHANG | os.WEXITED | os.WNOWAIT)
        found = info.si_pid if info is not None else 0
        return {'api': 'os.waitid-WNOWAIT', 'exitPending': found != 0,
                'matchingChild': found in (0, pid), 'errno': 0}
    require(sys.platform == 'darwin' and ctypes.sizeof(ctypes.c_void_p) == 8 and
            ctypes.sizeof(DarwinSiginfo) == 104 and DarwinSiginfo.pid.offset == 12 and
            DarwinSiginfo.status.offset == 20, 'ps-waitid-abi')
    lib = ctypes.CDLL('/usr/lib/libSystem.B.dylib', use_errno=True)
    lib.waitid.argtypes = [ctypes.c_int, ctypes.c_uint, ctypes.POINTER(DarwinSiginfo), ctypes.c_int]
    lib.waitid.restype = ctypes.c_int
    info = DarwinSiginfo()
    ctypes.set_errno(0)
    code = lib.waitid(1, pid, ctypes.byref(info), 1 | 4 | 32)
    if code:
        raise OSError(ctypes.get_errno(), 'ps-waitid-unavailable')
    return {'api': 'darwin64-waitid-WNOWAIT', 'exitPending': info.pid != 0,
            'matchingChild': info.pid in (0, pid), 'errno': 0}


def worker(task, mode):
    stopped = False
    def stop(signum, frame):
        nonlocal stopped
        stopped = True
    signal.signal(signal.SIGINT, stop)
    signal.signal(signal.SIGTERM, stop)
    require(signal.getsignal(signal.SIGCHLD) == signal.SIG_DFL, 'ps-child-auto-reap')
    child = subprocess.Popen(ps_command(os.getpid()), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    result = {'reaped': False, 'outputBytes': None, 'stderrBytes': None,
              'heldWithoutReapingWaitOrPoll': mode == 'held'}
    try:
        require(bool(select.select([child.stdout], [], [], 5)[0]), 'ps-output-not-ready')
        if mode == 'held':
            time.sleep(0.1)
        publish(task / 'ready.json', {'pid': child.pid, 'worker': os.getpid(),
                                     'pgid': os.getpgid(child.pid)})
        if mode == 'held':
            deadline = time.monotonic() + 15
            last_sequence = 0
            while not (task / 'release.json').is_file() and not stopped:
                require(time.monotonic() < deadline, 'ps-release-timeout')
                request = task / 'ack-request.json'
                if request.is_file():
                    sequence = json.loads(request.read_text())['sequence']
                    if sequence > last_sequence:
                        try:
                            state = child_status(child.pid)
                        except (OSError, Failure) as error:
                            state = {'errno': getattr(error, 'errno', None), 'exitPending': None,
                                     'matchingChild': False, 'errorType': type(error).__name__}
                        publish(task / f'ack-{sequence}.json', {**state, 'sequence': sequence,
                                'samePopenChild': child.pid, 'noReapingWaitOrPoll': True})
                        last_sequence = sequence
                time.sleep(0.01)
        out_bytes, err_bytes = bounded_drain(child)
        result.update(reaped=True, exitCode=child.returncode,
                      outputBytes=out_bytes, stderrBytes=err_bytes)
        require(child.returncode == 0, 'ps-child-nonzero')
    finally:
        child.stdout.close()
        child.stderr.close()
        if child.returncode is None:
            try:
                child.wait(timeout=5)
                result.update(reaped=True, exitCode=child.returncode)
            except subprocess.TimeoutExpired:
                result['reapError'] = 'TimeoutExpired'
        publish(task / 'drained.json', result)
    deadline = time.monotonic() + (10 if mode == 'held' else 1.25)
    while not stopped and time.monotonic() < deadline:
        time.sleep(0.01)


def bsd_identity(counter, pid, worker_pid, pgid):
    details = {}
    state, value, reason = counter.bsd(pid, details)
    result = {'status': state, 'reason': reason, 'query': details}
    if value is not None:
        result.update(identityTag=counter.tag((value.pid, value.start_sec, value.start_usec)),
                      state=value.status, parentMatchesWorker=value.ppid == worker_pid,
                      groupMatchesTarget=value.pgid == pgid,
                      ownerRelation='same' if value.uid == os.geteuid() else 'different',
                      realOwnerRelation='same' if value.ruid == os.getuid() else 'different')
    return result


def triple_denied(observations):
    return len(observations) == 3 and all(
        row.get('query', {}).get('status') == 'unavailable' and
        row.get('query', {}).get('taskQuery', {}).get('bytes') == 0 and
        row.get('query', {}).get('taskQuery', {}).get('errno') == errno.EPERM
        for row in observations)


def run_case(task, mode, plan):
    from darwin_activity import probe
    counter = probe()
    task.mkdir(mode=0o700)
    (task / 'tmp').mkdir(mode=0o700)
    runner = Runner(task, time.time(), plan['limits'])
    original_read, original_group = counter.read, counter.group
    sample = runner.group_activity
    observations, samples = [], []
    binding = {}
    sequence = 0
    started = time.monotonic()
    def acknowledgment():
        nonlocal sequence
        if mode != 'held':
            return {'status': 'not-held-control'}
        sequence += 1
        publish(task / 'ack-request.json', {'sequence': sequence})
        value = wait_file(task / f'ack-{sequence}.json', 2)
        require(value['sequence'] == sequence, 'ps-ack-sequence-mismatch')
        same = value.pop('samePopenChild') == binding['pid']
        return {**value, 'samePopenChild': same}
    def read(pid, pgid=None, expected=None):
        if pid != binding.get('pid'):
            return original_read(pid, pgid, expected)
        def auxiliary(call):
            try:
                return call()
            except BaseException as error:
                return {'status': 'unavailable', 'errorType': type(error).__name__,
                        'errorCode': str(error) if isinstance(error, Failure) else None}
        row = {'elapsedSeconds': round(time.monotonic() - started, 6), 'pidTag': counter.tag(pid)}
        observations.append(row)
        row['parentBefore'] = auxiliary(acknowledgment)
        row['bsdBefore'] = auxiliary(lambda: bsd_identity(counter, pid, binding['worker'], pgid))
        value = original_read(pid, pgid, expected)
        row['query'] = {k: v for k, v in value.items() if not k.startswith('_')}
        row['bsdAfter'] = auxiliary(lambda: bsd_identity(counter, pid, binding['worker'], pgid))
        row['parentAfter'] = auxiliary(acknowledgment)
        return value
    def group(inventory, pgid):
        value = original_group(inventory, pgid)
        samples.append({k: v for k, v in value.items() if not k.startswith('_')})
        return value
    def observe(pgid):
        if not binding:
            binding.update(wait_file(task / 'ready.json'))
            require(binding['worker'] == pgid and binding['pgid'] == pgid, 'ps-owned-group-mismatch')
            require(os.getpgrp() != pgid, 'ps-monitor-group-overlap')
        try:
            return sample(pgid)
        finally:
            if mode == 'held':
                publish(task / 'release.json', {'release': True})
                wait_file(task / 'drained.json')
    counter.read, counter.group = read, group
    runner.group_activity = observe
    error_code = None
    try:
        runner.run('ps-' + mode, [sys.executable, str(Path(__file__).resolve()),
                   '--worker', mode, '--task', str(task.resolve())], task,
                   clean_environment(task), plan['limits']['jobMinutes'])
    except BaseException as error:
        error_code = str(error) if isinstance(error, Failure) else type(error).__name__
    finally:
        counter.read, counter.group = original_read, original_group
    try:
        drained = wait_file(task / 'drained.json')
    except Failure as error:
        drained = {'reaped': False, 'errorCode': str(error)}
    identities = [item.get('identityTag') for row in observations
                  for item in (row.get('bsdBefore', {}), row.get('bsdAfter', {}))]
    return {'mode': mode, 'synthetic': True, 'errorCode': error_code,
            'pidTag': counter.tag(binding['pid']) if binding else None,
            'workerPidTag': counter.tag(binding['worker']) if binding else None,
            'parentPopenLifetimeBinding': mode == 'held' and drained.get('heldWithoutReapingWaitOrPoll', False),
            'bsdSameIdentityAllObservations': bool(identities) and None not in identities and
                                               len(set(identities)) == 1,
            'sameChildThreeEPERM': triple_denied(observations),
            'observations': observations, 'samples': samples, 'childReap': drained,
            'stages': runner.records, 'monitorFailure': runner.monitor_failure,
            'firstFailure': runner.failure}


def validate_held(case):
    require(case['parentPopenLifetimeBinding'], 'ps-lifetime-binding-missing')
    require(len(case['observations']) == 3 and len(case['samples']) == 3, 'ps-three-observation-coverage')
    require(all(ack.get('samePopenChild') and ack.get('matchingChild') and
                ack.get('noReapingWaitOrPoll') and ack.get('exitPending') is False and ack.get('errno') == 0
                for row in case['observations'] for ack in (row['parentBefore'], row['parentAfter'])),
            'ps-live-owned-identity-coverage')
    for row in case['observations']:
        for value in (row['bsdBefore'], row['bsdAfter']):
            if value.get('identityTag') is not None:
                require(value.get('state') in (1, 2, 3, 4) and value.get('parentMatchesWorker') and
                        value.get('groupMatchesTarget'), 'ps-bsd-identity-conflict')
    known_births = {value['identityTag'] for row in case['observations']
                    for value in (row['bsdBefore'], row['bsdAfter']) if value.get('identityTag') is not None}
    require(len(known_births) <= 1, 'ps-bsd-generation-conflict')
    if case['sameChildThreeEPERM']:
        require(case.get('firstFailure', {}).get('code') == 'monitor-unavailable' and case['monitorFailure'] is not None,
                'ps-fail-closed-missing')
        require(all(not row['sampleComplete'] and row['rssMiB'] is None and row['cpuSeconds'] is None
                    for row in case['samples']), 'ps-unavailable-measurement')
    else:
        require(case['errorCode'] is None, 'ps-unexpected-monitor-failure')
    require(case['childReap']['reaped'] and case['childReap']['exitCode'] == 0 and
            case['childReap']['outputBytes'] >= 65536, 'ps-backpressure-or-reap-coverage')
    require(all(row['groupCleanup'] == 'verified-gone' and row['leaderReap']['status'] == 'reaped'
                for row in case['stages']), 'ps-group-cleanup-incomplete')
    require(case['errorCode'] in (None, 'monitor-unavailable'), 'ps-unexpected-terminal-failure')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', required=True, type=Path)
    parser.add_argument('--harness', type=Path)
    parser.add_argument('--worker', choices=('held', 'draining'))
    args = parser.parse_args()
    if args.worker:
        worker(args.task, args.worker)
        return 0
    require(sys.platform == 'darwin', 'ps-real-mac-required')
    require(args.harness is not None, 'ps-harness-required')
    args.task.mkdir(mode=0o700, parents=True, exist_ok=False)
    plan = json.loads(Path(__file__).with_name('plan.json').read_text())
    result = {'status': 'failed', 'sdkAcquired': False, 'newArtifacts': 0,
              'scope': 'CONTROLLED_PS_LIFETIME_AND_PINNED_HARNESS_CENSUS',
              'nativeAuthentication': 'NOT_TESTED', 'historicalCause': 'UNKNOWN',
              'securityOrPrivilegeChanges': False, 'productionSamplerSourceChanged': False,
              'observationTiming': 'Diagnostic before/after BSD reads and parent WNOWAIT ACKs add timing overhead; retry count and sleep spacing are unchanged.'}
    try:
        metadata = Path('/bin/ps').stat()
        result['systemPsFile'] = {'ownedByRoot': metadata.st_uid == 0,
                                  'setuidBit': bool(metadata.st_mode & stat.S_ISUID)}
        outer = subprocess.Popen(['/bin/ps', '-axo', 'pid=,pgid='],
                                 stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        try:
            capture = []
            out_bytes, err_bytes = bounded_drain(outer, (1024**2, 0), capture)
            require(outer.returncode == 0 and out_bytes > 0 and err_bytes == 0, 'ps-outer-helper-failed')
            rows = [line.split() for line in b''.join(capture).decode('ascii').splitlines()]
            own = [row for row in rows if row and row[0] == str(outer.pid)]
            require(own == [[str(outer.pid), str(os.getpgrp())]], 'ps-outer-helper-group-mismatch')
        finally:
            outer.stdout.close()
            outer.stderr.close()
            try:
                outer.wait(timeout=5)
                result['outerHelperReaped'] = True
            except subprocess.TimeoutExpired:
                result['outerHelperReaped'] = False
                raise Failure('ps-outer-helper-reap-timeout')
        result['outerSamplerGroupInherited'] = True
        result['held'] = run_case(args.task / 'held', 'held', plan)
        try:
            validate_held(result['held'])
        except Failure as error:
            result['heldQualificationError'] = str(error)
        result['draining'] = run_case(args.task / 'draining', 'draining', plan)
        require(result['draining']['errorCode'] is None and result['draining']['childReap']['reaped'],
                'ps-draining-control-failed')
        import monitor_native_preflight
        saved_argv = sys.argv
        try:
            sys.argv = [saved_argv[0], '--task', str(args.task / 'harness'),
                        '--harness', str(args.harness)]
            code = monitor_native_preflight.main()
        finally:
            sys.argv = saved_argv
        result['realHarness'] = json.loads((args.task / 'harness/census-summary.json').read_text())
        require(code == 0, 'ps-real-harness-control-failed')
        require('heldQualificationError' not in result, result.get('heldQualificationError', 'ps-held-failed'))
        result['status'] = 'passed'
    except BaseException as error:
        result['errorCode'] = str(error) if isinstance(error, Failure) else type(error).__name__
    finally:
        write_json(args.task / 'ps-lifetime-summary.json', result)
        print(json.dumps({'psLifetimeDiagnostic': result}), flush=True)
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
