# SPDX-License-Identifier: MPL-2.0
"""Fresh inventory-helper import controls, idle and under bounded owned load.

The support-preimport control is not an exact recreation of the old program.
OS cache state is unknown. No cache flush, SDK, build or artifact is used.
"""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import select
import stat
import subprocess
import sys
import tempfile
import time

from qa_core import Failure, require, write_json
from darwin_inventory import PHASES, MAX_PROGRESS_EVENTS, decode_reply, progress_reply, timing_projection
from process_census import BINDING_KEYS, load_abi, prepare_abi

HERE = Path(__file__).resolve().parent
TIMEOUT = 3
VARIANTS = ('lean', 'support-preimport')
WORKER_CONTROL_CASE = ('test_helper_import_preflight.WorkloadTests.'
    'test_actual_small_workers_touch_memory_disk_and_exit_reaped_without_new_sessions')
CONTROL_FAILURE_CODES = (
    'helper-import-arguments', 'helper-import-binding-environment', 'helper-import-child-cleanup',
    'helper-import-control-failed', 'helper-import-control-group', 'helper-import-disk-binding',
    'helper-import-disk-cleanup', 'helper-import-disk-removal', 'helper-import-graph',
    'helper-import-insufficient-load', 'helper-import-lean-unavailable', 'helper-import-load-duration',
    'helper-import-load-ended-early', 'helper-import-platform', 'helper-import-preflight-failed',
    'helper-import-receipt-write', 'helper-import-sample-arguments', 'helper-import-sample-unavailable',
    'helper-import-stage-cleanup', 'helper-import-task-directory', 'helper-import-tmp-directory',
    'helper-import-variant-graph', 'helper-import-worker-arguments', 'helper-import-worker-barrier',
    'helper-import-worker-exit', 'helper-import-worker-ready', 'helper-import-worker-ready-timeout',
    'helper-import-worker-receipt', 'helper-import-worker-mode', 'helper-import-worker-write',
    'helper-import-workload-directory', 'helper-import-workload-failed', 'helper-import-workload-receipt')
GRAPH_FIELDS = ('qa_core', 'support', 'process_census', 'darwin_activity', 'ssl',
                'urllib.request', 'http.client', 'socket', 'subprocess', 'ctypes')
WRAPPER = '''import json,os,runpy,sys
variant,inventory,group=sys.argv[1:]
sys.path.insert(0,os.path.dirname(inventory))
if variant=='support-preimport':
 import support
sys.argv=[inventory,'--pgid',group]
runpy.run_path(inventory,run_name='__main__')
fields=FIELDS
sys.stderr.write(json.dumps({'helperImportGraph':{'schema':1,'variant':variant,
 'moduleCount':len(sys.modules),'modules':{name:name in sys.modules for name in fields}}})+'\\n')
'''.replace('FIELDS', repr(GRAPH_FIELDS))


def safe_error(error):
    return {'kind': 'timeout' if isinstance(error, subprocess.TimeoutExpired) else
        'permission' if isinstance(error, PermissionError) else
        'os-error' if isinstance(error, OSError) else
        'failure' if isinstance(error, Failure) else 'exception',
        'errno': error.errno if isinstance(error, OSError) and type(error.errno) is int else None,
        'failureCode': str(error) if isinstance(error, Failure) and str(error) in CONTROL_FAILURE_CODES else None}


def persist(path, result, label):
    """Persistence is independent evidence; it must never replace the first cause."""
    try:
        write_json(path, result)
    except BaseException as error:
        result['status'] = 'failed'
        result['receiptWriteFailure'] = safe_error(error)
        if result['firstFailure'] is None:
            result['firstFailure'] = {'phase': 'receipt', 'code': 'helper-import-receipt-write'}
    finally:
        print(json.dumps({label: result}), flush=True)


def number(value, *, integer=False, signed=False):
    valid = type(value) is int if integer else type(value) in (int, float)
    return value if valid and abs(value) < 2**63 and math.isfinite(value) and (signed or value >= 0) else None


def choice(value, allowed, fallback='other'):
    return value if type(value) is str and value in allowed else fallback


