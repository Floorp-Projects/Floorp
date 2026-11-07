# SPDX-License-Identifier: MPL-2.0
"""Bound one ordinary feles-build test run; never issue final qualification."""
import configparser
import hashlib
import json
import os
from pathlib import Path
import re
import selectors
import shutil
import signal
import subprocess
import time
from urllib.parse import unquote

ROOT = Path(__file__).resolve().parents[2]
TARGET = "browser-features/chrome/common/workspaces/test/nativeLastTabWorkspacesEnabled.test.ts"
NAMES = [f"{kind} tab retains native behavior (workspaces true, native {close})" for close in ["false", "true"] for kind in ["extension", "unattributed", "collapsed-group"]]
LIMIT = 8 * 1024 * 1024

def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def case_events(text):
    return [json.loads(unquote(m[1])) for m in re.finditer(r"\[nora@workspace-case\] (%7B[^\s\"]+)", text)]

def cases(text):
    events = case_events(text)
    if len(events) != 12:
        raise ValueError(f"Required 12 original case events, observed {len(events)}")
    for i, name in enumerate(NAMES):
        for event, status in zip(events[i * 2:i * 2 + 2], ["running", "passed"]):
            if event.get("moduleName") != Path(TARGET).name or event.get("testName") != name or event.get("index") != i + 1 or event.get("total") != 6 or event.get("status") != status or event.get("error"):
                raise ValueError("Original six-case identity, failure or missing/skip rejected")
    return events

def process_table():
    result = {}
    for path in Path('/proc').glob('[0-9]*/stat'):
        try:
            data = path.read_text(); fields = data[data.rfind(')') + 2:].split()
            result[int(path.parent.name)] = {'start': fields[19], 'ppid': int(fields[1]), 'group': int(fields[2]), 'session': int(fields[3]), 'state': fields[0]}
        except (FileNotFoundError, ProcessLookupError):
            continue
    return result

def adopt_owned(table, owned):
    active = {pid for pid,start in owned.items() if pid in table and table[pid]['start']==start and table[pid]['state']!='Z'}
    sessions = {table[pid]['session'] for pid in active}
    changed = True
    while changed:
        changed = False
        for pid,row in table.items():
            if pid in owned and owned[pid] != row['start']: continue
            if pid not in active and row['state']!='Z' and (row['ppid'] in active or row['session'] in sessions):
                active.add(pid); owned[pid]=row['start']; sessions.add(row['session']); changed=True
    return active

