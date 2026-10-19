# SPDX-License-Identifier: MPL-2.0
"""Run three bounded, current-source Mozilla xpcshell harness controls."""
import hashlib
import json
import os
from pathlib import Path
import sys

from helper_import_preflight import (ERROR_TYPES, FAILURE_CODES, choice,
                                     monitor_projection, number, runner_failure,
                                     stage_projection)
from mozilla_harness_receipt import receipt
from support import Failure, Runner, digest, require, write_json


HERE = Path(__file__).resolve().parent
DEBUG_MARKER = '"FLOORP_QA_COMPILED_DEBUG_158_0_1_V1"'
FAILURE_MARKER = 'FLOORP_QA_EXPECTED_FAILURE_V1'
CONTRACT = 'current-source-mozilla-xpcshell-smoke-v1'
SOURCE_COMMIT = '7dd619742b25567e479eeda97ae071d10542ad0b'
SMOKE_FAILURE_CODES = (*FAILURE_CODES, 'mozilla-smoke-source-contract', 'mozilla-smoke-input-hash',
    'mozilla-smoke-network-environment', 'mozilla-smoke-effective-config', 'mozilla-smoke-bundle-name',
    'mozilla-smoke-current-built-asset', 'mozilla-smoke-asset-identity', 'mozilla-smoke-case-failed',
    'mozilla-smoke-debug-assertions', 'mozilla-smoke-post-case-binding', 'mozilla-smoke-post-input-binding')
EXCEPTION_TYPES = (*ERROR_TYPES, 'RuntimeError', 'TypeError', 'AttributeError', 'AssertionError',
                   'FileExistsError', 'FileNotFoundError', 'SystemExit')


def error_code(error):
    if isinstance(error, Failure):
        return choice(str(error), SMOKE_FAILURE_CODES, 'runner-failure')
    return choice(type(error).__name__, EXCEPTION_TYPES, 'exception')


def verify_inputs(source, fixture):
    data = (HERE / 'mozilla-smoke.json').read_bytes()
    plan = json.loads(data)
    require(plan['contract'] == CONTRACT and plan['source']['commit'] == SOURCE_COMMIT and
            plan['source']['version'] == '158.0.1', 'mozilla-smoke-source-contract')
    for root, rows in ((source, plan['source']['files']), (fixture, plan['fixtures'])):
        for row in rows:
            path = root / row['path']
            require(path.is_file() and not path.is_symlink() and
                    path.stat().st_size == row['bytes'] and digest(path) == row['sha256'],
                    'mozilla-smoke-input-hash')
    return {'contract': CONTRACT, 'sourceCommit': SOURCE_COMMIT,
            'planSHA256': hashlib.sha256(data).hexdigest(),
            'sourceFilesVerified': len(plan['source']['files']),
            'fixtureFilesVerified': len(plan['fixtures'])}


def child_environment(env):
    result = dict(env)
    # The stock harness retries even one sequential test when this key exists.
    result.pop('MOZ_AUTOMATION', None)
    # These selected tests do not use the detached HTTP/2 support server.
    require(not any(key in result for key in ('MOZHTTP2_PORT', 'MOZNODE_EXEC_PORT',
                                              'MOZ_ASSUME_NODE_RUNNING')),
            'mozilla-smoke-network-environment')
    result['MOZ_ASSUME_NODE_RUNNING'] = '1'
    return result


def command(source, binary, manifest, test, raw):
    return [sys.executable, 'mach', 'xpcshell-test', '--verbose', '--sequential',
            '--threads', '1', '--no-logfiles', '--xpcshell', str(binary),
            '--manifest', str(manifest), '--log-raw', str(raw), str(test)]