def worker_control_projection(value):
    """Pure, bounded fixture receipt. Invalid input never becomes valid on readback."""
    valid = isinstance(value, dict)
    value = value if valid else {}
    valid = valid and value.get('valid', True) is True
    def check(condition):
        nonlocal valid
        valid = valid and condition
    def enum(item, options, *, optional=False):
        if optional and item is None:
            return None
        if type(item) is str and item in options:
            return item
        check(False)
        return None
    def numeric(item, *, integer=True, optional=True, signed=False, maximum=2**63-1):
        if optional and item is None:
            return None
        projected = number(item, integer=integer, signed=signed)
        if projected is None or abs(projected) > maximum:
            check(False)
            return None
        return projected
    def boolean(item, *, optional=False):
        if optional and item is None:
            return None
        if type(item) is bool:
            return item
        check(False)
        return None
    def error(item, *, first=False, cleanup=False):
        if item is None:
            return None
        if not isinstance(item, dict):
            check(False)
            return None
        row = {'kind': enum(item.get('kind'), ('timeout', 'permission', 'process-lookup',
            'os-error', 'failure', 'exception', 'interrupted')),
            'errno': numeric(item.get('errno'), maximum=2**31-1),
            'failureCode': enum(item.get('failureCode'), CONTROL_FAILURE_CODES, optional=True)}
        if first:
            row.update(phase=enum(item.get('phase'), ('setup', 'worker-launch', 'worker-ready', 'worker-group',
                'worker-barrier', 'worker-wait', 'worker-measurements', 'assertions', 'cleanup',
                'disk-cleanup', 'temporary-cleanup', 'finished')),
                code=enum(item.get('code'), CONTROL_FAILURE_CODES))
        if cleanup:
            row['operation'] = enum(item.get('operation'), ('poll', 'terminate', 'wait', 'kill',
                'wait-after-kill', 'close-stream', 'collect-cleanup'))
        return row
    def entries(item):
        if not isinstance(item, list):
            check(False)
            return []
        check(len(item) <= 2)
        rows, seen = [], set()
        for row in item[:2]:
            if not isinstance(row, dict):
                check(False)
                continue
            index = numeric(row.get('index'), optional=False, maximum=1)
            check(index not in seen)
            seen.add(index)
            rows.append((index, row))
        return rows
    check(type(value.get('schema')) is int and value['schema'] == 1)
    check(value.get('case') == WORKER_CONTROL_CASE and value.get('mode') == 'fixed-iterations')
    check(type(value.get('iterations')) is int and value['iterations'] == 4)
    result = {'schema': 1, 'case': WORKER_CONTROL_CASE, 'mode': 'fixed-iterations', 'iterations': 4,
        'status': enum(value.get('status'), ('passed', 'failed', 'unavailable')) or 'unavailable',
        'readyWorkers': numeric(value.get('readyWorkers'), optional=False, maximum=2),
        'barriersReleased': numeric(value.get('barriersReleased'), optional=False, maximum=2),
        'sameGroupVerified': boolean(value.get('sameGroupVerified')),
        'firstFailure': error(value.get('firstFailure'), first=True),
        'workerMeasurements': [], 'cleanup': []}
    for index, raw in entries(value.get('workerMeasurements')):
        row = {'index': index, 'status': enum(raw.get('status'), ('passed', 'failed', 'unavailable')),
            'mode': enum(raw.get('mode'), ('fixed-iterations',), optional=True),
            'iterations': numeric(raw.get('iterations'), maximum=4),
            'sameGroupVerified': boolean(raw.get('sameGroupVerified')),
            'sufficient': boolean(raw.get('sufficient')),
            'fdClosed': boolean(raw.get('fdClosed'), optional=True),
            'failurePresent': boolean(raw.get('failurePresent'), optional=True),
            'exitCode': numeric(raw.get('exitCode'), signed=True, maximum=2**31-1)}
        for key in ('workerMiB', 'diskMiB', 'touchedBytes', 'cpuIterations', 'cpuHashedBytes',
                    'diskInitialWriteBytes', 'diskReadIterations', 'diskReadBytes', 'fsyncs'):
            row[key] = numeric(raw.get(key))
        row['elapsedSeconds'] = numeric(raw.get('elapsedSeconds'), integer=False, maximum=3600)
        if row['status'] == 'passed':
            check(row['mode'] == 'fixed-iterations' and row['iterations'] == 4 and
                row['workerMiB'] == 1 and row['diskMiB'] == (1 if index == 0 else 0) and
                row['touchedBytes'] == 1024**2 and row['cpuIterations'] == 4 and
                row['cpuHashedBytes'] == 4*1024**2 and row['elapsedSeconds'] is not None and
                row['failurePresent'] is False and row['diskInitialWriteBytes'] == (1024**2 if index == 0 else 0) and
                row['diskReadIterations'] == (4 if index == 0 else 0) and
                row['diskReadBytes'] == (4*1024**2 if index == 0 else 0) and
                row['fsyncs'] == (5 if index == 0 else 0) and
                row['fdClosed'] is (True if index == 0 else None))
        result['workerMeasurements'].append(row)
    for index, raw in entries(value.get('cleanup')):
        result['cleanup'].append({'index': index, 'reaped': boolean(raw.get('reaped')),
            'exitCode': numeric(raw.get('exitCode'), signed=True, maximum=2**31-1),
            'failure': error(raw.get('failure'), cleanup=True),
            'streamCloseFailed': boolean(raw.get('streamCloseFailed', False))})
    removal = value.get('temporaryDiskRemoval')
    if not isinstance(removal, dict):
        check(False)
        removal = {}
    result['temporaryDiskRemoval'] = {'status': enum(removal.get('status'),
        ('unverified', 'absent', 'removed-after-exact-reap')), 'failure': error(removal.get('failure'))}
    directory = value.get('temporaryDirectoryCleanup')
    if not isinstance(directory, dict):
        check(False)
        directory = {}
    result['temporaryDirectoryCleanup'] = {'status': enum(directory.get('status'),
        ('not-created', 'removed-after-exact-reap', 'deferred-unverified', 'failed')),
        'failure': error(directory.get('failure'))}
    if result['status'] == 'passed':
        check(result['readyWorkers'] == result['barriersReleased'] == 2 and result['sameGroupVerified'] is True and
            result['firstFailure'] is None and len(result['workerMeasurements']) == len(result['cleanup']) == 2 and
            all(row['status'] == 'passed' and row['sufficient'] is True and row['sameGroupVerified'] is True and
                row['exitCode'] == 0 for row in result['workerMeasurements']) and
            all(row['reaped'] is True and row['exitCode'] == 0 and row['failure'] is None and
                row['streamCloseFailed'] is False for row in result['cleanup']) and
            result['temporaryDiskRemoval'] == {'status': 'removed-after-exact-reap', 'failure': None} and
            result['temporaryDirectoryCleanup'] == {'status': 'removed-after-exact-reap', 'failure': None})
    result['valid'] = bool(valid)
    return result


STAGES = ('census-abi-compile', 'census-abi', 'helper-import-workload')
ERROR_TYPES = ('TimeoutExpired', 'PermissionError', 'ProcessLookupError', 'OSError',
               'Failure', 'ValueError', 'InterruptedError', 'KeyboardInterrupt',
               'ChildProcessError', 'CalledProcessError', 'AnchorError', 'AnchorSpawnError',
               'AnchorInterrupted', 'failure', 'os-error', 'anchor-error', 'interrupted',
               'validation', 'exception')
PRIMARY_FAILURE_CODES = ('owned-primary-anchor-required', 'owned-primary-anchor-unavailable',
    'owned-primary-exit-unavailable', 'owned-primary-observation-unavailable',
    'owned-primary-post-reap-group-unconfirmed', 'owned-primary-reap-unconfirmed',
    'owned-primary-signal-unavailable')
FAILURE_CODES = ('monitor-unavailable', 'nonzero-exit', 'platform-job-limit',
    'free-disk-reserve', 'private-log-limit', 'native-private-size-limit', 'phase-deadline',
    'owned-process-memory-limit', 'stalled-no-progress', 'owned-descendants-outlived-leader',
    'owned-native-cleanup-incomplete', 'owned-group-cleanup-incomplete', 'owned-leader-reap-failed',
    'owned-process-cleanup-timeout', *PRIMARY_FAILURE_CODES, *ERROR_TYPES)
PROCESS_PHASES = ('launch', 'primary-observation', 'resources', 'group-activity',
                  'native-census', 'progress', 'wait', 'cleanup')
