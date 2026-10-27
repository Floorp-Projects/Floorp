# SPDX-License-Identifier: MPL-2.0
"""Checkpoint-free scope45 gates under the original job clock.

This route qualifies only the unchanged targeted XPCOM scope, never packaging,
full-host acceptance or native Shim authentication. No clock is created here.
"""
import json
import math
import os
from pathlib import Path
import stat
import tempfile
import time

from qa_core import Failure, require
from mozilla_target_build import stage_complete, proof_complete as targets_complete, STAGES
from mozilla_upstream import (EXTRA_STAGES, PREPARE_STAGES, proof_complete as upstream_complete,
                              asset_precondition_evidence, execution_ledger)

STATUS = 'mozilla-fresh45-ready'
QUALIFICATION = 'fresh-job-current-source-upstream-xpcom-v1'
EXPORT_STAGE = 'fresh45-exports'
FORBIDDEN_STAGES = frozenset(('full-build', 'stage-package', 'adhoc-copy', 'verify-copy',
    'native-suite', 'verify-copy-after', 'checkpoint-restore', 'checkpoint-exports',
    'checkpoint-rust', 'checkpoint-pack'))


def validation_outcome(report):
    failed = (report.get('monitorFailure') is not None or
              report.get('mozilla', {}).get('monitorFailure') is not None or
              report.get('mozillaUpstream', {}).get('monitorFailure') is not None)
    return {'execution': 'monitor-inconclusive' if failed else report['status'],
            'nativeAuthentication': 'not-verified',
            'monitorFailureEstablishesProductFailure': False}


def finite_float(value):
    result = float(value)
    require(math.isfinite(result), 'fresh-summary-readback')
    return result


def write_summary(path, value):
    """Bounded atomic owned receipt; never follow a substituted destination."""
    raw = (json.dumps(value, sort_keys=True, indent=2, allow_nan=False) + '\n').encode()
    require(0 < len(raw) <= 4 * 1024**2, 'fresh-summary-write')
    path = Path(path)
    try:
        before = path.lstat()
    except FileNotFoundError:
        before = None
    if before is not None:
        require(stat.S_ISREG(before.st_mode) and before.st_uid == os.getuid() and
                before.st_nlink == 1, 'fresh-summary-write')
    descriptor, name = tempfile.mkstemp(prefix='.fresh-summary-', dir=path.parent)
    try:
        with os.fdopen(descriptor, 'wb') as output:
            output.write(raw)
            output.flush()
            os.fsync(output.fileno())
        os.replace(name, path)
    finally:
        try:
            os.unlink(name)
        except FileNotFoundError:
            pass


def work_within(task, runner, origin, deadline):
    """Read-only original-origin check; never create a clock or extend a budget."""
    try:
        if not (type(origin) is bytes and 0 < len(origin) <= 128 and
                read_origin(task) == origin and type(deadline) is int and deadline == 345):
            return False
        started = float(origin)
        actual = runner.monotonic_started
        now = time.monotonic()
        return (math.isfinite(started) and started > 0 and type(actual) in (int, float) and
                math.isfinite(actual) and actual == started and 0 <= now - started < deadline * 60)
    except (Failure, OSError, ValueError, TypeError, OverflowError, AttributeError):
        return False


def same_value(left, right):
    """JSON evidence equality is type-strict, including bool versus integer."""
    if type(left) is not type(right):
        return False
    if type(left) is dict:
        return left.keys() == right.keys() and all(same_value(left[k], right[k]) for k in left)
    if type(left) is list:
        return len(left) == len(right) and all(same_value(a, b) for a, b in zip(left, right))
    return left == right