def process_evidence(case_runner):
    rows = case_runner.records if case_runner is not None else []
    if len(rows) != 1:
        return {'available': False, 'exitCode': None, 'exactReaped': None,
                'groupGone': None, 'cleanupClean': None, 'monitorFailureAbsent': None}
    row = rows[0]
    code = number(row.get('exitCode'), integer=True, signed=True)
    reap = row.get('leaderReap', {})
    exact_reaped = (type(code) is int and reap.get('status') == 'reaped' and
                    type(reap.get('exitCode')) is int and reap['exitCode'] == code)
    reason = row.get('reason')
    phase = row.get('failurePhase')
    return {'available': True, 'exitCode': code,
            'reason': None if reason is None else choice(reason, FAILURE_CODES, 'runner-failure'),
            'failurePhase': None if phase is None else choice(phase,
                ('launch', 'resources', 'group-activity', 'native-census', 'progress', 'wait', 'cleanup'),
                'unavailable'),
            'exactReaped': exact_reaped, 'groupGone': row.get('groupCleanup') == 'verified-gone',
            'cleanupClean': row.get('cleanupFailure') is None,
            'monitorFailureAbsent': case_runner.monitor_failure is None,
            'monitorDiagnosticsCount': len(case_runner.monitor_diagnostics),
            'minimumFreeGiB': number(row.get('minimumFreeGiB')),
            'peakRSSMiB': number(row.get('peakRSSMiB'))}


def stage_evidence(row):
    value = stage_projection(row)
    value['stage'] = 'mozilla-xpcshell' if row.get('stage') == 'mozilla-xpcshell' else None
    value['cleanupFailureCode'] = None if row.get('cleanupFailure') is None else choice(
        row['cleanupFailure'], (*FAILURE_CODES, 'owned-process-cleanup-timeout'), 'cleanup-failure')
    value['cleanupDiagnostics'] = []
    for item in row.get('cleanupDiagnostics', [])[-16:]:
        if type(item) is not dict:
            continue
        entry = {'operation':choice(item.get('operation'),
            ('group-probe', 'group-signal', 'denied-group-leader-reap'), 'unavailable')}
        for key in ('signal', 'errno', 'leaderExitAtProbe', 'boundedWaitSeconds'):
            if key in item:
                entry[key] = number(item[key], integer=key != 'boundedWaitSeconds',
                                    signed=key == 'leaderExitAtProbe')
        if 'errorType' in item:
            entry['errorType'] = choice(item['errorType'], ERROR_TYPES, 'unavailable')
        for key in ('leaderWasUnreaped', 'freshProbeAttempted'):
            if key in item:
                entry[key] = item[key] if type(item[key]) is bool else None
        if 'reapResult' in item:
            entry['reapResult'] = choice(item['reapResult'],
                ('already-reaped', 'pending', 'reaped', 'unavailable', 'still-running'), 'unavailable')
        value['cleanupDiagnostics'].append(entry)
    return value


def case_satisfied(value, expected_exit):
    process = value['process']
    clean = (process['available'] is True and type(process['exitCode']) is int and
             process['exitCode'] == expected_exit and process['exactReaped'] is True and
             process['groupGone'] is True and process['cleanupClean'] is True and
             process['monitorFailureAbsent'] is True)
    if expected_exit == 0:
        natural = (value['runnerFailure'] is None and process.get('reason') is None and
                   value['caughtCode'] is None)
    else:
        failure = value['runnerFailure']
        natural = (type(failure) is dict and failure.get('stage') == 'mozilla-xpcshell' and
                   failure.get('code') == 'nonzero-exit' and process.get('reason') == 'nonzero-exit' and
                   process.get('failurePhase') == 'wait' and value['caughtCode'] == 'nonzero-exit')
    return (clean and natural and value['harness']['expectationSatisfied'] is True and
            value.get('caseError') is None and value.get('evidenceWriteFailure') is None and
            value.get('evidenceCollectionFailure') is None)