# Fixed producer vocabulary; no import of the ownership primitive or raw receipt.
ANCHOR_REASONS = (
    'anchor-argv', 'anchor-bootstrap-stage', 'anchor-bsd-binding', 'anchor-bsd-unavailable',
    'anchor-census-limit', 'anchor-census-pids', 'anchor-census-schema', 'anchor-census-source',
    'anchor-census-unavailable', 'anchor-constructor-failed', 'anchor-constructor-interrupted',
    'anchor-deadline', 'anchor-echild', 'anchor-final-wait-interrupted',
    'anchor-final-wait-unavailable', 'anchor-generation-changed', 'anchor-hidden-reap-forbidden',
    'anchor-missing-from-roster', 'anchor-observation-binding', 'anchor-observation-schema',
    'anchor-observation-unavailable', 'anchor-operation-deadline', 'anchor-operation-busy',
    'anchor-popen-status-mutated', 'anchor-proc-binding', 'anchor-proc-schema', 'anchor-proc-size',
    'anchor-quarantine-limit', 'anchor-session', 'anchor-signal', 'anchor-platform',
    'anchor-sigchld-not-default', 'anchor-recovery-unbound', 'anchor-recovery-not-terminal',
    'anchor-recovery-not-ready', 'anchor-recovery-unavailable', 'anchor-terminal-mismatch',
    'anchor-terminal-race', 'anchor-wait-pid-mismatch', 'anchor-wait-signal', 'anchor-wait-status',
    'anchor-wait-status-mismatch', 'anchor-wait-not-ready', 'anchor-waitid-binding',
    'anchor-not-terminal', 'anchor-nonleaders-remain', 'anchor-signal-sent',
    'anchor-authority-revoked', 'anchor-recovered-without-group-proof')
ANCHOR_OPERATIONS = ('anchor-observation', 'anchor-retirement', 'anchor-group-signal',
    'anchor-post-reap-absence', 'anchor-exact-recovery', 'anchor-stop-refused',
    'anchor-group-inventory', 'group-probe', 'group-signal', 'denied-group-leader-reap')
ANCHOR_INVENTORY_FAILURE_CODES = (
    'anchor-inventory-deadline', 'anchor-inventory-abi-missing', 'anchor-inventory-size',
    'anchor-inventory-schema', 'anchor-inventory-empty-producer', 'anchor-inventory-unavailable',
    'anchor-inventory-timeout', 'anchor-inventory-producer-failed', 'inventory-protocol-size',
    'inventory-protocol', 'inventory-protocol-keys', 'inventory-protocol-queries',
    'inventory-protocol-query', 'inventory-protocol-unknown', 'inventory-protocol-failure',
    'inventory-protocol-pids', 'inventory-protocol-measurement', 'inventory-protocol-progress')
NATIVE_INVENTORY_FAILURE_CODES = (
    'inventory-helper-exception', 'inventory-query-permission', 'inventory-size-unavailable',
    'inventory-data-unavailable', 'inventory-size', 'inventory-returned-size',
    'inventory-record-length', 'inventory-record-pid', 'inventory-record-identity',
    'inventory-self-unavailable', 'inventory-self-count', 'inventory-abi-unavailable',
    'inventory-group-id', 'inventory-selector', 'inventory-retries-exhausted', 'inventory-platform')


def cleanup_diagnostic_projection(value):
    """One fixed lifecycle diagnostic, without identities or signal authority proof."""
    if type(value) is not dict:
        return None
    result = {'operation': choice(value.get('operation'), ANCHOR_OPERATIONS, 'unavailable')}
    for key in ('status', 'state'):
        if key in value:
            result[key] = choice(value[key],
                ('held', 'live', 'zombie', 'reaping', 'reaped', 'lost', 'recovered',
                 'unavailable', 'observed', 'unconfirmed'), 'unavailable')
    for key, allowed in (('reason', ANCHOR_REASONS), ('errorType', ERROR_TYPES)):
        if key in value:
            result[key] = None if value[key] is None else choice(value[key], allowed, 'unavailable')
    for key in ('errno', 'actualReturncode', 'observedExitCode', 'leaderExitAtProbe', 'boundedWaitSeconds'):
        if key in value:
            result[key] = number(value[key], integer=key != 'boundedWaitSeconds',
                signed=key in ('actualReturncode', 'observedExitCode', 'leaderExitAtProbe'))
    if 'signal' in value:
        item = value['signal']
        result['signal'] = item if type(item) is int and item in (0, 2, 9, 15) else None
    for key in ('actualReaped', 'signalAuthority', 'freshIdentityVerified', 'groupEmpty',
                'readOnly', 'leaderWasUnreaped', 'freshProbeAttempted'):
        if key in value:
            result[key] = value[key] if type(value[key]) is bool else None
    if 'reapResult' in value:
        result['reapResult'] = choice(value['reapResult'],
            ('already-reaped', 'pending', 'reaped', 'unavailable', 'still-running'), 'unavailable')
    if 'backend' in value:
        result['backend'] = choice(value['backend'],
            ('darwin-header-bound', 'darwin-bootstrap-ps', 'linux-ps-test'), 'unavailable')
    if 'failureCode' in value:
        result['failureCode'] = choice(value['failureCode'], ANCHOR_INVENTORY_FAILURE_CODES, 'unavailable')
    if 'producerFailureCode' in value:
        result['producerFailureCode'] = choice(
            value['producerFailureCode'], NATIVE_INVENTORY_FAILURE_CODES, 'unavailable')
    if 'phase' in value:
        result['phase'] = choice(value['phase'], (*PHASES, 'startup'), 'unobserved')
    if 'timeoutSeconds' in value:
        seconds = number(value['timeoutSeconds'])
        result['timeoutSeconds'] = seconds if seconds is not None and 0 < seconds <= TIMEOUT else None
    progress = value.get('helperProgress')
    if type(progress) is dict:
        events = number(progress.get('events'), integer=True)
        result['helperProgress'] = {
            'phase': choice(progress.get('phase'), PHASES, 'unobserved'),
            'events': events if events is not None and events <= MAX_PROGRESS_EVENTS else None}
        timings=timing_projection(progress)
        if timings is not None:result['helperProgress']['timings']=timings
    return result


def runner_failure(value):
    if not isinstance(value, dict):
        return None
    return {'source': 'runner', 'stage': choice(value.get('stage'), STAGES),
        'code': choice(value.get('code'), FAILURE_CODES, 'runner-failure'),
        'elapsedSeconds': number(value.get('elapsedSeconds')),
        'existingEvidenceInvalid': value.get('existingEvidenceInvalid') is True,
        'persistenceErrno': number(value.get('persistenceError'), integer=True)}


