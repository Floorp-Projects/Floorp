# SPDX-License-Identifier: MPL-2.0
"""Trace libproc on exact pipe-controlled children; never changes census gates.

PROC_PIDTASKINFO (4) is comparison evidence only. A completed experiment does
not establish a repair, a writer boundary, or reproduction of a past failure.
Private process identifiers and exception text never enter the receipt.
"""
import argparse
from collections import deque
import ctypes
import errno
import json
import os
from pathlib import Path
import select
import subprocess
import sys
import time

from census_diagnostics import identity
from darwin_activity import BSDInfo, DarwinActivity, TaskAllInfo, TaskInfo
from darwin_origin import UniqueInfo, key as unique_key
from support import Failure, clean_environment, write_json


MAX_CHILDREN = 32
MAX_SAMPLES = 256
MAX_QUERIES = 4096
WORK_SECONDS = 48
FLAVORS = {2: (TaskAllInfo, 'task-all-info'), 3: (BSDInfo, 'bsd-info'),
           4: (TaskInfo, 'task-info-comparison'), 17: (UniqueInfo, 'unique-info')}
PHASES = {'platform', 'spawn', 'ready', 'binding', 'held-live', 'exit-window',
          'zombie-wait', 'unreaped-zombie', 'cleanup', 'complete'}
REASONS = {'permission', 'short-task-info', 'invalid-identity', 'bsd-zombie',
           'generation-replaced', 'membership-changed', 'invalid-bsd-state',
           'invalid-task-counters', 'signal-zero-esrch', 'short-bsd-info',
           'generation-changed-during-recheck', 'measured'}
CHILD = r'''
import os,select,sys,time
control,ready=map(int,sys.argv[1:3])
os.write(ready,b'R'); os.close(ready)
deadline=time.monotonic()+12
while time.monotonic()<deadline:
    readable,_,_=select.select([control],[],[],min(.1,max(0,deadline-time.monotonic())))
    if readable and os.read(control,1)==b'':
        os.close(control); os._exit(0)
os._exit(71)
'''


class ProbeFailure(Failure):
    """Only module-owned fixed codes may be exposed as a primary failure."""


def ensure(condition, code):
    if not condition:
        raise ProbeFailure('task-api-' + code)


def error_kind(error):
    return ('permission' if isinstance(error, PermissionError) else
            'timeout' if isinstance(error, subprocess.TimeoutExpired) else
            'process-lookup' if isinstance(error, ProcessLookupError) else
            'os-error' if isinstance(error, OSError) else 'exception')


def error_errno(error):
    code = getattr(error, 'errno', None)
    return code if type(code) is int and 0 <= code <= 4096 else None


class TraceLog:
    def __init__(self, clock=None):
        self.clock = clock or time.monotonic
        self.started = self.clock()
        self.phase = 'platform'
        self.child = None
        self.sample = None
        self.count = 0
        self.recent = deque(maxlen=16)
        self.stop = None
        self.active_sample = None
        self.counts = {label: {'calls': 0, 'full': 0, 'zero': 0, 'short': 0,
                              'other': 0, 'exceptions': 0, 'errnoCounts': {}}
                       for _, label in FLAVORS.values()}

    def elapsed(self):
        return round(self.clock() - self.started, 6)

    def check_deadline(self):
        ensure(self.clock() - self.started < WORK_SECONDS, 'deadline')

    def begin(self, flavor, size):
        ensure(self.phase in PHASES, 'trace-phase')
        ensure(self.count < MAX_QUERIES, 'query-cap')
        self.check_deadline()
        self.count += 1
        row = {'api': 'proc_pidinfo', 'flavor': flavor, 'arg': 1 if flavor == 3 else 0,
               'bufferSize': size, 'returnBytes': None, 'errno': None,
               'order': self.count, 'startedSeconds': self.elapsed(),
               'finishedSeconds': None, 'phase': self.phase,
               'childIndex': self.child, 'sampleIndex': self.sample,
               'exceptionKind': None}
        self.recent.append(row)
        self.counts[FLAVORS[flavor][1]]['calls'] += 1
        return row

    def end(self, row, size=None, code=None, error=None):
        row['finishedSeconds'] = self.elapsed()
        tally = self.counts[FLAVORS[row['flavor']][1]]
        if error is not None:
            row.update(exceptionKind=error_kind(error), errno=error_errno(error))
            tally['exceptions'] += 1
            self.stop = dict(row)
        else:
            row.update(returnBytes=size, errno=code)
            category = ('full' if size == row['bufferSize'] else 'zero' if size == 0 else
                        'short' if 0 < size < row['bufferSize'] else 'other')
            tally[category] += 1
        if row['errno'] is not None:
            label = str(row['errno'])
            tally['errnoCounts'][label] = tally['errnoCounts'].get(label, 0) + 1

    def summary(self):
        return {'totalQueries': self.count, 'queryCap': MAX_QUERIES,
                'countsByFlavor': self.counts, 'recent16': list(self.recent),
                'stoppingQuery': self.stop,
                'elapsedSeconds': self.elapsed()}


