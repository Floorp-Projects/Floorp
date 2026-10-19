# SPDX-License-Identifier: MPL-2.0
"""Bounded safe failure facts, persisted before any browser cleanup request.

These are observation times, not kernel event timestamps. A missing observer
event or undiscovered child is unknown, not proof that no child launched.
"""
import copy
import importlib.util
import json
import os
from pathlib import Path
import time


LIMIT = 16
REASONS = frozenset(('launch-failed', 'process-exited', 'process-monitor-failed',
    'send-failed', 'not-running', 'connection-generation-exhausted',
    'window-close-timeout', 'hello-timeout', 'launch-cancelled', 'service-stopped',
    'window-retirement-limit', 'other'))
PHASES = frozenset(('host-signature', 'launch-host', 'host', 'native-service',
    'authenticated-shim', 'authenticate', 'gecko-window', 'shared-session',
    'native-input', 'window-close-retirement', 'native-process-recovery',
    'beforeunload-veto', 'app-quit', 'browser-quit-all',
    'browser-quit-with-app-running', 'context-menu', 'complete', 'connect-marionette',
    'seal-shim', 'launch-shim', 'authenticate-shim', 'create-pwa-window', 'completed'))
OPERATIONS = frozenset(('browser-service-stop', 'marionette-close', 'browser-host-close',
    'child-monitor-close', 'fixture-server-stop', 'browser-log-close'))
MONITOR_STATUS = frozenset(('not-started', 'discovering', 'monitoring', 'exit-observed',
    'not-observed', 'unavailable', 'unsupported-platform', 'private-bundle-unavailable'))
MONITOR_EVENTS = frozenset(('watcher-start', 'owned-pid-discovered', 'exit-monitor-registered',
    'kernel-exit-observed', 'observation-error', 'watcher-stop-timeout', 'watcher-stop'))
ERROR_KINDS = frozenset(('OSError', 'PermissionError', 'ProcessLookupError', 'TimeoutError',
    'TimeoutExpired', 'RuntimeError', 'ValueError', 'TypeError', 'KeyError', 'other'))
DIAGNOSTIC_OPERATIONS = frozenset(('child-monitor-start', 'browser-event-snapshot',
    'restore-rpc-timeout', 'child-monitor-snapshot', 'host-snapshot',
    'legacy-functional-event-snapshot',
    'write-native-before-cleanup.json', 'write-native-after-cleanup.json'))


def integer(value):
    return value if type(value) is int and -(2**63) <= value < 2**63 else None


def fact(error):
    kind = type(error).__name__
    return {'kind': kind if kind in ERROR_KINDS else 'other',
            'errno': integer(error.errno) if isinstance(error, OSError) else None}


def event(value):
    if not isinstance(value, dict) or value.get('type') not in ('connected', 'disconnected'):
        return None
    payload = value.get('payload', value)
    if not isinstance(payload, dict):
        payload = {}
    row = {'type': value['type']}
    for key in ('timeMs', 'sequence'):
        row[key] = integer(value.get(key))
    for key in ('reason', 'causeReason'):
        reason = payload.get(key)
        row[key] = reason if isinstance(reason, str) and reason in REASONS else 'other' if isinstance(reason, str) else None
    row['causeTimeMs'] = integer(payload.get('causeTimeMs'))
    row['connectedBeforeDisconnect'] = (payload.get('connectedBeforeDisconnect')
        if type(payload.get('connectedBeforeDisconnect')) is bool else None)
    # Preserve a binding fact without publishing the peer PID.
    row['peerIdentified'] = (payload.get('peerIdentified') if type(payload.get('peerIdentified')) is bool
        else integer(payload.get('pid')) is not None and payload['pid'] > 0)
    return row


def event_history(value):
    if not isinstance(value, dict):
        return {'status': 'unavailable', 'events': [], 'count': None,
                'firstEvent': None, 'firstDisconnected': None, 'truncated': None}
    rows = value.get('events')
    if not isinstance(rows, list):
        return event_history(None)
    count = integer(value.get('count'))
    return {'status': 'observed', 'count': count if count is not None and count >= 0 else None,
        'events': [item for row in rows[-LIMIT:] if (item := event(row)) is not None],
        'firstEvent': event(value.get('firstEvent')),
        'firstDisconnected': event(value.get('firstDisconnected')),
        'malformedEvents': integer(value.get('malformedEvents')),
        'truncated': value.get('truncated') if type(value.get('truncated')) is bool else None}