def monitor_evidence(value, *, partial=False):
    result = monitor_projection(value, partial=partial)
    if result is None:
        return None
    if not partial:
        result['stage'] = 'mozilla-xpcshell' if value.get('stage') == 'mozilla-xpcshell' else None
    examples = value.get('formatExamples')
    if type(examples) is list:
        known = set(result.get('unknownKinds') or {}) | {'measured'}
        result['queryExamples'] = []
        for example in examples[-3:]:
            if type(example) is not dict:
                continue
            kind = example.get('kind')
            item = {'kind': kind if type(kind) is str and kind in known else 'unavailable'}
            for key in ('taskQuery', 'bsdQuery'):
                query = example.get(key)
                if type(query) is dict:
                    item[key] = {field: number(query[field], integer=True)
                                 for field in ('bytes', 'expectedBytes', 'errno') if field in query}
            result['queryExamples'].append(item)
    if not partial:
        samples = value.get('partialSamples')
        result['partialSamples'] = [monitor_evidence(row, partial=True) for row in samples[-3:]] \
            if type(samples) is list else []
    return result


def run_case(name, source, binary, manifest, test, task, parent_runner, env, deadline,
             *, negative=False, debug=False, value=None):
    case_task = task / ('mozilla-' + name)
    private = case_task / 'native-private'
    raw = private / 'harness.raw.jsonl'
    mode = 'negative-control' if negative else 'positive'
    value = {} if value is None else value
    value.update(case=name, mode=mode, expectedExitCode=1 if negative else 0,
                 caughtCode=None, caseError=None, caseFailurePhase=None,
                 evidenceWriteFailure=None, evidenceCollectionFailure=None,
                 runnerFailure=None, process=process_evidence(None),
                 stages=[], monitorDiagnostics=[], monitorFailure=None,
                 monitorFailureObserved=None,
                 harness={'status':'unavailable', 'expectationSatisfied':False,
                          'counts':None, 'reason':'mozilla-harness-not-executed'},
                 expectationSatisfied=False)
    case_runner, phase, created = None, 'setup', False
    private_errors = {}
    def retain_error(label, error):
        private_errors[label] = {'type':type(error).__name__, 'message':str(error)[:4096]}
    try:
        case_task.mkdir(mode=0o700)
        created = True
        (case_task / 'tmp').mkdir(mode=0o700)
        private.mkdir(mode=0o700)
        (case_task / 'started-monotonic').write_text(str(parent_runner.monotonic_started))
        case_runner = Runner(case_task, parent_runner.started, parent_runner.limits)
        phase = 'environment'
        case_env = child_environment(env)
        case_env['TMPDIR'] = str(case_task / 'tmp')
        phase = 'launch'
        try:
            case_runner.run('mozilla-xpcshell', command(source, binary, manifest, test, raw),
                            source, case_env, parent_runner.limits['jobMinutes'],
                            phase_deadline_minutes=deadline)
        except BaseException as error:
            retain_error('launch', error)
            value['caughtCode'] = error_code(error)
        phase = 'harness-receipt'
        value['harness'] = receipt(raw, [test.name], mode=mode,
                                   debug_marker=DEBUG_MARKER if debug else None,
                                   failure_marker=FAILURE_MARKER if negative else None)
    except BaseException as error:
        retain_error(phase, error)
        value.update(caseError=error_code(error),
                     caseFailurePhase=phase)
    finally:
        private_evidence = None
        try:
            if case_runner is not None:
                private_evidence = {'stages':case_runner.records,
                                    'runnerFailure':case_runner.failure,
                                    'monitorDiagnostics':case_runner.monitor_diagnostics[-16:],
                                    'monitorFailure':case_runner.monitor_failure}
                value['monitorFailureObserved'] = case_runner.monitor_failure is not None
                failure = runner_failure(case_runner.failure)
                if failure is not None:
                    failure['stage'] = 'mozilla-xpcshell' if case_runner.failure.get('stage') == 'mozilla-xpcshell' else None
                value['runnerFailure'] = failure
                value['process'] = process_evidence(case_runner)
                value['stages'] = [stage_evidence(row) for row in case_runner.records]
                value['monitorDiagnostics'] = [monitor_evidence(row)
                                                for row in case_runner.monitor_diagnostics[-16:]]
                value['monitorFailure'] = monitor_evidence(case_runner.monitor_failure)
            value['expectationSatisfied'] = case_satisfied(value, value['expectedExitCode'])
        except BaseException as error:
            retain_error('evidence-collection', error)
            value.update(evidenceCollectionFailure=error_code(error), expectationSatisfied=False)
        if created:
            try:
                write_json(case_task / 'receipt.json', {**value, 'privateEvidence':private_evidence,
                                                       'privateErrors':private_errors})
            except BaseException as error:
                value.update(evidenceWriteFailure=error_code(error), expectationSatisfied=False)
        else:
            value.update(evidenceWriteFailure='case-directory-unavailable', expectationSatisfied=False)
    return value