class TracedActivity(DarwinActivity):
    """Use production query/read unchanged, with one exact unreaped PID scope."""
    def __init__(self, pid, log, **kwargs):
        self.allowed_pid, self.trace = pid, log
        self.reaped = False
        super().__init__(**kwargs)

    def query(self, pid, flavor, value):
        ensure(not self.reaped and type(pid) is int and pid == self.allowed_pid,
               'query-ownership')
        ensure(flavor in FLAVORS and type(value) is FLAVORS[flavor][0], 'query-shape')
        row = self.trace.begin(flavor, ctypes.sizeof(value))
        try:
            size, code = super().query(pid, flavor, value)
        except BaseException as error:
            self.trace.end(row, error=error)
            raise
        self.trace.end(row, size, code)
        return size, code


def raw_bsd(reader):
    value = BSDInfo()
    size, code = reader.query(reader.allowed_pid, 3, value)
    valid = (size == 136 and code == 0 and value.pid == reader.allowed_pid and
             value.start_sec > 0 and value.start_usec < 1000000 and
             value.status in (1, 2, 3, 4, 5))
    return value if valid else None


def raw_unique(reader):
    value = UniqueInfo()
    size, code = reader.query(reader.allowed_pid, 17, value)
    valid = (size == 56 and code == 0 and value.uniqueid != 0 and any(value.uuid) and
             value.reserved2 == value.reserved3 == 0)
    return value if valid else None


def bsd_projection(value, expected):
    matches = identity(value) == expected if value is not None else None
    return {'sameSpawnFiveTuple': matches,
            'status': int(value.status) if matches else None,
            'inExit': bool(value.flags & 4) if matches else None,
            'sameOwner': (value.uid == os.geteuid() and value.ruid == os.getuid())
                         if matches else None,
            'sameMonitoredGroup': value.pgid == os.getpgrp() if matches else None,
            'directChildParent': value.ppid == os.getpid() if matches else None}


def query_projection(value):
    if not isinstance(value, dict):
        return None
    return {key: value.get(key) if type(value.get(key)) is int else None
            for key in ('bytes', 'expectedBytes', 'errno')}


def production_projection(value, expected):
    result = {'status': value.get('status') if value.get('status') in
              ('measured', 'gone', 'unavailable') else 'unavailable',
              'reason': value.get('reason') if value.get('reason') in REASONS else None,
              'taskQuery': query_projection(value.get('taskQuery')),
              'bsdQuery': query_projection(value.get('bsdQuery')),
              'sameSpawnBirth': value['_birth'] == expected[:3] if '_birth' in value else None,
              'fallbackReason': value.get('fallbackReason') if value.get('fallbackReason') in
                                REASONS else None,
              'esrchRecheck': None}
    source = value.get('esrchRecheck')
    if isinstance(source, dict):
        result['esrchRecheck'] = {
            key: query_projection(source.get(key))
            for key in ('firstTaskQuery', 'firstBSDQuery', 'taskQuery', 'bsdQuery')}
        result['esrchRecheck'].update(
            sameGeneration=source.get('sameGeneration') if type(source.get('sameGeneration')) is bool else None,
            outcome=source.get('outcome') if source.get('outcome') in
                    ('measured', 'gone', 'unavailable') else 'unavailable',
            reason=source.get('reason') if source.get('reason') in REASONS else None)
    return result