def observer_script(state):
    """Independent bounded observer; existing functional event arrays stay intact."""
    return '''const root=STATE; const wanted=arguments[0];
const d=root.__failureDiagnostics={events:[],count:0,firstEvent:null,firstDisconnected:null,malformedEvents:0};
const reasons=new Set(REASONS);
d.observer={observe(_s,_t,data){try{const e=JSON.parse(data);
if(e.appId!==wanted||!['connected','disconnected'].includes(e.type))return;
const p=e.payload??{};d.count++;
const row={type:e.type,sequence:d.count,timeMs:Date.now(),
peerIdentified:Number.isSafeInteger(p.pid)&&p.pid>0,
reason:typeof p.reason==='string'?(reasons.has(p.reason)?p.reason:'other'):null,
causeReason:typeof p.causeReason==='string'?(reasons.has(p.causeReason)?p.causeReason:'other'):null,
causeTimeMs:Number.isSafeInteger(p.causeTimeMs)?p.causeTimeMs:null,
connectedBeforeDisconnect:typeof p.connectedBeforeDisconnect==='boolean'?p.connectedBeforeDisconnect:null};
if(!d.firstEvent)d.firstEvent=row;
if(e.type==='disconnected'&&!d.firstDisconnected)d.firstDisconnected=row;
d.events.push(row);if(d.events.length>16)d.events.shift();
}catch(_error){d.malformedEvents++;}}};
Services.obs.addObserver(d.observer,'floorp-web-app-shim-event');
return true;'''.replace('STATE', state).replace('REASONS', json.dumps(sorted(REASONS)))


def monitor_projection(value):
    if not isinstance(value, dict):
        return {'status': 'unavailable', 'events': [], 'identityObserved': None}
    rows = []
    sources = value.get('events')
    for source in (sources[-LIMIT:] if isinstance(sources, list) else []):
        if not isinstance(source, dict) or not isinstance(source.get('kind'), str) or source['kind'] not in MONITOR_EVENTS:
            continue
        row = {'kind': source['kind']}
        for key in ('monotonicNs', 'wallTimeNs', 'exitCode', 'signal', 'errno'):
            if key in source:
                row[key] = integer(source[key])
        for key in ('identityVerified', 'exitStatusAvailable', 'codeSigningKill', 'memoryKill', 'decryptFailure'):
            if type(source.get(key)) is bool:
                row[key] = source[key]
        rows.append(row)
    status = value.get('status')
    return {'status': status if isinstance(status, str) and status in MONITOR_STATUS else 'unavailable', 'events': rows,
        'identityObserved': value['identityObserved'] if type(value.get('identityObserved')) is bool else
            True if integer(value.get('pid')) is not None and value['pid'] > 0 else None,
        'kernelTimestampAvailable': False, 'scope': 'first-private-shim-instance',
        'nativeAuthenticationCompleted': None}


def host_snapshot(host):
    result = {'status': 'not-started', 'identityObserved': False,
        'marionetteOwnershipVerified': False, 'launchServices': None, 'browserLiveness': 'not-observed',
        'browserExitCode': None, 'browserSignal': None, 'browserExitStatusAvailable': None,
        'launcherExitCode': None, 'requestedSignal': None, 'failure': None}
    if host is None:
        return result
    result.update(status='unavailable', identityObserved=host.pid is not None and host.identity is not None,
                  marionetteOwnershipVerified=host.verified is True, launchServices=host.launch_services is True)
    if result['identityObserved']:
        result['browserLiveness'] = 'unavailable'
    try:
        host.poll()  # Consume exit events once; never serialize poll's liveness sentinel.
        code = integer(host.exit_code if host.launch_services else host.process.returncode)
        result.update(status='observed', browserExitCode=code,
            browserSignal=-code if code is not None and code < 0 else None,
            browserExitStatusAvailable=code is not None,
            browserLiveness=('not-observed' if not result['identityObserved'] else
                'exit-observed' if host.exited or code is not None else 'alive'))
    except BaseException as error:
        result['failure'] = fact(error)
    try:
        result['launcherExitCode'] = integer(host.process.poll())
    except BaseException as error:
        result['launcherFailure'] = fact(error)
    requested = host.requested_signal
    result['requestedSignal'] = int(requested) if isinstance(requested, int) and not isinstance(requested, bool) else None
    return result


