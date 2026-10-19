# SPDX-License-Identifier: MPL-2.0
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
from support import Failure, clean_environment, require, write_json
from native_cause import STREAM_MARKERS, load_module


def read_one(directory, name):
    files = list(directory.rglob(name))
    require(len(files) == 1, 'native-report-count')
    require(files[0].stat().st_size < 2 * 1024**2, 'native-report-size')
    return json.loads(files[0].read_text())


def context_result(value):
    require(value['status'] == 'passed' and value['requestedIterations'] == 20 and
            value['attemptedIterations'] == 20 and value['customMenuVisible'] is True,
            'context-menu-result')
    require(not value.get('diagnosticsIncomplete') and not value.get('cleanupErrors'), 'context-diagnostics')
    events = [e for e in value['results'] if e['type'] == 'contextmenu']
    require(len(events) == 20 and all(e['constructor'] == 'PointerEvent' and
            e['pointerType'] == 'mouse' and e['button'] == 2 and e['isTrusted'] is True
            for e in events), 'context-menu-trusted-events')
    kernel = value['shimLifecycle']
    require(any(e['kind'] == 'kernel-exit-observed' and e.get('pid') == value['shimPid']
                for e in kernel['events']), 'context-shim-cleanup-exit')
    return {'status': 'passed', 'attempted': 20, 'trustedContextMenus': 20,
            'shimPID': value['shimPid'], 'cleanupKernelExitObserved': True}


def failure_recording(harness, value):
    """Reproject the independent pre/post record without replacing first cause."""
    try:
        helper = load_module('native_failure_recording',
            harness / 'tools/app-shim/shim_failure_diagnostics.py')
        return helper.project_diagnostics(value.get('failureDiagnostics'))
    except Exception:
        return {'schema': 1, 'available': False}


def failure_snapshot(value):
    snapshot = value.get('beforeCleanup', {})
    shim = snapshot.get('shim', {})
    reasons = {'launch-failed', 'process-exited', 'process-monitor-failed', 'send-failed',
               'not-running', 'connection-generation-exhausted', 'window-close-timeout',
               'hello-timeout', 'launch-cancelled', 'service-stopped', 'window-retirement-limit', 'other'}
    rows = []
    for event in shim.get('events', [])[-16:]:
        if event.get('type') not in ('connected', 'disconnected'):
            continue
        row = {'type': event['type']}
        for key in ('pid', 'timeMs', 'sequence', 'causeTimeMs'):
            if type(event.get(key)) is int:
                row[key] = event[key]
        for key in ('reason', 'causeReason'):
            if event.get(key) in reasons:
                row[key] = event[key]
        if type(event.get('connectedBeforeDisconnect')) is bool:
            row['connectedBeforeDisconnect'] = event['connectedBeforeDisconnect']
        rows.append(row)
    return {'events': rows, 'truncated': shim.get('truncated') is True,
            'nativeAuthenticationCompleted': None,
            'cleanupErrorsPresent': bool(value.get('cleanupErrors'))}