def snapshot(reader, expected, anchor, sample_index):
    log = reader.trace
    log.sample = sample_index
    sample = {'sampleIndex': sample_index, 'childIndex': log.child,
              'phase': log.phase, 'startedSeconds': log.elapsed(), 'completed': False,
              'beforeBSD': None, 'afterBSD': None, 'uniqueBeforeAvailable': None,
              'uniqueAfterAvailable': None, 'production': None, 'taskInfoComparison': None}
    log.active_sample = sample
    before = raw_bsd(reader)
    sample['beforeBSD'] = bsd_projection(before, expected)
    first = raw_unique(reader)
    sample['uniqueBeforeAvailable'] = first is not None
    production = reader.read(reader.allowed_pid, os.getpgrp(),
                             {'pid': expected[0], 'startSec': expected[1],
                              'startUsec': expected[2]})
    projected = sample['production'] = production_projection(production, expected)
    task = TaskInfo()
    size, code = reader.query(reader.allowed_pid, 4, task)
    sample['taskInfoComparison'] = {'query': {'bytes': size, 'expectedBytes': 96, 'errno': code},
                                    'fullRecord': size == 96 and code == 0,
                                    'measurementAuthority': False}
    second = raw_unique(reader)
    sample['uniqueAfterAvailable'] = second is not None
    after = raw_bsd(reader)
    sample['afterBSD'] = bsd_projection(after, expected)
    before_after = (identity(before) == identity(after)
                    if before is not None and after is not None else None)
    exec_same = (unique_key(first) == unique_key(second)
                 if first is not None and second is not None else None)
    exec_anchor = (unique_key(first) == anchor == unique_key(second)
                   if first is not None and second is not None else None)
    sample.update(elapsedSeconds=log.elapsed(),
              fiveTupleBeforeAfterMatches=before_after,
              uniqueExecBeforeAfterMatches=exec_same,
              uniqueExecMatchesSpawnAnchor=exec_anchor)
    for value in (before, after):
        ensure(value is None or identity(value) == expected, 'generation-changed')
    task_query = projected['taskQuery'] or {}
    same_live = (before_after is True and sample['beforeBSD']['sameSpawnFiveTuple'] is True and
                 sample['beforeBSD']['status'] in (1, 2, 3, 4) and
                 sample['afterBSD']['status'] in (1, 2, 3, 4))
    sample['sameGenerationLiveTaskAllESRCH'] = bool(
        same_live and projected['status'] == 'unavailable' and
        projected['reason'] == 'short-task-info' and projected['fallbackReason'] == 'measured' and
        projected['sameSpawnBirth'] is True and
        task_query.get('bytes') == 0 and task_query.get('errno') == errno.ESRCH)
    sample['execBoundSameGenerationLiveTaskAllESRCH'] = bool(
        sample['sameGenerationLiveTaskAllESRCH'] and exec_anchor is True)
    sample['queryTrace'] = [dict(row) for row in log.recent
                            if row['sampleIndex'] == sample_index]
    sample['completed'] = True
    log.active_sample = None
    return sample


def retain_sample(result, sample):
    ensure(result['sampleCount'] < MAX_SAMPLES, 'sample-cap')
    result['sampleCount'] += 1
    recent = result['recent16Samples']
    recent.append(sample)
    del recent[:-16]
    phase = sample['phase']
    counts = result['sampleCountsByPhase']
    counts[phase] = counts.get(phase, 0) + 1
    if phase == 'held-live' and result['firstHeldLive'] is None:
        result['firstHeldLive'] = sample
    if phase == 'unreaped-zombie' and result['firstUnreapedZombie'] is None:
        result['firstUnreapedZombie'] = sample
    if sample['production']['status'] == 'unavailable':
        result['productionUnavailableSamples'] += 1
        if result['firstProductionUnavailable'] is None:
            result['firstProductionUnavailable'] = sample
    if sample['production'].get('esrchRecheck') is not None:
        result['internalESRCHRecheckSamples'] += 1
        if result['firstInternalESRCHRecheck'] is None:
            result['firstInternalESRCHRecheck'] = sample
    for field, counter in (('sameGenerationLiveTaskAllESRCH', 'sameGenerationLiveTaskAllESRCHSamples'),
                           ('execBoundSameGenerationLiveTaskAllESRCH',
                            'execBoundSameGenerationLiveTaskAllESRCHSamples')):
        if sample[field]:
            result[counter] += 1
            if result['firstSameGenerationLiveTaskAllESRCH'] is None:
                result['firstSameGenerationLiveTaskAllESRCH'] = sample