def project_diagnostics(value):
    """Public collector API. Reproject report['failureDiagnostics']; never copy it."""
    if not isinstance(value, dict) or type(value.get('schema')) is not int or value['schema'] != 1:
        return {'schema': 1, 'available': False}
    def safe_fact(source):
        if not isinstance(source, dict):
            return None
        kind = source.get('kind')
        return {'kind': kind if isinstance(kind, str) and kind in ERROR_KINDS else 'other',
                'errno': integer(source.get('errno'))}
    def functional(source):
        if not isinstance(source, dict):
            return None
        codes = ('shim-authentication-timeout', 'marionette-timeout', 'functional-exception')
        return {**safe_fact(source), 'phase': source.get('phase') if isinstance(source.get('phase'), str) and source['phase'] in PHASES else 'other',
            'code': source.get('code') if source.get('code') in codes else 'functional-exception',
            'observedAtMs': integer(source.get('observedAtMs'))}
    def host(source):
        if not isinstance(source, dict):
            return {'status': 'unavailable'}
        result = {'status': source.get('status') if source.get('status') in
                  ('not-started', 'observed', 'unavailable') else 'unavailable'}
        for key in ('identityObserved', 'marionetteOwnershipVerified', 'launchServices', 'browserExitStatusAvailable'):
            result[key] = source.get(key) if type(source.get(key)) is bool else None
        result['browserLiveness'] = source.get('browserLiveness') if source.get('browserLiveness') in (
            'not-observed', 'alive', 'exit-observed', 'unavailable') else 'unavailable'
        for key in ('browserExitCode', 'browserSignal', 'launcherExitCode', 'requestedSignal'):
            result[key] = integer(source.get(key))
        for key in ('failure', 'launcherFailure'):
            if key in source:
                result[key] = safe_fact(source[key])
        return result
    def snapshot(source):
        if not isinstance(source, dict):
            return None
        result = {'observedAtMs': integer(source.get('observedAtMs')), 'host': host(source.get('host')),
                  'childMonitor': monitor_projection(source.get('childMonitor'))}
        if 'events' in source:
            history = source['events']
            result['events'] = event_history(history if isinstance(history, dict) and
                                            history.get('status') == 'observed' else None)
        if 'firstFunctionalFailure' in source:
            result['firstFunctionalFailure'] = functional(source['firstFunctionalFailure'])
        if 'firstReportedNativeCause' in source:
            result['firstReportedNativeCause'] = event(source['firstReportedNativeCause'])
        return result
    def operation(source):
        if not isinstance(source, dict) or not isinstance(source.get('operation'), str) or source['operation'] not in OPERATIONS:
            return None
        return {'operation': source['operation'], 'completed': source.get('completed') is True,
            'startedAtMs': integer(source.get('startedAtMs')), 'finishedAtMs': integer(source.get('finishedAtMs')),
            'failure': safe_fact(source.get('failure'))}
    operations = value.get('cleanupOperations')
    errors = value.get('diagnosticErrors')
    return {'schema': 1, 'available': True,
        'firstFunctionalFailure': functional(value.get('firstFunctionalFailure')),
        'firstReportedNativeCause': event(value.get('firstReportedNativeCause')),
        'beforeCleanup': snapshot(value.get('beforeCleanup')), 'afterCleanup': snapshot(value.get('afterCleanup')),
        'cleanupCount': integer(value.get('cleanupCount')),
        'cleanupOperations': [row for source in operations[-LIMIT:] if (row := operation(source)) is not None]
            if isinstance(operations, list) else [],
        'firstCleanupFailure': operation(value.get('firstCleanupFailure')),
        'diagnosticErrors': [{'operation': source.get('operation') if isinstance(source.get('operation'), str)
            and source['operation'] in DIAGNOSTIC_OPERATIONS else 'diagnostic-collection', **safe_fact(source)}
            for source in errors[-LIMIT:] if isinstance(source, dict)] if isinstance(errors, list) else [],
        'diagnosticsIncomplete': value.get('diagnosticsIncomplete') if type(value.get('diagnosticsIncomplete')) is bool else None,
        'timeMeaning': 'local-observation-times-not-kernel-timestamps',
        'causeInterpretation': 'reported-native-reason-not-established-root-cause',
        'rawProcessIdentitiesPublished': False, 'rawExceptionTextPublished': False}