def monitor_projection(value, *, partial=False):
    """Drop raw identities, examples, paths and arbitrary keys at every level."""
    if not isinstance(value, dict):
        return None
    known = ('inventory-timeout', 'inventory-unavailable', 'inventory-format', 'duplicate-pid',
        'empty-group', 'column-count', 'pid-unavailable', 'rss-unavailable', 'cpu-unavailable',
        'state-unavailable', 'permission', 'invalid-identity', 'invalid-bsd-state', 'short-bsd-info',
        'short-task-info', 'membership-changed', 'invalid-task-counters',
        'generation-changed-during-recheck')
    kinds = value.get('unknownKinds')
    result = {'unknownKinds': {key: number(item, integer=True) for key, item in kinds.items()
                              if key in known} if isinstance(kinds, dict) else None}
    for key in ('rssMiB', 'cpuSeconds', 'rssLowerBoundMiB', 'cpuLowerBoundSeconds',
                'knownRSSLowerBoundMiB', 'knownCPULowerBoundSeconds', 'elapsedSeconds'):
        if key in value:
            result[key] = number(value[key])
    if not partial:
        result.update(stage=choice(value.get('stage'), STAGES),
            sampleComplete=value.get('sampleComplete') if type(value.get('sampleComplete')) is bool else None,
            sampleAttempts=number(value.get('sampleAttempts'), integer=True),
            processes=number(value.get('processes'), integer=True),
            inventoryBackend=choice(value.get('inventoryBackend'),
                ('kern-proc-pgrp-helper', 'bootstrap-ps', 'linux-ps'), 'unavailable'),
            errorType=choice(value.get('errorType'), ERROR_TYPES, 'unavailable'))
        items = value.get('partialSamples')
        result['partialSamples'] = [monitor_projection(item, partial=True) for item in items[-3:]] if isinstance(items, list) else []
    progress=value.get('helperProgress')
    if type(progress) is dict:
        events=number(progress.get('events'),integer=True)
        result['helperProgress']={'phase':choice(progress.get('phase'),PHASES,'unobserved'),
            'events':events if events is not None and events<=MAX_PROGRESS_EVENTS else None}
        timings=timing_projection(progress)
        if timings is not None:result['helperProgress']['timings']=timings
    failure = value.get('queryFailure')
    if isinstance(failure, dict):
        query = {'operation': choice(failure.get('operation'),
            ('kern-proc-pgrp-inventory', 'process-group-inventory')),
            'errorType': choice(failure.get('errorType'), ERROR_TYPES, 'unavailable'),
            'phase': choice(failure.get('phase'), (*PHASES, 'startup'), 'unobserved')}
        for key in ('errno', 'exitCode', 'timeoutSeconds', 'elapsedSeconds'):
            if key in failure:
                query[key] = number(failure[key], integer=key in ('errno', 'exitCode'), signed=key == 'exitCode')
        codes = ('inventory-helper-exception', 'inventory-query-permission', 'inventory-size-unavailable',
            'inventory-data-unavailable', 'inventory-size', 'inventory-returned-size', 'inventory-record-length',
            'inventory-record-pid', 'inventory-record-identity', 'inventory-self-unavailable', 'inventory-self-count',
            'inventory-abi-unavailable', 'inventory-group-id', 'inventory-selector', 'inventory-retries-exhausted',
            'inventory-platform', 'inventory-abi-missing', 'inventory-protocol-size', 'inventory-protocol',
            'inventory-protocol-keys', 'inventory-protocol-queries', 'inventory-protocol-query',
            'inventory-protocol-unknown', 'inventory-protocol-failure', 'inventory-protocol-pids',
            'inventory-protocol-measurement', 'inventory-protocol-progress')
        if 'failureCode' in failure:
            query['failureCode'] = choice(failure['failureCode'], codes, 'inventory-helper-exception')
        queries = failure.get('queries')
        if isinstance(queries, list):
            query['queries'] = [{
                'query': choice(item.get('query'), ('self-size', 'self-data', 'group-size', 'group-data')),
                **{key: number(item[key], integer=True, signed=key == 'returnCode')
                   for key in ('attempt', 'returnCode', 'errno', 'bytes', 'capacityBytes') if key in item}}
                for item in queries[:12] if isinstance(item, dict)]
        progress = failure.get('helperProgress')
        if isinstance(progress, dict):
            query['helperProgress'] = {'phase': choice(progress.get('phase'), PHASES, 'unobserved'),
                                      'events': number(progress.get('events'), integer=True)}
            timings=timing_projection(progress)
            if timings is not None:query['helperProgress']['timings']=timings
        result['queryFailure'] = query
    return result


PRIMARY_ANCHOR_KEYS = frozenset(('policy', 'retirementVerified', 'actualReaped', 'actualReturncode',
    'signalAuthorityRevoked', 'postReapAbsence', 'scope', 'sessionOrDescendantClosure'))
PRIMARY_ABSENCE_KEYS = frozenset(('status', 'groupEmpty', 'readOnly', 'signalAuthority'))


def primary_anchor_complete(value, exit_code):
    if type(value) is not dict or set(value) != PRIMARY_ANCHOR_KEYS or type(exit_code) is not int:
        return False
    absence = value['postReapAbsence']
    return (type(absence) is dict and set(absence) == PRIMARY_ABSENCE_KEYS and
            type(value['policy']) is str and value['policy'] == 'retained-direct-child-v1' and
            value['retirementVerified'] is True and value['actualReaped'] is True and
            type(value['actualReturncode']) is int and value['actualReturncode'] == exit_code and
            value['signalAuthorityRevoked'] is True and
            type(value['scope']) is str and value['scope'] == 'active-process-group-epoch' and
            type(value['sessionOrDescendantClosure']) is str and
            value['sessionOrDescendantClosure'] == 'unconfirmed' and
            type(absence['status']) is str and absence['status'] == 'observed' and
            absence['groupEmpty'] is True and absence['readOnly'] is True and
            absence['signalAuthority'] is False)


def stage_complete(row):
    leader = row.get('leaderReap') if type(row) is dict else None
    return (type(row) is dict and type(row.get('exitCode')) is int and row['exitCode'] == 0 and
            row.get('groupCleanup') == 'verified-gone' and
            all(key in row and row[key] is None for key in ('reason', 'failurePhase', 'cleanupFailure')) and
            type(leader) is dict and leader.get('status') == 'reaped' and
            type(leader.get('exitCode')) is int and leader['exitCode'] == 0 and
            primary_anchor_complete(row.get('primaryAnchor'), row['exitCode']))


def primary_anchor_projection(value):
    # Reject shape changes as a unit: dropping an extra private key must not
    # turn a forged proof into a valid proof on a later readback.
    if type(value) is not dict or set(value) != PRIMARY_ANCHOR_KEYS:
        return None
    absence = value['postReapAbsence']
    if type(absence) is not dict or set(absence) != PRIMARY_ABSENCE_KEYS:
        return None
    boolean = lambda item: item if type(item) is bool else None
    return {'policy': choice(value['policy'], ('retained-direct-child-v1',), None),
        'retirementVerified': boolean(value['retirementVerified']),
        'actualReaped': boolean(value['actualReaped']),
        'actualReturncode': number(value['actualReturncode'], integer=True, signed=True),
        'signalAuthorityRevoked': boolean(value['signalAuthorityRevoked']),
        'postReapAbsence': {
            'status': choice(absence['status'], ('observed', 'unconfirmed'), None),
            'groupEmpty': boolean(absence['groupEmpty']), 'readOnly': boolean(absence['readOnly']),
            'signalAuthority': boolean(absence['signalAuthority'])},
        'scope': choice(value['scope'], ('active-process-group-epoch',), None),
        'sessionOrDescendantClosure': choice(value['sessionOrDescendantClosure'], ('unconfirmed',), None)}