def cause_failure_snapshot(value):
    result = {key: value[key] for key in
              ('status', 'phase', 'causeReason', 'connectedBeforeDisconnect',
               'kernelExitObserved', 'cleanupComplete', 'errorCode', 'diagnosticsIncomplete',
               'snapshotErrorType') if key in value}
    if type(value.get('errorErrno')) is int:
        result['errorErrno'] = value['errorErrno']
    before = value.get('beforeCleanup', {})
    if type(before.get('observedAtMs')) is int:
        result['beforeCleanupObservedAtMs'] = before['observedAtMs']
    source = before.get('hostStartup', {})
    host = {}
    for key in ('started', 'browserIdentityObserved', 'marionetteOwnershipVerified', 'launchServices',
                'configuredAboutBlank', 'browserExitStatusAvailable'):
        if type(source.get(key)) is bool:
            host[key] = source[key]
    if source.get('browserLiveness') in ('not-observed', 'alive', 'exit-observed'):
        host['browserLiveness'] = source['browserLiveness']
    for key in ('browserExitCode', 'browserSignal', 'launcherExitCode', 'requestedSignal'):
        if source.get(key) is None or type(source.get(key)) is int:
            host[key] = source.get(key)
    host['configuredBrowserSwitches'] = [flag for flag in source.get('configuredBrowserSwitches', [])
        if flag in ('--no-remote', '--profile', '--marionette', '--remote-allow-system-access')]
    if type(source.get('browserArgvObservedMatches')) is bool or source.get('browserArgvObservedMatches') is None:
        host['browserArgvObservedMatches'] = source.get('browserArgvObservedMatches')
    result['beforeCleanupHostStartup'] = host
    result['beforeCleanupNativeProbeEnvironment'] = {key: entry for key, entry in
        before.get('nativeProbeEnvironment', {}).items() if key in
        ('GITHUB_TOKEN', 'SDK_READ_TOKEN', 'ACTIONS_RUNTIME_TOKEN', 'MOZ_DISABLE_CONTENT_SANDBOX',
         'DYLD_LIBRARY_PATH', 'DYLD_INSERT_LIBRARIES') and type(entry) is bool}
    result['applicationEnvironmentBeforeMarionette'] = 'UNVERIFIED_LAUNCHSERVICES_ENVIRONMENT'
    result['afterCleanupHost'] = {key: entry for key, entry in value.get('afterCleanupHost', {}).items()
        if key in ('browserExitCode', 'browserSignal', 'launcherExitCode', 'requestedSignal',
                   'browserExitStatusAvailable') and (entry is None or type(entry) in (int, bool))}
    result['beforeCleanupStreams'] = {}
    for label in ('launcher', 'stdout', 'stderr'):
        source = before.get('streams', {}).get(label, {})
        record = {'markers': [marker for marker in source.get('markers', [])
                               if marker in STREAM_MARKERS.values()]}
        for key in ('present', 'rawContentPublished', 'tailTruncated'):
            if type(source.get(key)) is bool:
                record[key] = source[key]
        for key in ('bytesObserved', 'headBytesRead', 'tailBytesRead'):
            if type(source.get(key)) is int and source[key] >= 0:
                record[key] = source[key]
        record['osStatusCodes'] = [code for code in source.get('osStatusCodes', [])
                                    if type(code) is int and -(2**31) <= code < 2**31][:8]
        result['beforeCleanupStreams'][label] = record
    return result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--browser', type=Path, required=True)
    parser.add_argument('--harness', type=Path, required=True)
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--build-id', required=True)
    args = parser.parse_args()
    require(not any(k in os.environ for k in ('GITHUB_TOKEN', 'SDK_READ_TOKEN', 'ACTIONS_RUNTIME_TOKEN',
                'MOZ_DISABLE_CONTENT_SANDBOX', 'DYLD_LIBRARY_PATH', 'DYLD_INSERT_LIBRARIES')), 'native-environment')
    private = args.task / 'native-private'
    private.mkdir(mode=0o700)
    env = clean_environment(args.task)
    report = {'status': 'failed', 'contextMenu': None, 'gui': None, 'firstCause': None,
              'nativeFailureDiagnostics': {}}
    child = None
    try:
        commands = [
            ('cause', [sys.executable, str(Path(__file__).with_name('native_cause.py')),
                       '--browser', str(args.browser), '--harness', str(args.harness),
                       '--output', str(private / 'cause'), '--build-id', args.build_id]),
            ('context', [sys.executable, str(args.harness / 'tools/app-shim/test-context-menu.py'),
                         '--browser', str(args.browser), '--output', str(private / 'context'), '--iterations', '20']),
            ('gui', [sys.executable, str(args.harness / 'tools/app-shim/test-runtime.py'),
                     '--browser', str(args.browser), '--output', str(private / 'gui'), '--launch-services'])]
        for name, command in commands:
            with (private / (name + '.private.log')).open('xb') as output:
                child = subprocess.Popen(command, env=env, stdout=output, stderr=subprocess.STDOUT)
                code = child.wait()
            if name in ('context', 'gui'):
                try:
                    value = read_one(private / name, 'report.json' if name == 'gui' else 'result.json')
                except Exception:
                    value = {}
                report['nativeFailureDiagnostics'][name] = failure_recording(args.harness, value)
            if code and name == 'context':
                try:
                    report['firstFailureSnapshot'] = failure_snapshot(read_one(private / name, 'result.json'))
                except Exception:
                    report['firstFailureSnapshot'] = {'unavailable': True}
            elif code and name == 'cause':
                try:
                    value = read_one(private / name, 'result.json')
                    report['firstFailureSnapshot'] = cause_failure_snapshot(value)
                except Exception:
                    report['firstFailureSnapshot'] = {'unavailable': True}
            require(code == 0, 'native-' + name + '-exit')
            if name == 'context':
                report['contextMenu'] = context_result(read_one(private / name, 'result.json'))
            elif name == 'gui':
                value = read_one(private / name, 'report.json')
                native = value.get('nativeInput')
                require(value['status'] == 'passed' and not value.get('cleanupError') and
                        not value.get('hostCleanupError') and isinstance(native, dict) and
                        native.get('matched') is True and native.get('eventCount', 0) > 0,
                        'native-gui-result')
                report['gui'] = {'status': 'passed', 'nativeInputSkipped': False}
            else:
                value = read_one(private / name, 'result.json')
                require(value['status'] == 'passed' and value['kernelExitObserved'] is True and
                        value['cleanupComplete'] is True and value['causeReason'] == 'launch-cancelled' and
                        value['hostOwnershipVerified'] is True and not value.get('diagnosticsIncomplete') and
                        value['connectedBeforeDisconnect'] is True and value['contentSandboxLevel'] == 3 and
                        value['browserCompiledDebug'] is True and value['contentSandboxUserOverride'] is False and
                        value['contentSandboxDisableEnvEmpty'] is True, 'native-first-cause-result')
                report['firstCause'] = {key: value[key] for key in ('status', 'causeReason', 'causeTimeMs',
                      'terminalReason', 'connectedBeforeDisconnect', 'kernelExitObserved', 'cleanupComplete',
                      'hostOwnershipVerified',
                      'contentSandboxLevel', 'browserCompiledDebug', 'contentSandboxUserOverride',
                      'contentSandboxDisableEnvEmpty', 'browserBuildID', 'browserVersion')}
        report['status'] = 'passed'
    except BaseException as error:
        report['errorCode'] = str(error) if isinstance(error, Failure) else type(error).__name__
        if child is not None and child.poll() is None:
            try:
                child.wait(timeout=80)
            except subprocess.TimeoutExpired:
                report['cleanupIncomplete'] = True
    finally:
        write_json(args.task / 'native-summary.json', report)
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