def close_fd(handles, name):
    fd = handles.get(name)
    if fd is not None:
        # Forget only after close succeeds; cleanup will retry a failed close.
        os.close(fd)
        handles[name] = None


def spawn_child(task, handles):
    handles['controlRead'], handles['controlWrite'] = os.pipe()
    handles['readyRead'], handles['readyWrite'] = os.pipe()
    child = subprocess.Popen(
        [sys.executable, '-c', CHILD, str(handles['controlRead']), str(handles['readyWrite'])],
        pass_fds=(handles['controlRead'], handles['readyWrite']),
        env={**clean_environment(task), 'PYTHONDONTWRITEBYTECODE': '1'},
        stdin=subprocess.DEVNULL, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    return child


def cleanup_child(child, handles, reader, receipt):
    receipt.update(controlPipeClosed=False, leaderReaped=False, exitCode=None,
                   descriptorsClosed=False, groupGone=None,
                   groupCleanupScope='outer-monitored-stage',
                   outerGroupCompletionRequired=True, signalsSent=[], cleanupFailures=[])
    for name in ('controlWrite', 'controlRead', 'readyRead', 'readyWrite'):
        try:
            close_fd(handles, name)
        except BaseException as error:
            receipt['cleanupFailures'].append({'operation': 'close-' + name,
                                               'kind': error_kind(error), 'errno': error_errno(error)})
    receipt['controlPipeClosed'] = handles.get('controlWrite') is None
    receipt['descriptorsClosed'] = all(value is None for value in handles.values())
    if child is None:
        receipt['noChildCreated'] = True
        return
    operation = 'wait-natural'
    try:
        try:
            child.wait(timeout=2)
        except subprocess.TimeoutExpired:
            operation = 'terminate-exact-child'
            child.terminate()
            receipt['signalsSent'].append('SIGTERM')
            try:
                child.wait(timeout=2)
            except subprocess.TimeoutExpired:
                operation = 'kill-exact-child'
                child.kill()
                receipt['signalsSent'].append('SIGKILL')
                operation = 'wait-after-kill'
                child.wait(timeout=2)
        receipt.update(leaderReaped=child.returncode is not None, exitCode=child.returncode)
        if reader is not None:
            reader.reaped = receipt['leaderReaped']
    except BaseException as error:
        receipt['cleanupFailures'].append({'operation': operation, 'kind': error_kind(error),
                                           'errno': error_errno(error)})
    finally:
        receipt.update(leaderReaped=child.returncode is not None, exitCode=child.returncode)
        if reader is not None:
            reader.reaped = receipt['leaderReaped']


def record_failure(result, log, error):
    if result['firstFailure'] is None:
        code = str(error) if isinstance(error, ProbeFailure) else 'task-api-' + error_kind(error)
        result['firstFailure'] = {'code': code, 'phase': log.phase, 'kind': error_kind(error),
                                  'errno': error_errno(error), 'childIndex': log.child,
                                  'sampleIndex': log.sample}
        if log.stop is None and log.recent:
            log.stop = dict(log.recent[-1])
        if log.active_sample is not None:
            result['stoppingSample'] = dict(log.active_sample)
            result['stoppingSample']['queryTrace'] = [dict(row) for row in log.recent
                if row['sampleIndex'] == log.sample]
        elif result['recent16Samples']:
            result['stoppingSample'] = result['recent16Samples'][-1]


def exercise_child(task, index, log, result):
    child, reader, handles = None, None, {}
    control = {'childIndex': index, 'status': 'failed', 'ready': False,
               'spawnBindingVerified': False, 'heldLiveMeasured': False,
               'eofReleased': False, 'unreapedZombieVerified': False,
               'naturalExit': None, 'cleanup': {}}
    result['children'].append(control)
    try:
        log.phase, log.child, log.sample = 'spawn', index, None
        log.check_deadline()
        child = spawn_child(task, handles)
        close_fd(handles, 'controlRead')
        close_fd(handles, 'readyWrite')
        log.phase = 'ready'
        readable, _, _ = select.select([handles['readyRead']], [], [], 3)
        ensure(readable and os.read(handles['readyRead'], 1) == b'R', 'child-ready')
        close_fd(handles, 'readyRead')
        control['ready'] = True
        log.phase = 'binding'
        reader = TracedActivity(child.pid, log)
        before = raw_bsd(reader)
        first, second = raw_unique(reader), raw_unique(reader)
        after = raw_bsd(reader)
        ensure(before is not None and after is not None and before.status in (1, 2, 3, 4) and
               after.status in (1, 2, 3, 4) and identity(before) == identity(after) and
               before.uid == after.uid == os.geteuid() and before.ruid == after.ruid == os.getuid() and
               before.ppid == after.ppid == os.getpid() and before.pgid == after.pgid == os.getpgrp() and
               first is not None and second is not None and unique_key(first) == unique_key(second),
               'spawn-binding')
        expected, anchor = identity(before), unique_key(first)
        control['spawnBindingVerified'] = True
        log.phase = 'held-live'
        sample = snapshot(reader, expected, anchor, result['sampleCount'])
        retain_sample(result, sample)
        ensure(sample['production']['status'] == 'measured' and
               sample['beforeBSD']['sameSpawnFiveTuple'] is True and
               sample['afterBSD']['sameSpawnFiveTuple'] is True and
               sample['uniqueExecMatchesSpawnAnchor'] is True, 'held-live-measurement')
        control['heldLiveMeasured'] = True
        close_fd(handles, 'controlWrite')
        control['eofReleased'] = True
        log.phase = 'exit-window'
        zombie = False
        for _ in range(6):
            log.check_deadline()
            sample = snapshot(reader, expected, anchor, result['sampleCount'])
            retain_sample(result, sample)
            if sample['afterBSD']['status'] == 5:
                zombie = True
                break
            time.sleep(.001)
        log.phase = 'zombie-wait'
        # The leader is deliberately unreaped: no Popen.poll or wait in this
        # interval. Full BSD(arg=1) must identify the exact zombie generation.
        for _ in range(64):
            if zombie:
                break
            value = raw_bsd(reader)
            ensure(value is None or identity(value) == expected, 'generation-changed')
            if value is not None and value.status == 5:
                zombie = True
                break
            time.sleep(.005)
        ensure(zombie, 'zombie-not-observed')
        log.phase = 'unreaped-zombie'
        sample = snapshot(reader, expected, anchor, result['sampleCount'])
        retain_sample(result, sample)
        ensure(sample['beforeBSD']['status'] == sample['afterBSD']['status'] == 5 and
               sample['fiveTupleBeforeAfterMatches'] is True and
               sample['production']['status'] == 'gone' and
               sample['production']['reason'] == 'bsd-zombie', 'zombie-generation')
        control['unreapedZombieVerified'] = True
        # Only now reap the exact Popen child; do not query a recycled PID.
        log.phase = 'cleanup'
        child.wait(timeout=2)
        reader.reaped = True
        control['naturalExit'] = child.returncode
        ensure(child.returncode == 0, 'natural-exit')
        control['status'] = 'completed'
    except BaseException as error:
        record_failure(result, log, error)
        raise
    finally:
        log.phase = 'cleanup'
        cleanup_child(child, handles, reader, control['cleanup'])
        cleanup = control['cleanup']
        complete = cleanup['descriptorsClosed'] and (child is None or cleanup['leaderReaped'])
        if cleanup['cleanupFailures'] or not complete:
            control['status'] = 'failed'
            error = ProbeFailure('task-api-child-cleanup')
            record_failure(result, log, error)
            if sys.exc_info()[0] is None:
                raise error


def run_experiment(task, result, *, children=MAX_CHILDREN):
    """Mutate a safe receipt even on error. No SDK, build or census gate used."""
    log = TraceLog()
    result.update(status='failed', observationOnly=True, actualDarwinExecution=False,
                  checkpointEligibilityNotEvaluated=True, productionGateChanged=False,
                  comparisonFallbackAuthority=False, rawProcessMetadataPublished=False,
                  resourceLimitsChanged=False, unrelatedProcessesQueried=False,
                  childGroupScope='inherited-outer-monitored-stage',
                  outerRunnerResourceMonitoringRequired=True,
                  outerRunnerGroupCleanupRequired=True,
                  outerGroupGoneVerifiedHere=False,
                  requestedChildren=children if type(children) is int and
                                    1 <= children <= MAX_CHILDREN else None,
                  children=[], sampleCount=0, sampleCap=MAX_SAMPLES,
                  sampleCountsByPhase={}, recent16Samples=[], firstHeldLive=None,
                  firstUnreapedZombie=None, firstProductionUnavailable=None,
                  firstInternalESRCHRecheck=None, internalESRCHRecheckSamples=0,
                  firstSameGenerationLiveTaskAllESRCH=None, stoppingSample=None,
                  productionUnavailableSamples=0, sameGenerationLiveTaskAllESRCHSamples=0,
                  execBoundSameGenerationLiveTaskAllESRCHSamples=0, firstFailure=None,
                  workDeadlineSeconds=WORK_SECONDS,
                  observationLimit=['Controlled exact Python children only; no Floorp/Rust workload.',
                                    'A short run cannot establish absence of rare exit races.',
                                    'No reproduction does not prove a repair of run 35.',
                                    'The production outer 0.25-second census recheck is not exercised.',
                                    'Synchronous libproc calls are bounded by the outer Runner, not interrupted here.',
                                    'Run this CLI under Runner; resource monitoring and final group-gone proof are external.',
                                    'Task-info flavor 4 never authorizes measurement fallback.',
                                    'No writer, checkpoint, or long-build qualification is evaluated.'])
    try:
        ensure(type(children) is int and 1 <= children <= MAX_CHILDREN, 'child-count')
        ensure(sys.platform == 'darwin', 'platform')
        ensure((ctypes.sizeof(BSDInfo), ctypes.sizeof(TaskInfo), ctypes.sizeof(TaskAllInfo),
                ctypes.sizeof(UniqueInfo)) == (136, 96, 232, 56), 'abi')
        result['actualDarwinExecution'] = True
        for index in range(children):
            exercise_child(Path(task), index, log, result)
        log.phase = 'complete'
        result['status'] = 'completed'
        result['experimentCompleted'] = True
    except BaseException as error:
        record_failure(result, log, error)
        result['experimentCompleted'] = False
        raise
    finally:
        result['queryTrace'] = log.summary()
        result['sameGenerationLiveTaskAllESRCHObserved'] = (
            result['execBoundSameGenerationLiveTaskAllESRCHSamples'] > 0)
        result['run35FailureReproduction'] = 'not-established'
        result['repairEstablished'] = False


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', type=Path, required=True)
    args = parser.parse_args()
    result = {}
    try:
        run_experiment(args.task, result)
    except BaseException:
        # The safe primary failure was captured before cleanup; no raw error.
        pass
    try:
        write_json(args.task / 'task-api-experiment.json', result)
    except BaseException:
        result.update(status='failed', evidenceWriteError=True)
        if result.get('firstFailure') is None:
            result['firstFailure'] = {'code': 'task-api-evidence-write', 'phase': 'evidence-write',
                                      'kind': 'exception', 'errno': None,
                                      'childIndex': None, 'sampleIndex': None}
    print(json.dumps({'taskAPIExperiment': result}, sort_keys=True), flush=True)
    return 0 if result.get('status') == 'completed' and not result.get('evidenceWriteError') else 1


if __name__ == '__main__':
    raise SystemExit(main())