def stage_projection(row):
    row = row if type(row) is dict else {}
    leader = row.get('leaderReap')
    leader = leader if isinstance(leader, dict) else {}
    return {'stage': choice(row.get('stage'), STAGES),
        'exitCode': number(row.get('exitCode'), integer=True, signed=True),
        'groupCleanup': choice(row.get('groupCleanup'), ('verified-gone', 'not-launched', 'unavailable')),
        'leaderReap': {'status': choice(leader.get('status'),
            ('reaped', 'not-launched', 'still-running', 'unavailable')),
            'exitCode': number(leader.get('exitCode'), integer=True, signed=True),
            'errorType': choice(leader.get('errorType'), ERROR_TYPES, 'unavailable'),
            'errno': number(leader.get('errno'), integer=True)},
        'primaryAnchor': primary_anchor_projection(row.get('primaryAnchor')),
        'reason': None if row.get('reason') is None else choice(row['reason'], FAILURE_CODES, 'runner-failure'),
        'failurePhase': None if row.get('failurePhase') is None else choice(
            row['failurePhase'], PROCESS_PHASES, 'unavailable'),
        'cleanupFailurePresent': row.get('cleanupFailure') is not None,
        'cleanupFailureCode': None if row.get('cleanupFailure') is None else choice(
            row['cleanupFailure'], FAILURE_CODES, 'cleanup-failure'),
        'cleanupDiagnostics': [cleanup_diagnostic_projection(item)
            for item in row.get('cleanupDiagnostics', [])[-16:] if type(item) is dict]
            if type(row.get('cleanupDiagnostics', [])) is list else [],
        'cleanupStoppingDiagnostic': cleanup_diagnostic_projection(row.get('cleanupStoppingDiagnostic')),
        'cleanupInventoryFailure': cleanup_diagnostic_projection(row.get('cleanupInventoryFailure')),
        'peakRSSMiB': number(row.get('peakRSSMiB')), 'minimumFreeGiB': number(row.get('minimumFreeGiB')),
        'lastActivity': monitor_projection(row.get('lastActivity'))}


def validate_arguments(seconds, pairs, workers, mib, disk_mib):
    require(type(seconds) in (int, float) and math.isfinite(seconds) and 0 < seconds <= 90 and
            type(pairs) is int and 2 <= pairs <= 10 and type(workers) is int and 2 <= workers <= 3 and
            type(mib) is int and 1 <= mib and workers*mib <= 512 and
            type(disk_mib) is int and 1 <= disk_mib <= 128, 'helper-import-arguments')


def helper_environment(source):
    names = (*BINDING_KEYS, 'TASK_ROOT', 'FLOORP_CENSUS_ABI', 'HOME', 'TMPDIR', 'LANG', 'LC_ALL')
    value = {key: source[key] for key in names if key in source}
    require(all(key in value for key in (*BINDING_KEYS, 'TASK_ROOT', 'FLOORP_CENSUS_ABI')),
            'helper-import-binding-environment')
    value['PYTHONDONTWRITEBYTECODE'] = '1'
    return value


def import_graph(output, variant):
    require(isinstance(output, str) and len(output.encode()) <= 8192, 'helper-import-graph')
    value = json.loads(output)
    require(isinstance(value, dict) and set(value) == {'helperImportGraph'}, 'helper-import-graph')
    graph = value['helperImportGraph']
    require(isinstance(graph, dict) and set(graph) == {'schema', 'variant', 'moduleCount', 'modules'} and
            type(graph['schema']) is int and graph['schema'] == 1 and graph['variant'] == variant and
            type(graph['moduleCount']) is int and 0 < graph['moduleCount'] < 10000 and
            isinstance(graph['modules'], dict) and set(graph['modules']) == set(GRAPH_FIELDS) and
            all(type(value) is bool for value in graph['modules'].values()), 'helper-import-graph')
    return graph


def timeout_context(error):
    """A run() kill/wait error must not erase a preceding TimeoutExpired."""
    for _ in range(4):
        if isinstance(error, subprocess.TimeoutExpired):
            return error
        error = getattr(error, '__context__', None)
        if error is None:
            break
    return None


def sample(variant, phase, ordinal, env, pgid):
    require(variant in VARIANTS and phase in ('idle', 'contended') and
            type(ordinal) is int and ordinal > 0 and type(pgid) is int and
            0 < pgid < 2**31 and pgid == os.getpgrp(), 'helper-import-sample-arguments')
    value = {'variant': variant, 'phase': phase, 'ordinal': ordinal, 'status': 'unavailable',
        'elapsedSeconds': None, 'progress': {'phase': 'unobserved', 'events': 0},
        'measurement': None, 'importGraph': None, 'failure': None, 'timeoutObserved': False,
        'cleanup': {'authority': 'subprocess.run-exact-child', 'reaped': None,
                    'killedOnTimeout': None, 'failure': None}}
    started = time.monotonic()
    output = None
    try:
        result = subprocess.run([sys.executable, '-S', '-c', WRAPPER, variant,
            str(HERE/'darwin_inventory.py'), str(pgid)], capture_output=True, check=True,
            timeout=TIMEOUT, text=True, env=helper_environment(env), cwd=HERE)
        value['cleanup'].update(reaped=True, killedOnTimeout=False)
        output = result.stdout
        decoded = decode_reply(output, pgid)
        value['progress'] = progress_reply('\n'.join(output.splitlines()[:-1])+'\n')
        if decoded['status'] != 'measured':
            value['status'] = 'inventory-unavailable'
            value['queryFailure'] = monitor_projection({'queryFailure': {
                'operation': 'kern-proc-pgrp-inventory', **decoded}})['queryFailure']
            return value
        measured = decoded['measurement']
        value['measurement'] = {key: measured[key] for key in
            ('api', 'snapshotBytes', 'snapshotEntries', 'selfBindingVerified', 'layoutSHA256')}
        value['importGraph'] = import_graph(result.stderr, variant)
        require(value['importGraph']['modules']['support'] is (variant == 'support-preimport'),
                'helper-import-variant-graph')
        value['status'] = 'measured'
    except subprocess.TimeoutExpired as error:
        value.update(status='timeout', timeoutObserved=True, progress=progress_reply(error.output),
                     failure=safe_error(error))
        value['cleanup'].update(reaped=True, killedOnTimeout=True)
    except BaseException as error:
        preceding = timeout_context(error)
        if preceding is not None:
            value.update(status='cleanup-unverified', timeoutObserved=True,
                         progress=progress_reply(preceding.output))
            value['cleanup']['failure'] = safe_error(error)
        elif isinstance(error, subprocess.CalledProcessError):
            value['cleanup'].update(reaped=True, killedOnTimeout=False)
            value.update(status='nonzero-exit', progress=progress_reply(error.output))
        elif output is not None:
            value.update(status='invalid-reply', progress=progress_reply(output))
            value['measurement'] = None
        else:
            value['status'] = 'spawn-or-setup-failed'
        value['failure'] = safe_error(error)
    finally:
        value['elapsedSeconds'] = round(time.monotonic()-started, 6)
    return value