def final_problem(task, runner, report, origin, deadline, budget):
    """Recheck current and persisted scope45 proof without promoting failed work."""
    if not work_within(task, runner, origin, deadline):
        return 'fresh-work-deadline'
    problem = None
    try:
        required = ('configure', 'effective-config', EXPORT_STAGE) + STAGES + EXTRA_STAGES
        records, saved = runner.records, report.get('stages')
        if runner.failure is not None or report.get('firstFailure') is not None:
            problem = 'fresh-first-failure-present'
        elif runner.monitor_failure is not None or report.get('monitorFailure') is not None:
            problem = 'fresh-monitor-unavailable'
        elif not (type(records) is list and records and type(saved) is list and
                all(stage_complete(row) for row in records) and
                all(stage_complete(row) for row in saved) and same_value(records, saved) and
                all(sum(row['stage'] == name for row in records) == 1 for name in required) and
                not any(row['stage'] in FORBIDDEN_STAGES or row['stage'].startswith('checkpoint-')
                        for row in records) and
                [row['stage'] for row in records if row['stage'] in required] ==
                    ['configure', 'effective-config', EXPORT_STAGE, *PREPARE_STAGES,
                     *STAGES, *EXTRA_STAGES[len(PREPARE_STAGES):]]):
            problem = 'fresh-stage-proof-incomplete'
        else:
            upstream = report.get('mozillaUpstream', {})
            smoke = report.get('mozilla', {})
            expected_diagnostic = {'schema': 1, 'status': 'passed', 'checkedAssets': 6,
                                   'failure': None, 'inspection': None}
            diagnostics = [upstream.get(key) for key in
                           ('nativeAssetPrecondition', 'postNativeAssetPrecondition')]
            if not (type(report) is dict and report.get('status') == STATUS and
                    report.get('qualification') == QUALIFICATION and
                    all(key in report for key in ('monitorPreflight', 'harness', 'sdk', 'toolchains',
                        'source', 'effectiveConfig', 'hostObject', 'firstFailure', 'monitorFailure')) and
                    report['monitorPreflight'].get('status') == 'passed' and
                    type(report['toolchains']) is list and len(report['toolchains']) == 11 and
                    all(report.get(key) is True for key in ('freshFirefox45', 'freshObjdir', 'sameJob')) and
                    report.get('checkpointUsed') is False and report.get('productQualification') is False and
                    report.get('fullHostQualification') is False and report.get('fullHostXUL') == 'NOT_TESTED' and
                    report.get('stagePackage') == report.get('nativeAuthentication') == 'NOT_TESTED' and
                    report.get('native') is None and runner.native_cleanup is None and
                    report.get('nativeCleanup') is None and
                    not any(key in report for key in ('checkpointRole', 'checkpointRestore',
                        'checkpointReplay', 'checkpointReplayContext', 'resumedEffectiveConfigVerified')) and
                    same_value(report.get('executionTimeBudget'), budget) and
                    all(same_value(value, expected_diagnostic) and
                        same_value(asset_precondition_evidence(value), expected_diagnostic)
                        for value in diagnostics) and
                    smoke.get('status') == 'passed' and smoke.get('monitorFailure') is None and
                    all(smoke.get(key) is True for key in ('actualCompiledDebug',
                        'actualCurrentSourceUpstreamTest', 'intentionalFailureDetected',
                        'allCasesExactReaped', 'allCasesGroupsGone')) and
                    type(smoke.get('cases')) is list and len(smoke['cases']) == 3 and
                    targets_complete(report.get('mozillaTargets', {}), smoke, records) and
                    upstream_complete(upstream, records)):
                problem = 'fresh-firefox45-proof-incomplete'
    except Exception:
        problem = 'fresh-final-state-unconfirmed'
    return problem if work_within(task, runner, origin, deadline) else 'fresh-work-deadline'


