# SPDX-License-Identifier: MPL-2.0
import argparse
import importlib.util
import json
import os
from pathlib import Path
import plistlib
import re
import sys
import time
import uuid
from support import Failure, require, write_json


STREAM_MARKERS = {
    'Library not loaded:': 'loader-library-not-loaded',
    'Bad CPU type in executable': 'loader-unsupported-cpu',
    'Code Signature Invalid': 'invalid-code-signature',
    '###!!! ASSERTION:': 'gecko-assertion',
    'Assertion failure:': 'gecko-fatal-assertion',
    'MOZ_CRASH': 'gecko-crash-marker',
    'Marionette enabled': 'marionette-enabled',
    'Listening on port': 'listener-started',
    'Error Domain=NSOSStatusErrorDomain': 'launch-services-osstatus',
    'kLSNoLaunchPermissionErr': 'launch-services-permission',
    'kLSNoExecutableErr': 'launch-services-no-executable',
    'kLSExecutableIncorrectFormat': 'launch-services-executable-format',
}


def exit_snapshot(host):
    code = host.exit_code if type(host.exit_code) is int else None
    launcher = host.process.poll()
    requested = host.requested_signal
    return {'browserExitCode': code, 'browserSignal': -code if code is not None and code < 0 else None,
            'browserExitStatusAvailable': code is not None,
            'launcherExitCode': launcher if type(launcher) is int else None,
            'requestedSignal': int(requested) if isinstance(requested, int) and not isinstance(requested, bool) else None}