def read_ready(child, seconds=15):
    deadline = time.monotonic()+seconds
    data = bytearray()
    while b'\n' not in data:
        left = deadline-time.monotonic()
        require(left > 0 and select.select([child.stdout], [], [], left)[0], 'helper-import-worker-ready-timeout')
        chunk = os.read(child.stdout.fileno(), 64)
        require(chunk and len(data)+len(chunk) <= 128, 'helper-import-worker-ready')
        data.extend(chunk)
    require(bytes(data) == b'ready\n', 'helper-import-worker-ready')


def worker(task, seconds, mib, disk_mib=0, index=0, *, iterations=None):
    require(type(seconds) in (int, float) and math.isfinite(seconds) and 0 < seconds <= 90 and
            type(mib) is int and 1 <= mib <= 256 and type(disk_mib) is int and 0 <= disk_mib <= 128 and
            type(index) is int and 0 <= index <= 2 and (index == 0 or disk_mib == 0) and
            (iterations is None or type(iterations) is int and iterations == 4 and mib == 1 and disk_mib <= 1),
            'helper-import-worker-arguments')
    result = {'schema': 1, 'index': index, 'status': 'failed',
        'mode': 'duration' if iterations is None else 'fixed-iterations', 'iterations': iterations,
        'seconds': seconds if iterations is None else None,
        'workerMiB': mib, 'diskMiB': disk_mib, 'touchedBytes': 0, 'cpuIterations': 0,
        'cpuHashedBytes': 0, 'diskInitialWriteBytes': 0, 'diskReadIterations': 0,
        'diskReadBytes': 0, 'fsyncs': 0, 'elapsedSeconds': None, 'fdClosed': None,
        'firstFailure': None}
    descriptor = None
    began = None
    try:
        memory = bytearray(mib*1024**2)
        for offset in range(0, len(memory), 4096):
            memory[offset] = 1
        result['touchedBytes'] = len(memory)
        if disk_mib:
            descriptor = os.open(task/'disk-control.bin', os.O_RDWR|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600)
            result['fdClosed'] = False
            block = b'x'*1024**2
            for _ in range(disk_mib):
                view = memoryview(block)
                while view:
                    written = os.write(descriptor, view)
                    require(written > 0, 'helper-import-worker-write')
                    result['diskInitialWriteBytes'] += written
                    view = view[written:]
            os.fsync(descriptor)
            result['fsyncs'] += 1
        print('ready', flush=True)
        require(select.select([sys.stdin], [], [], 15)[0] and os.read(sys.stdin.fileno(), 1) == b'1',
                'helper-import-worker-barrier')
        began = time.monotonic()
        until = began+seconds
        while (time.monotonic() < until if iterations is None else result['cpuIterations'] < iterations):
            hashlib.sha256(memoryview(memory)[:4*1024**2]).digest()
            result['cpuIterations'] += 1
            result['cpuHashedBytes'] += min(len(memory), 4*1024**2)
            if descriptor is not None:
                os.lseek(descriptor, 0, os.SEEK_SET)
                while chunk := os.read(descriptor, 1024**2):
                    hashlib.sha256(chunk).digest()
                    result['diskReadBytes'] += len(chunk)
                os.fsync(descriptor)
                result['diskReadIterations'] += 1
                result['fsyncs'] += 1
        result['status'] = 'passed'
    except BaseException as error:
        result['firstFailure'] = safe_error(error)
    finally:
        if began is not None:
            result['elapsedSeconds'] = time.monotonic()-began
        if descriptor is not None:
            try:
                os.close(descriptor)
                result['fdClosed'] = True
            except BaseException as error:
                result['status'] = 'failed'
                result['fdCloseFailure'] = safe_error(error)
                if result['firstFailure'] is None:
                    result['firstFailure'] = safe_error(error)
        try:
            write_json(task/('worker-'+str(index)+'.json'), result)
        except BaseException:
            result['status'] = 'failed'
    return result


def worker_measurement(task, index, seconds, mib, disk_mib, *, iterations=None):
    path = task/('worker-'+str(index)+'.json')
    require(not path.is_symlink() and path.stat().st_size <= 8192, 'helper-import-worker-receipt')
    raw = json.loads(path.read_text())
    require(isinstance(raw, dict) and raw.get('schema') == 1 and type(raw.get('index')) is int and
            raw['index'] == index and raw.get('status') in ('passed', 'failed') and
            type(raw.get('workerMiB')) is int and raw['workerMiB'] == mib and
            type(raw.get('diskMiB')) is int and raw['diskMiB'] == disk_mib,
            'helper-import-worker-receipt')
    require((iterations is None and raw.get('mode') == 'duration' and raw.get('iterations') is None and
             type(raw.get('seconds')) in (int, float) and raw['seconds'] == seconds) or
            (type(iterations) is int and iterations == 4 and raw.get('mode') == 'fixed-iterations' and
             type(raw.get('iterations')) is int and raw['iterations'] == iterations and raw.get('seconds') is None),
            'helper-import-worker-mode')
    keys = ('touchedBytes', 'cpuIterations', 'cpuHashedBytes', 'diskInitialWriteBytes',
            'diskReadIterations', 'diskReadBytes', 'fsyncs')
    value = {key: raw[key] for key in ('schema', 'index', 'status', 'mode', 'iterations', 'seconds', 'workerMiB', 'diskMiB')}
    for key in keys:
        require(number(raw.get(key), integer=True) is not None, 'helper-import-worker-receipt')
        value[key] = raw[key]
    value['elapsedSeconds'] = number(raw.get('elapsedSeconds'))
    value['fdClosed'] = raw.get('fdClosed') if type(raw.get('fdClosed')) is bool else None
    value['failurePresent'] = raw.get('firstFailure') is not None
    value['sufficient'] = (value['status'] == 'passed' and not value['failurePresent'] and
        value['touchedBytes'] == mib*1024**2 and value['cpuIterations'] > 0 and
        value['cpuHashedBytes'] == value['cpuIterations']*min(mib, 4)*1024**2 and
        value['elapsedSeconds'] is not None and
        (iterations is None and value['elapsedSeconds'] >= seconds or
         iterations is not None and value['cpuIterations'] == iterations) and
        value['diskInitialWriteBytes'] == disk_mib*1024**2 and
        (disk_mib == 0 and value['diskReadBytes'] == value['diskReadIterations'] == value['fsyncs'] == 0 or
         disk_mib > 0 and value['fdClosed'] is True and value['diskReadIterations'] > 0 and
         value['diskReadBytes'] == value['diskReadIterations']*disk_mib*1024**2 and
         value['fsyncs'] == value['diskReadIterations']+1))
    return value


