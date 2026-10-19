# SPDX-License-Identifier: MPL-2.0
"""Exercise the fixed upstream serializer without starting a resource worker."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import stat
import sys
import types

from build_completion import collect, start_snapshot
from qa_core import Failure, require, write_json


RUNTIME_REF = '7dd619742b25567e479eeda97ae071d10542ad0b'
SOURCE_PATH = 'testing/mozbase/mozsystemmonitor/mozsystemmonitor/resourcemonitor.py'
SOURCE_BYTES = 126050
SOURCE_SHA256 = '5f2f9572658fc19d342b5e56904ce110eeb258ff57c4e5d8614a8b0b5b7d1eaf'
PROFILE_NAME = 'profile_log_20261019_000000.json'
FORMATS = ('jsonl', 'whole-profile')
MODES = ('positive', 'no-end', 'truncated')
CODES = ('contract-source-kind', 'contract-source-size', 'contract-source-hash',
         'contract-source-changed', 'contract-task', 'contract-snapshot',
         'contract-monitor-init', 'contract-worker-started', 'contract-observation',
         'contract-cleanup', 'contract-receipt-write')
PHASES = ('source', 'task', 'snapshot', 'serializer', 'observation', 'cleanup', 'receipt')


def safe_error(error, phase):
    return {'phase': phase if phase in PHASES else 'serializer',
            'code': str(error) if isinstance(error, Failure) and str(error) in CODES else 'contract-error',
            'kind': 'failure' if isinstance(error, Failure) else
                    'permission' if isinstance(error, PermissionError) else
                    'os-error' if isinstance(error, OSError) else 'exception',
            'errno': error.errno if isinstance(error, OSError) and type(error.errno) is int else None}


def _identity(value):
    return (value.st_dev, value.st_ino, value.st_uid, value.st_mode, value.st_size,
            value.st_mtime_ns, value.st_ctime_ns, value.st_nlink)


def load_serializer(path):
    flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | getattr(os, 'O_CLOEXEC', 0)
    descriptor = os.open(path, flags)
    with os.fdopen(descriptor, 'rb') as stream:
        before = os.fstat(stream.fileno())
        require(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid() and
                before.st_nlink == 1, 'contract-source-kind')
        require(before.st_size == SOURCE_BYTES and before.st_size <= 1024**2, 'contract-source-size')
        raw = stream.read(1024**2 + 1)
        require(len(raw) == SOURCE_BYTES, 'contract-source-size')
        require(hashlib.sha256(raw).hexdigest() == SOURCE_SHA256, 'contract-source-hash')
        require(_identity(before) == _identity(os.fstat(stream.fileno())) ==
                _identity(os.stat(path, follow_symlinks=False)), 'contract-source-changed')
    module = types.ModuleType('fixed_runtime_profile_serializer')
    exec(compile(raw, '<fixed-runtime-profile-serializer>', 'exec'), module.__dict__)
    return module


class _Clock:
    value = 100.0

    def monotonic(self):
        return self.value

    def time(self):
        return 1800000000.0


def close_monitor(monitor):
    result = {'streamClosed': False, 'parentPipeClosed': False, 'childPipeClosed': False,
              'processClosed': False, 'workerNeverStarted': False, 'failures': []}
    process = getattr(monitor, '_process', None)
    stream = getattr(monitor, '_stream_file', None)
    parent = getattr(monitor, '_pipe', None)
    args = getattr(process, '_args', ())
    child = args[0] if type(args) is tuple and args else None
    result['workerNeverStarted'] = (process is not None and process.pid is None and
                                    getattr(process, '_popen', None) is None)
    for label, resource in (('stream', stream), ('parentPipe', parent), ('childPipe', child)):
        try:
            if resource is not None:
                resource.close()
            result[label + 'Closed'] = resource is not None and resource.closed is True
        except BaseException as error:
            result['failures'].append({'operation': label, **safe_error(error, 'cleanup')})
    try:
        if result['workerNeverStarted']:
            process.close()
            result['processClosed'] = getattr(process, '_closed', False) is True
    except BaseException as error:
        result['failures'].append({'operation': 'process', **safe_error(error, 'cleanup')})
    return result


def _clean(value):
    return type(value) is dict and not value.get('failures') and all(value.get(key) is True for key in
        ('streamClosed', 'parentPipeClosed', 'childPipeClosed', 'processClosed', 'workerNeverStarted'))


def serialize(module, path, profile_format, mode):
    require(profile_format in FORMATS and mode in MODES, 'contract-observation')
    clock = _Clock()
    old_time, old_psutil = module.time, module.psutil
    module.time, module.psutil = clock, module.PsutilStub()
    monitor = module.SystemResourceMonitor.__new__(module.SystemResourceMonitor)
    cleanup, error = None, None
    try:
        monitor.__init__(metadata={})
        process = monitor._process
        require(process is not None, 'contract-monitor-init')
        require(process.pid is None and process._popen is None, 'contract-worker-started')
        monitor.start_streaming(str(path))
        os.fchmod(monitor._stream_file.fileno(), 0o600)
        if mode != 'no-end':
            for phase, start, end in (('compile', 101.0, 140.0), ('export', 102.0, 120.0)):
                clock.value = start
                monitor.begin_phase(phase)
                clock.value = end
                monitor.finish_phase(phase)
        for name, text, start, end in (('SharedLib', 'XUL', 103.0, 135.0),
                                      ('SharedLib', 'XUL', 101.0, 110.0),
                                      ('Program', 'xpcshell', 104.0, 130.0),
                                      ('Program', 'xpcshell', 102.0, 115.0)):
            monitor._add_marker(name, start, None if mode == 'no-end' else end,
                                {'type': 'Text', 'text': text})
        monitor.stop_time = monitor.end_time = 150.0
        clock.value = 150.0
        if profile_format == 'whole-profile':
            profile = monitor.as_profile()
            monitor._stream_file.seek(0)
            monitor._stream_file.truncate()
            json.dump(profile, monitor._stream_file, separators=(',', ':'))
            monitor._stream_file.write('\n')
        monitor._stream_file.flush()
    except BaseException as caught:
        error = caught
    finally:
        cleanup = close_monitor(monitor)
        module.time, module.psutil = old_time, old_psutil
    if error is not None:
        return cleanup, error
    if mode == 'truncated':
        try:
            with path.open('r+b') as stream:
                stream.truncate(path.stat().st_size - 2)
        except BaseException as caught:
            return cleanup, caught
    return cleanup, None


def _number(value):
    return value if type(value) in (int, float) and math.isfinite(value) and 0 <= value < 2**63 else None


def observation_projection(value):
    value = value if type(value) is dict else {}
    last = value.get('lastObservedFinishedTier')
    last = last if type(last) is dict else {}
    targets = value.get('targets')
    targets = targets if type(targets) is dict else {}
    return {'status': value.get('status') if value.get('status') in ('observed', 'unconfirmed') else None,
        'profileFormat': value.get('profileFormat') if value.get('profileFormat') in FORMATS else None,
        'completeCoverage': value.get('completeCoverage') if type(value.get('completeCoverage')) is bool else None,
        'lastFinishedTier': {'tier': last.get('tier') if last.get('tier') in ('compile', 'export') else None,
                             'endTimeMs': _number(last.get('endTimeMs'))},
        'targets': {name: {'status': row.get('status') if row.get('status') in
                           ('observed-finished', 'unconfirmed') else None,
                           'endTimeMs': _number(row.get('endTimeMs'))}
                    for name in ('XUL', 'xpcshell')
                    for row in [targets.get(name) if type(targets.get(name)) is dict else {}]},
        'finishedTargetCounts': {name: value.get('finishedTargetMarkers', {}).get(name)
                                if type(value.get('finishedTargetMarkers')) is dict and
                                type(value['finishedTargetMarkers'].get(name)) is int and
                                value['finishedTargetMarkers'][name] >= 0 else None
                                for name in ('SharedLib', 'Program')},
        'provesBuildCompletion': False, 'provesCacheHit': False}


def satisfied(value, profile_format, mode):
    if mode == 'truncated':
        return (value['status'] == 'unconfirmed' and value['completeCoverage'] is False and
                all(row['status'] == 'unconfirmed' for row in value['targets'].values()) and
                all(count is None for count in value['finishedTargetCounts'].values()))
    if value['status'] != 'observed' or value['profileFormat'] != profile_format or value['completeCoverage'] is not True:
        return False
    if mode == 'no-end':
        return (all(row['status'] == 'unconfirmed' and row['endTimeMs'] is None
                    for row in value['targets'].values()) and
                all(count == 0 for count in value['finishedTargetCounts'].values()) and
                value['lastFinishedTier'] == {'tier': None, 'endTimeMs': None})
    return (value['lastFinishedTier'] == {'tier': 'compile', 'endTimeMs': 40000.0} and
            value['targets'] == {'XUL': {'status': 'observed-finished', 'endTimeMs': 35000.0},
                                 'xpcshell': {'status': 'observed-finished', 'endTimeMs': 30000.0}} and
            value['finishedTargetCounts'] == {'SharedLib': 2, 'Program': 2})


def run_contract(source, task):
    result = {'schema': 1, 'status': 'failed', 'qualification': 'synthetic-serializer-contract-actual-source',
              'runtimeRef': RUNTIME_REF, 'sourceSHA256': SOURCE_SHA256,
              'sourceBytes': SOURCE_BYTES, 'actualSerializerSourceVerified': False,
              'serializerBackend': 'upstream-PsutilStub', 'syntheticClockAndMarkers': True,
              'resourceSamplesCollected': False, 'workerStarted': None, 'realBuildPerformed': False,
              'mozillaQualification': False, 'fullBuildQualification': False,
              'resourceQualification': False, 'bothFormatsObserved': False,
              'negativeInputsDidNotConfirmTargets': False, 'cases': [], 'firstFailure': None,
              'taskCreated': False, 'receiptWriteFailure': None}
    phase = 'source'
    try:
        module = load_serializer(source)
        result['actualSerializerSourceVerified'] = True
        phase = 'task'
        task = Path(task)
        require(task.is_absolute() and task.parent.resolve() == task.parent and
                task.parent.stat().st_uid == os.geteuid() and '..' not in task.parts, 'contract-task')
        task.mkdir(mode=0o700, exist_ok=False)
        result['taskCreated'] = True
        for profile_format in FORMATS:
            for mode in MODES:
                case_task = task / (profile_format + '-' + mode)
                case_task.mkdir(mode=0o700)
                directory = case_task / 'obj/.mozbuild/logs/build'
                directory.mkdir(mode=0o700, parents=True)
                phase = 'snapshot'
                snapshot = start_snapshot(case_task)
                require(snapshot.get('status') == 'captured', 'contract-snapshot')
                path = directory / PROFILE_NAME
                phase = 'serializer'
                cleanup, error = serialize(module, path, profile_format, mode)
                result['workerStarted'] = False if cleanup.get('workerNeverStarted') is True else None
                row = {'format': profile_format, 'control': mode, 'cleanup': cleanup,
                       'observation': None, 'expectationSatisfied': False}
                result['cases'].append(row)
                if error is not None:
                    raise error
                phase = 'cleanup'
                require(_clean(cleanup), 'contract-cleanup')
                phase = 'observation'
                row['observation'] = observation_projection(collect(case_task, snapshot))
                row['expectationSatisfied'] = satisfied(row['observation'], profile_format, mode)
                require(row['expectationSatisfied'], 'contract-observation')
        result.update(status='observed-both-formats', bothFormatsObserved=True,
                      negativeInputsDidNotConfirmTargets=True)
    except BaseException as error:
        result['firstFailure'] = safe_error(error, phase)
    return result


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument('--runtime-profile-source', type=Path, required=True)
    parser.add_argument('--task', type=Path, required=True)
    args = parser.parse_args(argv)
    result = run_contract(args.runtime_profile_source, args.task)
    try:
        require(result['taskCreated'] is True, 'contract-task')
        write_json(args.task / 'build-completion-contract.json', result)
    except BaseException as error:
        result['status'] = 'failed'
        result['receiptWriteFailure'] = safe_error(error, 'receipt')
        if result['firstFailure'] is None:
            result['firstFailure'] = {'phase': 'receipt', 'code': 'contract-receipt-write',
                                      'kind': 'failure', 'errno': None}
    finally:
        print(json.dumps({'buildCompletionContract': result}), flush=True)
    return 0 if result['status'] == 'observed-both-formats' and result['receiptWriteFailure'] is None else 1


if __name__ == '__main__':
    raise SystemExit(main())