def owned_bytes(path, cap):
    """Read one owned regular inode with no link following and a fixed cap."""
    def binding(value):
        return (value.st_dev, value.st_ino, value.st_uid, value.st_mode, value.st_nlink,
                value.st_size, value.st_mtime_ns, value.st_ctime_ns)
    before = path.lstat()
    require(stat.S_ISREG(before.st_mode) and before.st_uid == os.getuid() and
            before.st_nlink == 1 and 0 < before.st_size <= cap, 'fresh-summary-readback')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    try:
        require(binding(os.fstat(fd)) == binding(before), 'fresh-summary-readback')
        chunks, remaining = [], before.st_size + 1
        while remaining:
            chunk = os.read(fd, min(65536, remaining))
            if not chunk:
                break
            chunks.append(chunk)
            remaining -= len(chunk)
        raw = b''.join(chunks)
        require(len(raw) == before.st_size and binding(os.fstat(fd)) == binding(before) and
                binding(path.lstat()) == binding(before), 'fresh-summary-readback')
    finally:
        os.close(fd)
    return raw


def read_origin(task):
    return owned_bytes(task / 'started-monotonic', 128)


def read_summary(path):
    """Bounded owned readback with strict duplicate and finite-number checks."""
    def pairs(rows):
        value = {}
        for key, item in rows:
            require(key not in value, 'fresh-summary-readback')
            value[key] = item
        return value
    raw = owned_bytes(path, 4 * 1024**2)
    value = json.loads(raw, object_pairs_hook=pairs, parse_float=finite_float,
                       parse_constant=lambda _: require(False, 'fresh-summary-readback'))
    require(type(value) is dict, 'fresh-summary-readback')
    return value


def finish(task, runner, report, origin, deadline, budget):
    """Latch failures across write/readback/print; this cannot promote failed work."""
    snapshot = lambda value: json.loads(json.dumps(value))
    budget = snapshot(budget)
    state = {'failed': report.get('status') != STATUS, 'primary': None, 'monitor': None}

    def reject(code, stage='final-proof'):
        state['failed'] = True
        for value in (runner.failure, report.get('firstFailure')):
            if state['primary'] is None and value is not None:
                state['primary'] = snapshot(value)
        if state['primary'] is None:
            runner.fail(stage, code)
            state['primary'] = snapshot(runner.failure)

    def check(value=None):
        for observed in (runner.failure, report.get('firstFailure')):
            if state['primary'] is None and observed is not None:
                state['primary'] = snapshot(observed)
        for observed in (runner.monitor_failure, report.get('monitorFailure')):
            if state['monitor'] is None and observed is not None:
                state['monitor'] = snapshot(observed)
        problem = final_problem(task, runner, report if value is None else value,
                                      origin, deadline, budget)
        if problem is not None:
            reject(problem)
        if state['failed']:
            report.update(status='failed', firstFailure=state['primary'],
                          monitorFailure=state['monitor'])
        report['stages'], report['nativeCleanup'] = runner.records, runner.native_cleanup
        report['validationOutcome'] = validation_outcome(report)
        report['mozillaUpstreamExecution'] = execution_ledger(report.get('mozillaUpstream'))

    def persist():
        check()
        operation = 'write'
        try:
            write_summary(task / 'summary.json', report)
            check()
            operation = 'readback'
            saved = read_summary(task / 'summary.json')
            if not state['failed']:
                check(saved)
            if not same_value(saved, snapshot(report)):
                reject('fresh-summary-readback')
            check()
        except BaseException as error:
            code = 'fresh-summary-write' if operation == 'write' else 'fresh-summary-readback'
            reject(code, 'evidence-write')
            if isinstance(error, OSError):
                report['evidenceWriteError'] = error.errno
            check()

    def emit():
        try:
            print(json.dumps(report), flush=True)
        except BaseException:
            reject('fresh-summary-print', 'evidence-print')
        finally:
            check()

    check()
    persist()
    if state['failed']:
        persist()
    emit()
    # emit()'s finally is the last success gate; no new I/O or clock sample
    # follows it on the success path. Failures still enter bounded repair.
    if state['failed']:
        # Repair a previously passed on-disk receipt after a write/print callback.
        # Bounded retries: a persistently unwritable receipt never grants success.
        persist()
        emit()
        persist()
    return 0 if not state['failed'] and report.get('status') == STATUS else 1