def load_presence(children, expected):
    try:
        running = sum(child.poll() is None for child in children)
        return {'expectedWorkers': expected, 'runningWorkers': running,
                'complete': len(children) == running == expected, 'failure': None}
    except BaseException as error:
        return {'expectedWorkers': expected, 'runningWorkers': None, 'complete': False,
                'failure': safe_error(error)}


def disk_binding(path, size):
    value = path.lstat()
    require(stat.S_ISREG(value.st_mode) and value.st_uid == os.geteuid() and value.st_nlink == 1 and
            value.st_size == size, 'helper-import-disk-binding')
    return value.st_dev, value.st_ino


def workload(task, seconds=60, pairs=6, workers=2, mib=128, disk_mib=64):
    from inventory_stress import cleanup_children
    validate_arguments(seconds, pairs, workers, mib, disk_mib)
    require(task.is_dir() and not task.is_symlink() and task.stat().st_uid == os.geteuid(),
            'helper-import-workload-directory')
    result = {'status': 'failed', 'seconds': seconds, 'pairsPerPhase': pairs,
        'workers': workers, 'workerMiB': mib, 'maximumDiskMiB': disk_mib,
        'readyWorkers': 0, 'barriersReleased': 0, 'loadSeconds': None,
        'samples': [], 'loadChecks': [], 'firstFailure': None, 'firstControlFailure': None, 'cleanup': [],
        'workerMeasurements': [], 'temporaryDiskRemoval': {'status': 'unverified', 'failure': None},
        'samplingReserveSeconds': min(seconds/2, 2*TIMEOUT+2),
        'productionRSSReproduction': False, 'pressureScope': 'bounded-small-memory-and-disk-control',
        'osCacheState': 'unknown-cold-or-warm', 'osCacheFlushed': False,
        'exactOldProgramReproduction': False, 'startupRepairEstablished': False,
        'originalCauseEstablished': False, 'rawProcessMetadataPublished': False}
    children = []
    backing = None
    stage = 'idle-samples'
    def observe(phase, index):
        for variant in VARIANTS:
            before = load_presence(children, workers) if phase == 'contended' else None
            check = {'variant': variant, 'phase': phase, 'ordinal': index+1, 'before': before, 'after': None}
            result['loadChecks'].append(check)
            require(before is None or before['complete'], 'helper-import-load-ended-early')
            row = sample(variant, phase, index+1, os.environ, os.getpgrp())
            row['loadBefore'] = before
            row['loadAfter'] = load_presence(children, workers) if phase == 'contended' else None
            check['after'] = row['loadAfter']
            result['samples'].append(row)
            require(row['loadAfter'] is None or row['loadAfter']['complete'], 'helper-import-load-ended-early')
            if row['status'] != 'measured':
                key = 'firstFailure' if variant == 'lean' or row['cleanup']['reaped'] is not True else 'firstControlFailure'
                if result[key] is None:
                    result[key] = {'phase': phase, 'variant': variant, 'ordinal': index+1,
                                   'code': 'helper-import-sample-unavailable'}
    try:
        for index in range(pairs):
            observe('idle', index)
        stage = 'worker-launch'
        for index in range(workers):
            child = subprocess.Popen([sys.executable, str(Path(__file__).resolve()), '--worker',
                '--task', str(task), '--seconds', str(seconds), '--mib', str(mib),
                '--worker-disk-mib', str(disk_mib if index == 0 else 0), '--worker-index', str(index)],
                stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                env=helper_environment(os.environ))
            children.append(child)
        stage = 'worker-ready'
        for child in children:
            read_ready(child)
            result['readyWorkers'] += 1
        backing = disk_binding(task/'disk-control.bin', disk_mib*1024**2)
        stage = 'worker-barrier'
        began = time.monotonic()
        for child in children:
            child.stdin.write(b'1'); child.stdin.flush(); child.stdin.close()
            result['barriersReleased'] += 1
        stage = 'contended-samples'
        for index in range(pairs):
            target = began+index*(seconds-result['samplingReserveSeconds'])/(pairs-1)
            while time.monotonic() < target:
                time.sleep(max(0, min(.1, target-time.monotonic())))
            observe('contended', index)
        stage = 'worker-wait'
        for child in children:
            require(child.wait(timeout=max(1, began+seconds+10-time.monotonic())) == 0,
                    'helper-import-worker-exit')
        result['loadSeconds'] = time.monotonic()-began
        require(result['loadSeconds'] >= seconds, 'helper-import-load-duration')
        require(result['firstFailure'] is None, 'helper-import-lean-unavailable')
        result['status'] = 'passed'
    except BaseException as error:
        if result['firstFailure'] is None:
            result['firstFailure'] = {'phase': stage, 'code': 'helper-import-workload-failed', **safe_error(error)}
    finally:
        try:
            result['cleanup'] = cleanup_children(children)
        except BaseException as error:
            result['cleanup'] = [{'index': index, 'reaped': False, 'exitCode': None,
                'failure': safe_error(error)} for index in range(len(children))]
        failed = any(not row['reaped'] or row['failure'] is not None or row.get('streamCloseFailed')
                     for row in result['cleanup'])
        if failed:
            result['status'] = 'failed'
            if result['firstFailure'] is None:
                result['firstFailure'] = {'phase': 'cleanup', 'code': 'helper-import-child-cleanup'}
        for index, child in enumerate(children):
            try:
                value = worker_measurement(task, index, seconds, mib, disk_mib if index == 0 else 0)
            except BaseException as error:
                value = {'index': index, 'status': 'unavailable', 'sufficient': False,
                         'failure': safe_error(error)}
            value['exitCode'] = result['cleanup'][index]['exitCode']
            result['workerMeasurements'].append(value)
        if len(children) != workers or any(not row['sufficient'] or row['exitCode'] != 0
                                          for row in result['workerMeasurements']):
            result['status'] = 'failed'
            if result['firstFailure'] is None:
                result['firstFailure'] = {'phase': 'worker-measurements', 'code': 'helper-import-insufficient-load'}
        try:
            if backing is not None and not failed:
                require(disk_binding(task/'disk-control.bin', disk_mib*1024**2) == backing,
                        'helper-import-disk-binding')
                (task/'disk-control.bin').unlink()
                require(not os.path.lexists(task/'disk-control.bin'), 'helper-import-disk-removal')
                result['temporaryDiskRemoval']['status'] = 'removed-after-exact-reap'
            elif not os.path.lexists(task/'disk-control.bin'):
                result['temporaryDiskRemoval']['status'] = 'absent'
        except BaseException as error:
            result['temporaryDiskRemoval']['failure'] = safe_error(error)
        if result['temporaryDiskRemoval']['status'] == 'unverified':
            result['status'] = 'failed'
            if result['firstFailure'] is None:
                result['firstFailure'] = {'phase': 'disk-cleanup', 'code': 'helper-import-disk-cleanup'}
        result['counts'] = {phase: {variant: {
            'samples': sum(row['phase'] == phase and row['variant'] == variant for row in result['samples']),
            'validMeasurements': sum(row['phase'] == phase and row['variant'] == variant and row['status'] == 'measured'
                                     for row in result['samples']),
            'timeouts': sum(row['phase'] == phase and row['variant'] == variant and row['timeoutObserved']
                            for row in result['samples'])} for variant in VARIANTS} for phase in ('idle', 'contended')}
        persist(task/'workload.json', result, 'helperImportWorkload')
    return result