def main():
    output = ROOT / '_dist/workspace-development'
    output.mkdir(mode=0o700)
    profile = ROOT / '_dist/profile/test'
    if profile.exists() or profile.is_symlink():
        raise ValueError('Fresh synthetic profile is required')
    profile.mkdir(mode=0o700, parents=True)
    if os.environ.get('GITHUB_RUN_ATTEMPT') != '1':
        raise ValueError('Original development attempt 1 required')
    head = subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip()
    if head != os.environ['GITHUB_SHA']:
        raise ValueError('Source/test revision differs from original controller')
    lock = json.loads((ROOT / 'floorp-runtime.lock.json').read_text())
    receipt = {'schema': 'qa3-workspace-development-v1', 'status': 'FAIL_DEVELOPMENT_ONLY', 'scope': 'ORDINARY_DEVELOPMENT_E2E', 'floorpHead': head, 'run': int(os.environ['GITHUB_RUN_ID']), 'attempt': 1, 'runtimeSource': lock['source']['commit'], 'runtimeLockSha256': digest(ROOT / 'floorp-runtime.lock.json'), 'testSha256': digest(ROOT / TARGET), 'harnessSha256': digest(ROOT / 'browser-features/chrome/test/utils/test_harness.ts'), 'nativeProductQualification': 'UNVERIFIED', 'compileProfileQualification': 'UNVERIFIED', 'publicationAuthorized': False, 'originalError': None, 'cleanupError': None}
    ini = configparser.ConfigParser(); ini.read(ROOT / '_dist/bin/floorp/application.ini')
    artifact = next(x for x in lock['artifacts'] if x['platform']=='linux' and x['architecture']=='x86_64')
    receipt['runtimeArtifact'] = artifact['asset']
    if ini['App']['BuildID'] != artifact['buildId']:
        raise ValueError('Locked Runtime BuildID differs')
    receipt['runtimeBuildID'] = ini['App']['BuildID']
    receipt['binaryRelativePath'] = '_dist/bin/floorp/floorp-bin'
    receipt['binarySha256BeforeInjection'] = digest(ROOT / receipt['binaryRelativePath'])
    home = output / 'home'; home.mkdir(mode=0o700)
    env = {key: os.environ[key] for key in ['PATH', 'LANG', 'DENO_DIR', 'CI', 'GITHUB_ACTIONS'] if key in os.environ}
    env.update(HOME=str(home), TMPDIR=str(home), DISPLAY=':99', DENO_DIR=os.environ.get('DENO_DIR', str(Path.home() / '.cache/deno')))
    owned = {}; processes = []; selector = selectors.DefaultSelector(); logs = {}
    deadline = time.monotonic() + 900
    cleaning = False
    cancellation = None
    def cancel(signum, _frame):
        nonlocal cancellation
        cancellation = cancellation or ('Original development cancelled by signal '+str(signum))
        if not cleaning: raise InterruptedError(cancellation)
    previous = {s: signal.signal(s,cancel) for s in [signal.SIGTERM,signal.SIGINT]}
    def capture():
        table = process_table()
        adopt_owned(table,owned)
        return table
    def spawn(name, args):
        p = subprocess.Popen(args, cwd=ROOT, env=env, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, start_new_session=True)
        processes.append(p); table = process_table(); owned[p.pid] = table[p.pid]['start']
        f = (output / (name + '.log')).open('xb'); logs[name] = f
        selector.register(p.stdout, selectors.EVENT_READ, (name, f)); return p
    def pump():
        capture()
        if time.monotonic() > deadline: raise TimeoutError('Original development wall deadline')
        for key, _ in selector.select(0.1):
            data = os.read(key.fileobj.fileno(), 65536)
            if not data: selector.unregister(key.fileobj); key.fileobj.close(); continue
            name, f = key.data
            if f.tell() + len(data) > LIMIT: raise ValueError('Original bounded log overflow: ' + name)
            f.write(data); f.flush()
    try:
        if Path('/tmp/.X11-unix/X99').exists(): raise ValueError('Development display is not fresh')
        spawn('xvfb', ['Xvfb', ':99', '-screen', '0', '1280x1024x24'])
        display_deadline = time.monotonic() + 10
        while not Path('/tmp/.X11-unix/X99').exists():
            pump()
            if time.monotonic() > display_deadline: raise TimeoutError('Original display startup')
        collector = spawn('collector', ['deno', 'task', 'test', '--layer', 'chrome', '--near', TARGET, '--timeout-ms', '300000', '--startup-timeout-ms', '300000', '--no-autostart'])
        control = profile / 'nora-tests-control.json'
        while not control.exists():
            pump()
            if collector.poll() is not None: raise ValueError('Original collector exited before control')
        original_control = json.loads(control.read_text())
        if original_control.get('schemaVersion') != 1 or original_control.get('filter') != [TARGET] or not original_control.get('runId') or original_control.get('expiresAtMs',0) <= time.time() * 1000:
            raise ValueError('Control selection differs from the single target')
        receipt['control'] = original_control
        (output / 'original-control.json').write_bytes(control.read_bytes())
        browser = spawn('feles', ['deno', 'task', 'feles-build', 'test'])
        while collector.poll() is None:
            pump()
            if browser.poll() is not None: raise ValueError('Original feles test process exited')
        for _ in range(5): pump()
        if collector.returncode != 0: raise ValueError('Original browser collector failed')
        data = (output / 'collector.log').read_text()
        if f'Found 1 browser test file(s)' not in data or 'Browser test result: 1 passed, 0 failed' not in data or 'skipped' in data.lower():
            raise ValueError('Missing, failed or skipped selected test file')
        receipt['cases'] = cases((output / 'feles.log').read_text())
        receipt['overlayBuildID2'] = (ROOT / '_dist/buildid2').read_text().strip() if (ROOT / '_dist/buildid2').is_file() else None
        receipt['status'] = 'PASS_DEVELOPMENT_ONLY'
    except Exception as error:
        receipt['originalError'] = str(error)
    finally:
        cleaning = True
        try:
            table = capture()
            for sig in [signal.SIGTERM, signal.SIGKILL]:
                for pid, start in owned.items():
                    if pid in table and table[pid]['start'] == start and table[pid]['state'] != 'Z':
                        try: os.kill(pid, sig)
                        except ProcessLookupError: pass
                end = time.monotonic() + (5 if sig == signal.SIGTERM else 2)
                while time.monotonic() < end:
                    for p in processes: p.poll()
                    time.sleep(0.1)
                table = capture()
            survivors = [pid for pid,start in owned.items() if pid in table and table[pid]['start']==start and table[pid]['state']!='Z']
            if survivors: raise ValueError('Owned processes survived cleanup: '+str(survivors))
            receipt['ownedCleanupComplete'] = True
        except Exception as error:
            receipt['cleanupError'] = str(error); receipt['status'] = 'FAIL_DEVELOPMENT_ONLY'
        for f in logs.values(): f.close()
        if (output / 'feles.log').exists():
            try:
                receipt['observedCaseEvents'] = case_events((output / 'feles.log').read_text())
                first = next((e for e in receipt['observedCaseEvents'] if e.get('status')=='failed'),None)
                if first:
                    receipt['controllerError'] = receipt['originalError']
                    receipt['originalError'] = first.get('error') or 'Original case failed'
                    receipt['firstFailedCase'] = first
                    receipt['status'] = 'FAIL_DEVELOPMENT_ONLY'
            except Exception as error:
                receipt['originalError'] = receipt['originalError'] or str(error)
                receipt['status'] = 'FAIL_DEVELOPMENT_ONLY'
        prefs = profile / 'prefs.js'
        if prefs.exists() and prefs.stat().st_size <= LIMIT: shutil.copyfile(prefs, output / 'original-prefs.js')
        if receipt.get('ownedCleanupComplete'):
            try:
                shutil.rmtree(profile)
                receipt['syntheticProfileRemoved'] = True
            except Exception as error:
                receipt['cleanupError'] = receipt['cleanupError'] or str(error)
                receipt['status'] = 'FAIL_DEVELOPMENT_ONLY'
        for path in output.glob('*.log'): receipt.setdefault('logs', {})[path.name] = {'bytes':path.stat().st_size,'sha256':digest(path)}
        if cancellation:
            receipt['originalError'] = receipt['originalError'] or cancellation
            receipt['status'] = 'FAIL_DEVELOPMENT_ONLY'
        (output / 'receipt.json').write_text(json.dumps(receipt, indent=2)+'\n')
        for signum, handler in previous.items(): signal.signal(signum,handler)
    if receipt['status'] != 'PASS_DEVELOPMENT_ONLY': raise ValueError('Original development run failed; evidence retained')
    print(json.dumps(receipt))

if __name__ == '__main__': main()
