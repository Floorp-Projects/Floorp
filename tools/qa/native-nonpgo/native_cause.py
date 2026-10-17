# SPDX-License-Identifier: MPL-2.0
import argparse
import importlib.util
import json
from pathlib import Path
import plistlib
import sys
import time
import uuid
from support import Failure, require, write_json


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
        report['errorCode'] = str(error) if isinstance(error, Failure) else type(error).__name__
        report['status'] = 'failed'
    finally:
        errors = []
        if client and host and host.verified and host.poll() is None:
            try:
                client.context('chrome')
                client.script(f"Services.obs.removeObserver({state}.observer,'floorp-web-app-shim-event');{h.SERVICE}.stop();")
            except Exception as error:
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
        report['cleanupComplete'] = not errors and host is not None and host.poll() is not None
        if errors or not report['cleanupComplete']:
            report['cleanupErrors'] = errors
            report['status'] = 'failed'
        write_json(output / 'result.json', report)
    return 0 if report['status'] == 'passed' else 1


if __name__ == '__main__':
    raise SystemExit(main())