def startup_snapshot(host, output):
    snapshot = {'observedAtMs': int(time.time() * 1000), 'hostStartup': {'started': host is not None},
                'streams': {}}
    if host is not None:
        host.poll()
        observed = host.pid is not None and host.identity is not None
        # This probe always uses LaunchServices. Read the exit code already
        # consumed by poll; returncode would consume a later event after the
        # liveness observation and could make the two fields contradictory.
        exits = exit_snapshot(host)
        snapshot['hostStartup'].update({
            'browserIdentityObserved': observed, 'marionetteOwnershipVerified': host.verified,
            'browserLiveness': ('not-observed' if not observed else
                                'exit-observed' if host.exited else 'alive'),
            **exits,
            'launchServices': host.launch_services,
            'configuredBrowserSwitches': [flag for flag in host.command if flag in
                ('--no-remote', '--profile', '--marionette', '--remote-allow-system-access')],
            'configuredAboutBlank': host.command[-1] == 'about:blank',
            'browserArgvObservedMatches': True if observed and not host.exited else None})
    snapshot['nativeProbeEnvironment'] = {name: name in os.environ for name in
        ('GITHUB_TOKEN', 'SDK_READ_TOKEN', 'ACTIONS_RUNTIME_TOKEN', 'MOZ_DISABLE_CONTENT_SANDBOX',
         'DYLD_LIBRARY_PATH', 'DYLD_INSERT_LIBRARIES')}
    snapshot['applicationEnvironmentBeforeMarionette'] = 'UNVERIFIED_LAUNCHSERVICES_ENVIRONMENT'
    for label, name in (('launcher', 'browser.private.log'), ('stdout', 'browser.stdout.log'),
                         ('stderr', 'browser.stderr.log')):
        path = output / name
        record = {'present': path.is_file(), 'rawContentPublished': False, 'markers': []}
        if record['present']:
            with path.open('rb') as stream:
                size = stream.seek(0, 2)
                stream.seek(0)
                head = stream.read(32768)
                offset = max(len(head), size - 32768)
                stream.seek(offset)
                tail = stream.read(32768)
            head_count, tail_count = len(head), len(tail)
            if offset > len(head):
                head = head.rsplit(b'\n', 1)[0] if b'\n' in head else b''
                tail = tail.split(b'\n', 1)[1] if b'\n' in tail else b''
            separator = b'\n' if offset > head_count else b''
            text = (head + separator + tail).decode('utf-8', errors='replace')
            codes = list(dict.fromkeys(int(code) for code in re.findall(
                r'Error Domain=NSOSStatusErrorDomain Code=(-?[0-9]{1,10})(?![0-9])', text)
                if -(2**31) <= int(code) < 2**31))[:8]
            record.update({'bytesObserved': size, 'headBytesRead': head_count, 'tailBytesRead': tail_count,
                           'tailTruncated': offset > head_count, 'osStatusCodes': codes,
                           'markers': sorted({value for key, value in STREAM_MARKERS.items() if key in text})})
        snapshot['streams'][label] = record
    return snapshot


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--browser', required=True, type=Path)
    parser.add_argument('--harness', required=True, type=Path)
    parser.add_argument('--output', required=True, type=Path)
    parser.add_argument('--build-id', required=True)
    args = parser.parse_args()
    h = load_module('runtime', args.harness / 'tools/app-shim/test-runtime.py')
    lifecycle = load_module('lifecycle', args.harness / 'tools/app-shim/shim_lifecycle.py')
    output = args.output
    output.mkdir(mode=0o700, exist_ok=False)
    profile = output / 'profile'
    profile.mkdir(mode=0o700)
    port = h.unused_port()
    preferences = {'marionette.port': port, 'browser.shell.checkDefaultBrowser': False,
                   'browser.startup.page': 0, 'browser.startup.homepage': 'about:blank',
                   'browser.aboutwelcome.enabled': False, 'app.update.enabled': False,
                   'datareporting.policy.dataSubmissionEnabled': False}
    (profile / 'user.js').write_text('\n'.join('user_pref(' + json.dumps(k) + ', ' +
                                    json.dumps(v) + ');' for k, v in preferences.items()) + '\n')
    state = 'Services.appShell.hiddenDOMWindow.__qaFirstCause'
    app_id, profile_id = '{' + str(uuid.uuid4()) + '}', 'first-cause-' + uuid.uuid4().hex
    binary = args.browser / 'Contents/MacOS' / plistlib.loads(
        (args.browser / 'Contents/Info.plist').read_bytes())['CFBundleExecutable']
    report = {'status': 'failed', 'phase': 'host', 'controlledScenario': 'cancel-connected-launch',
              'originalTimeoutCause': 'UNKNOWN', 'cleanupComplete': False}
    host = client = monitor = None
    log = (output / 'browser.private.log').open('w')
    try:
        host = h.HostProcess(args.browser, binary, profile, output, log, launch_services=True)
        def connect():
            require(host.poll() is None, 'cause-host-exited')
            try:
                return h.Marionette(port)
            except OSError:
                return None
        client = h.wait_for('Marionette', connect, 35)
        host.verify_session(client.session)
        client.context('chrome')
        protections = client.script('''const {AppConstants}=ChromeUtils.importESModule('resource://gre/modules/AppConstants.sys.mjs');
return {debug:AppConstants.MOZ_DEBUG,buildID:Services.appinfo.appBuildID,version:Services.appinfo.version,
os:Services.appinfo.OS,is64Bit:Services.appinfo.is64Bit,level:Services.prefs.getIntPref('security.sandbox.content.level',-1),
userOverride:Services.prefs.prefHasUserValue('security.sandbox.content.level'),
disableEnv:Services.env.get('MOZ_DISABLE_CONTENT_SANDBOX')};''')
        require(protections['debug'] is True and protections['level'] == 3 and
                protections['userOverride'] is False and protections['disableEnv'] == '', 'runtime-protections')
        require(protections['buildID'] == args.build_id and protections['version'] == '158.0.1' and
                protections['os'] == 'Darwin' and protections['is64Bit'] is True, 'runtime-build-identity')
        report.update({'browserCompiledDebug': True, 'contentSandboxLevel': 3,
                       'contentSandboxUserOverride': False, 'contentSandboxDisableEnvEmpty': True,
                       'browserBuildID': protections['buildID'], 'browserVersion': protections['version']})
        identity = json.loads(client.script('return ' + h.SERVICE + '.hostIdentityJSON;'))
        bundle = h.seal_bundle(output, args.browser / 'Contents/MacOS/floorp-app-shim',
                               identity, profile, app_id, profile_id)
        monitor = lifecycle.ShimMonitor(bundle / 'Contents/MacOS/floorp-app-shim',
                                        host.pid, h.HostProcess._process_rows)
        monitor.start()
        client.script(f'''const wanted=arguments[0];{state}={{events:[],count:0}};
{state}.observer={{observe(s,t,d){{const e=JSON.parse(d);if(e.appId!==wanted)return;
if(e.type==='connected'||e.type==='disconnected'){{
const p=e.payload??{{}};{state}.count++;{state}.events.push({{type:e.type,time:Date.now(),payload:{{
pid:Number.isSafeInteger(p.pid)?p.pid:null,reason:typeof p.reason==='string'?p.reason.slice(0,64):null,
causeReason:typeof p.causeReason==='string'?p.causeReason.slice(0,64):null,
causeTimeMs:Number.isSafeInteger(p.causeTimeMs)?p.causeTimeMs:null,
connectedBeforeDisconnect:typeof p.connectedBeforeDisconnect==='boolean'?p.connectedBeforeDisconnect:null}}}});
if({state}.events.length>64){state}.events.shift();}}}}}};
Services.obs.addObserver({state}.observer,'floorp-web-app-shim-event');
{h.SERVICE}.configure(arguments[1]);{h.SERVICE}.registerApp(arguments[0],arguments[2]);
{h.SERVICE}.launchApp(arguments[0]);''', app_id, profile_id, str(bundle))
        report['phase'] = 'authenticate'
        event = h.wait_for('Shim authentication', lambda: next((e for e in client.script(
            f'return {state}.events;') if e['type'] == 'connected'), None), 20)
        pid = event['payload']['pid']
        require(type(pid) is int and pid > 0, 'cause-auth-pid')
        h.wait_for('owned kernel monitor', lambda: monitor.status == 'monitoring', 5)
        require(monitor.pid == pid, 'cause-kernel-auth-binding')
        report['phase'] = 'cancel-connected'
        before = client.script('return Date.now();')
        client.script(h.SERVICE + '.cancelLaunch(arguments[0]);', app_id)
        disconnected = h.wait_for('controlled disconnect', lambda: next((e for e in client.script(
            f'return {state}.events;') if e['type'] == 'disconnected'), None), 20)
        value = disconnected['payload']
        require(value.get('reason') in ('launch-failed', 'process-exited', 'process-monitor-failed', 'send-failed',
                'not-running', 'connection-generation-exhausted', 'window-close-timeout', 'hello-timeout',
                'launch-cancelled', 'service-stopped', 'window-retirement-limit'), 'terminal-reason')
        require(client.script(f'return {state}.count;') <= 64, 'controlled-events-truncated')
        require(value.get('causeReason') == 'launch-cancelled' and
                value.get('connectedBeforeDisconnect') is True and value.get('pid') == pid,
                'first-cause-payload')
        stamp = value.get('causeTimeMs')
        require(type(stamp) is int and before <= stamp <= disconnected['time'], 'first-cause-time')
        h.wait_for('owned Shim exit', lambda: monitor.status == 'exit-observed', 15)
        report.update({'phase': 'complete', 'status': 'passed', 'pid': pid,
                       'causeReason': 'launch-cancelled', 'causeTimeMs': stamp,
                       'terminalReason': value['reason'],
                       'connectedBeforeDisconnect': True, 'kernelExitObserved': True,
                       'hostOwnershipVerified': host.verified})
    except BaseException as error:
        timeouts = {'Marionette': 'cause-marionette-timeout', 'Shim authentication': 'cause-shim-auth-timeout',
                    'owned kernel monitor': 'cause-kernel-monitor-timeout',
                    'controlled disconnect': 'cause-controlled-disconnect-timeout',
                    'owned Shim exit': 'cause-owned-shim-exit-timeout'}
        report['errorCode'] = str(error) if isinstance(error, Failure) else next(
            (code for name, code in timeouts.items() if str(error) == 'Timed out waiting for ' + name),
            type(error).__name__)
        if isinstance(error, OSError) and type(error.errno) is int:
            report['errorErrno'] = error.errno
        report['status'] = 'failed'
    finally:
        errors = []
        try:
            log.flush()
            report['beforeCleanup'] = startup_snapshot(host, output)
            observed = report['beforeCleanup']['hostStartup']
            if report['status'] == 'passed' and not (
                    observed.get('browserLiveness') == 'alive' and
                    observed.get('marionetteOwnershipVerified') is True and
                    observed.get('browserExitCode') is None and
                    observed.get('requestedSignal') is None):
                report['status'] = 'failed'
                report['errorCode'] = 'cause-host-not-alive-before-cleanup'
        except BaseException as error:
            report['diagnosticsIncomplete'] = True
            report['snapshotErrorType'] = type(error).__name__
            report['status'] = 'failed'
            report.setdefault('errorCode', 'cause-startup-snapshot-unavailable')
        try:
            if client and host and host.verified and host.poll() is None:
                client.context('chrome')
                client.script(f"Services.obs.removeObserver({state}.observer,'floorp-web-app-shim-event');{h.SERVICE}.stop();")
        except BaseException as error:
            errors.append(type(error).__name__)
        for value, method in ((client, 'close'), (host, 'close'), (monitor, 'close'), (log, 'close')):
            if value:
                try:
                    if value is client and (not host or not host.verified):
                        client.socket.close()
                    else:
                        getattr(value, method)()
                except BaseException as error:
                    errors.append(type(error).__name__)
        try:
            report['cleanupComplete'] = (not errors and host is not None and
                                         host.pid is not None and host.identity is not None and
                                         host.poll() is not None)
            if host is not None:
                report['afterCleanupHost'] = exit_snapshot(host)
                terminal = report['afterCleanupHost']
                code, requested = terminal['browserExitCode'], terminal['requestedSignal']
                if code not in (None, 0, -(requested or 0)):
                    report['status'] = 'failed'
                    report.setdefault('errorCode', 'cause-host-abnormal-exit-during-cleanup')
        except BaseException as error:
            errors.append(type(error).__name__)
            report['cleanupComplete'] = False
        if errors or not report['cleanupComplete']:
            report['cleanupErrors'] = errors
            report['status'] = 'failed'
            report.setdefault('errorCode', 'cause-cleanup-incomplete')
        write_json(output / 'result.json', report)
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