def run_smoke(source, fixture, task, runner, env, deadline, macho, result):
    """Preserve a negative child's failure separately from the build runner."""
    result.update(status='failed', contract=CONTRACT, nativeAuthentication='NOT_TESTED',
                  browserChrome='NOT_TESTED', broadFirefoxCoverage=False,
                  productQualification=False, cases=[],
                  networkServers={'http2': 'not-started-stock-assume-node-running',
                                  'http3': 'stock-harness-in-owned-stage-group',
                                  'networkCoverage': False},
                  automaticRetry=False, verbose=True, sequential=True, threads=1)
    before = result['inputs'] = verify_inputs(source, fixture)
    config = json.loads((task / 'effective-config.json').read_text())
    require(config['debug'] is True and config['pgo'] is False and
            config['MOZ_APP_VERSION'] == '158.0.1', 'mozilla-smoke-effective-config')
    bundle = config['MOZ_MACBUNDLE_NAME']
    require(type(bundle) is str and bundle.endswith('.app') and '/' not in bundle and
            '\\' not in bundle, 'mozilla-smoke-bundle-name')
    app = task / 'obj/dist' / bundle
    binary = app / 'Contents/MacOS/xpcshell'
    xul = app / 'Contents/MacOS/XUL'
    root = (task / 'obj').resolve()
    for path in (binary, xul):
        require(path.is_file() and path.resolve().is_relative_to(root) and
                os.access(path, os.X_OK) and path.stat().st_mtime >= runner.started,
                'mozilla-smoke-current-built-asset')
    assets = {'xpcshell': macho(binary, 2), 'XUL': macho(xul, 6)}
    result['builtAssets'] = assets
    require(assets['xpcshell']['uuid'] is not None and assets['XUL']['uuid'] is not None,
            'mozilla-smoke-asset-identity')
    cases = [('compiled-debug', fixture / 'mozilla-smoke/positive/xpcshell.toml',
              fixture / 'mozilla-smoke/positive/test_current_debug.js', False, True),
             ('failure-control', fixture / 'mozilla-smoke/negative/xpcshell.toml',
              fixture / 'mozilla-smoke/negative/test_expected_failure.js', True, False),
             ('upstream-version-comparator', source / 'xpcom/tests/unit/xpcshell.toml',
              source / 'xpcom/tests/unit/test_versioncomparator.js', False, False)]
    for name, manifest, test, negative, debug in cases:
        value = {}
        result['cases'].append(value)
        run_case(name, source, binary, manifest, test, task, runner, env, deadline,
                 negative=negative, debug=debug, value=value)
        if value['monitorFailureObserved'] is True:
            result['monitorFailure'] = value['monitorFailure'] or {'stage':'mozilla-xpcshell',
                                                                 'details':'unavailable'}
        require(value['expectationSatisfied'], 'mozilla-smoke-case-failed')
        if debug:
            require(value['harness']['counts']['subtestStatuses']['PASS'] >= 3,
                    'mozilla-smoke-debug-assertions')
        require(digest(binary) == assets['xpcshell']['sha256'] and
                digest(xul) == assets['XUL']['sha256'], 'mozilla-smoke-post-case-binding')
    require(verify_inputs(source, fixture) == before, 'mozilla-smoke-post-input-binding')
    result.update(status='passed', actualCompiledDebug=True,
                  actualCurrentSourceUpstreamTest=True, intentionalFailureDetected=True,
                  allCasesExactReaped=True, allCasesGroupsGone=True,
                  defaultUpstreamHeadLoaded=True)