def select_abi(task, runner, env):
    path = task/'census-abi.json'
    reused = path.exists() or path.is_symlink()
    if reused:
        load_abi(path)
    else:
        path = prepare_abi(task, runner, env)
        load_abi(path)
    os.environ['FLOORP_CENSUS_ABI'] = str(path)
    env.update({key: os.environ[key] for key in (*BINDING_KEYS, 'TASK_ROOT')})
    env['FLOORP_CENSUS_ABI'] = str(path)
    return reused


def run_experiment(task, result):
    from support import Runner, clean_environment
    task = task.resolve()
    result.update(schema=1, status='failed', firstFailure=None, monitorFailure=None, monitorDiagnostics=[],
        workload=None, stages=[], abiReused=None, sdkUsed=False, floorpBuildPerformed=False,
        newArtifactsStored=0, originalCauseEstablished=False, startupRepairEstablished=False,
        exactOldProgramReproduction=False, productionRSSReproduction=False,
        osCacheState='unknown-cold-or-warm', osCacheFlushed=False,
        helperTimeoutSeconds=TIMEOUT, productionRetryPolicyChanged=False,
        resourceLimitsChanged=False, allCandidateAccountingChanged=False)
    runner = None
    private = None
    stage = 'platform'
    try:
        require(sys.platform == 'darwin', 'helper-import-platform')
        require(task.is_dir() and not task.is_symlink() and task.stat().st_uid == os.geteuid(),
                'helper-import-task-directory')
        plan = json.loads((HERE/'plan.json').read_text())
        runner = Runner(task, float((task/'started').read_text()), plan['limits'])
        env = clean_environment(task)
        temporary = task/'tmp'; temporary.mkdir(mode=0o700, exist_ok=True)
        require(temporary.is_dir() and not temporary.is_symlink() and temporary.stat().st_uid == os.geteuid(),
                'helper-import-tmp-directory')
        env['PYTHONDONTWRITEBYTECODE'] = '1'
        stage = 'abi'
        result['abiReused'] = select_abi(task, runner, env)
        private = Path(tempfile.mkdtemp(prefix='helper-import-', dir=task)); private.chmod(0o700)
        stage = 'helper-import-workload'
        runner.run(stage, [sys.executable, str(Path(__file__).resolve()), '--workload', '--task', str(private)],
                   HERE, env, 360, phase_deadline_minutes=15)
        value = json.loads((private/'workload.json').read_text())
        result['workload'] = value
        require(value['status'] == 'passed', 'helper-import-workload-failed')
        require(runner.failure is None and runner.monitor_failure is None and runner.records and
            all(stage_complete(row) for row in runner.records), 'helper-import-stage-cleanup')
        result['status'] = 'passed'
    except BaseException as error:
        result['firstFailure'] = (runner_failure(runner.failure) if runner is not None else None) or {
            'phase': stage, 'code': 'helper-import-preflight-failed', **safe_error(error)}
    finally:
        try:
            if runner is not None:
                primary = runner_failure(runner.failure)
                if primary is not None:
                    result['firstFailure'] = primary
                    result['status'] = 'failed'
                result['monitorFailure'] = monitor_projection(runner.monitor_failure)
                result['monitorDiagnostics'] = [monitor_projection(row) for row in runner.monitor_diagnostics]
                result['stages'] = [stage_projection(row) for row in runner.records]
            if private is not None:
                if (private/'workload.json').is_file():
                    result['workload'] = json.loads((private/'workload.json').read_text())
                    write_json(task/'helper-import-workload.json', result['workload'])
        except BaseException as error:
            result['workloadReceiptFailure'] = safe_error(error)
            result['status'] = 'failed'
            if result['firstFailure'] is None:
                result['firstFailure'] = {'phase': 'receipt', 'code': 'helper-import-workload-receipt'}
        finally:
            persist(task/'helper-import-preflight.json', result, 'helperImportPreflight')
    return result


def main():
    parser = argparse.ArgumentParser()
    modes = parser.add_mutually_exclusive_group()
    modes.add_argument('--workload', action='store_true')
    modes.add_argument('--worker', action='store_true')
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=60)
    parser.add_argument('--pairs', type=int, default=6)
    parser.add_argument('--workers', type=int, default=2)
    parser.add_argument('--mib', type=int, default=128)
    parser.add_argument('--disk-mib', type=int, default=64)
    parser.add_argument('--worker-disk-mib', type=int, default=0)
    parser.add_argument('--worker-index', type=int, default=0)
    parser.add_argument('--worker-iterations', type=int)
    args = parser.parse_args()
    if args.worker:
        result = worker(args.task, args.seconds, args.mib, args.worker_disk_mib, args.worker_index,
                        iterations=args.worker_iterations)
        return 0 if result['status'] == 'passed' else 1
    validate_arguments(args.seconds, args.pairs, args.workers, args.mib, args.disk_mib)
    if args.workload:
        result = workload(args.task, args.seconds, args.pairs, args.workers, args.mib, args.disk_mib)
    else:
        result = run_experiment(args.task, {})
    return 0 if result['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