class Recorder:
    def __init__(self, output):
        self.output = Path(output)
        self.monitor = None
        self.value = {'schema': 1, 'firstFunctionalFailure': None, 'firstReportedNativeCause': None,
            'beforeCleanup': None, 'afterCleanup': None, 'cleanupOperations': [],
            'cleanupCount': 0, 'firstCleanupFailure': None,
            'diagnosticErrors': [], 'diagnosticsIncomplete': False,
            'timeMeaning': 'local-observation-times-not-kernel-timestamps',
            'causeInterpretation': 'reported-native-reason-not-established-root-cause',
            'rawProcessIdentitiesPublished': False, 'rawExceptionTextPublished': False}

    def diagnostic_error(self, operation, error):
        self.value['diagnosticsIncomplete'] = True
        self.value['diagnosticErrors'].append({'operation': operation, **fact(error)})
        self.value['diagnosticErrors'] = self.value['diagnosticErrors'][-LIMIT:]

    def functional_failure(self, phase, error):
        if self.value['firstFunctionalFailure'] is None:
            code = {'Timed out waiting for Shim authentication': 'shim-authentication-timeout',
                'Timed out waiting for authenticated Shim': 'shim-authentication-timeout',
                'Timed out waiting for Marionette': 'marionette-timeout'}.get(str(error), 'functional-exception')
            self.value['firstFunctionalFailure'] = {'phase': phase if phase in PHASES else 'other',
                'code': code, 'observedAtMs': time.time_ns()//1000000, **fact(error)}

    def install_observer(self, client, state, app_id):
        client.script(observer_script(state), app_id)

    def start_monitor(self, executable, host, process_rows):
        try:
            path = Path(__file__).with_name('shim_lifecycle.py')
            spec = importlib.util.spec_from_file_location('failure_child_lifecycle', path)
            module = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(module)
            self.monitor = module.ShimMonitor(executable, host.pid, process_rows)
            self.monitor.start()
        except BaseException as error:
            self.diagnostic_error('child-monitor-start', error)

    def _events(self, client, state):
        if client is None:
            return event_history(None)
        previous = None
        changed = False
        try:
            # Avoid extending the original failure by the normal 60s RPC timeout.
            previous = client.socket.gettimeout()
            client.socket.settimeout(min(previous, 3) if previous is not None else 3)
            changed = True
            client.context('chrome')
            value = client.script('''const d=STATE?.__failureDiagnostics;
return d?{events:d.events,count:d.count,firstEvent:d.firstEvent,
firstDisconnected:d.firstDisconnected,malformedEvents:d.malformedEvents,truncated:d.count>16}:null;
'''.replace('STATE', state))
            return event_history(value)
        except BaseException as error:
            self.diagnostic_error('browser-event-snapshot', error)
            return event_history(None)
        finally:
            if changed:
                try:
                    client.socket.settimeout(previous)
                except BaseException as error:
                    self.diagnostic_error('restore-rpc-timeout', error)

    def _monitor(self):
        if self.monitor is None:
            return {'status': 'not-started', 'identityObserved': None, 'events': []}
        try:
            return monitor_projection(self.monitor.snapshot())
        except BaseException as error:
            self.diagnostic_error('child-monitor-snapshot', error)
            return monitor_projection(None)

    def _host(self, host):
        try:
            result = host_snapshot(host)
            if result.get('failure') or result.get('launcherFailure'):
                self.value['diagnosticsIncomplete'] = True
            return result
        except BaseException as error:
            self.diagnostic_error('host-snapshot', error)
            return {'status': 'unavailable', 'failure': fact(error)}

    def persist(self, name, value):
        try:
            descriptor = os.open(self.output/name, os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600)
            with os.fdopen(descriptor, 'w') as stream:
                json.dump(value, stream, sort_keys=True)
                stream.write('\n')
                stream.flush()
                os.fsync(stream.fileno())
        except BaseException as error:
            self.diagnostic_error('write-'+name, error)

    def before_cleanup(self, host, client, state):
        if self.value['beforeCleanup'] is not None:
            return
        snapshot = {'observedAtMs': time.time_ns()//1000000, 'host': self._host(host),
                    'events': self._events(client, state), 'childMonitor': self._monitor(),
                    'firstFunctionalFailure': copy.deepcopy(self.value['firstFunctionalFailure'])}
        self.value['beforeCleanup'] = snapshot
        first = snapshot['events'].get('firstDisconnected')
        if first is not None:
            self.value['firstReportedNativeCause'] = copy.deepcopy(first)
        snapshot['firstReportedNativeCause'] = copy.deepcopy(self.value['firstReportedNativeCause'])
        self.persist('native-before-cleanup.json', snapshot)

    def cleanup(self, operation, action):
        if operation not in OPERATIONS:
            raise ValueError('unrecognized-cleanup-operation')
        row = {'operation': operation, 'startedAtMs': time.time_ns()//1000000,
               'completed': False, 'failure': None}
        self.value['cleanupOperations'].append(row)
        self.value['cleanupOperations'] = self.value['cleanupOperations'][-LIMIT:]
        self.value['cleanupCount'] += 1
        try:
            action()
            row['completed'] = True
        except BaseException as error:
            row['failure'] = fact(error)
        row['finishedAtMs'] = time.time_ns()//1000000
        if row['failure'] and self.value['firstCleanupFailure'] is None:
            self.value['firstCleanupFailure'] = copy.deepcopy(row)
        return row['completed']

    def after_cleanup(self, host):
        self.value['afterCleanup'] = {'observedAtMs': time.time_ns()//1000000,
            'host': self._host(host), 'childMonitor': self._monitor()}
        self.persist('native-after-cleanup.json', self.value['afterCleanup'])

    def summary(self):
        return project_diagnostics(self.value)

    def failed(self):
        return self.value['diagnosticsIncomplete'] or self.value['firstCleanupFailure'] is not None
