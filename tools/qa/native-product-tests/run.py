#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
"""Invoke Mozilla's native harnesses against a finalized, test-enabled product."""
import argparse
import ast
import configparser
import hashlib
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import tomllib
import copy
import zipfile
from datetime import datetime, timezone


def digest(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as f:
        for block in iter(lambda: f.read(1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def module_inventory(root):
    root = Path(root)
    if not root.is_dir():
        raise ValueError(f'Missing testing modules: {root}')
    files = {str(p.relative_to(root)): digest(p) for p in sorted(root.rglob('*')) if p.is_file()}
    if not files:
        raise ValueError('Empty testing module directory')
    return files


def app_identity(path):
    ini = configparser.ConfigParser(interpolation=None)
    ini.read(path)
    return {key: ini['App'][key] for key in ('Name', 'Version', 'BuildID', 'ID')}


def source_dir(objdir):
    # Read only the literal assignment; never execute config.status.
    tree = ast.parse((objdir / 'config.status').read_text())
    for node in tree.body:
        if isinstance(node, ast.Assign) and any(isinstance(t, ast.Name) and t.id == 'topsrcdir' for t in node.targets):
            return Path(ast.literal_eval(node.value)).resolve()
    raise ValueError('config.status does not identify topsrcdir')


def chunk_p0(config, runtime, size):
    """Select original manifest entries; native resolver still applies metadata/skips."""
    if size < 1:
        raise ValueError('Chunk size must be positive')
    config = copy.deepcopy(config)
    expanded = []
    for suite in config['suites']:
        if suite['id'] != 'p0-tabs':
            expanded.append(suite)
            continue
        base = runtime / suite['path']
        entries = tomllib.loads((base / 'browser.toml').read_text())
        names = [name for name in entries if name != 'DEFAULT']
        if not names or any(not name.startswith('browser_') or not name.endswith('.js') or '/' in name or '\\' in name for name in names):
            raise ValueError('Unexpected P0 manifest entry; refusing lossy expansion')
        for start in range(0, len(names), size):
            part = copy.deepcopy(suite)
            part['id'] = f'p0-tabs-chunk-{start // size + 1:03d}'
            part['selection_group'] = 'p0-tabs'
            part['test_paths'] = [str(Path(suite['path']) / name) for name in names[start:start + size]]
            part['registered_files'] = len(part['test_paths'])
            part['source_registered_files'] = len(names)
            expanded.append(part)
    config['suites'] = expanded
    return config


def preflight(args, config):
    if source_dir(args.objdir) != args.runtime:
        raise ValueError('objdir belongs to a different Runtime checkout')
    recorded = json.loads((args.objdir / '.mozconfig.json').read_text())
    if Path(recorded['topsrcdir']).resolve() != args.runtime or Path(recorded['topobjdir']).resolve() != args.objdir:
        raise ValueError('Recorded mozconfig belongs to a different source/object directory')
    if not (args.objdir / '.mozconfig').is_file():
        raise ValueError('Missing recorded objdir .mozconfig snapshot')
    info = json.loads((args.objdir / 'mozinfo.json').read_text())
    if not info.get('tests_enabled') or info.get('artifact'):
        raise ValueError('A native tests-enabled build is required, not an artifact-only build')
    if not args.binary.is_file() or not os.access(args.binary, os.X_OK):
        raise ValueError('Product executable is missing or not executable')
    expected_resources = args.binary.parent.parent / 'Resources' if args.binary.parent.name == 'MacOS' else args.binary.parent
    if args.resources != expected_resources.resolve():
        raise ValueError('Resources directory is not part of the selected product')
    if not args.modules.is_relative_to(args.resources):
        raise ValueError('Testing modules must be present inside the selected product resources')
    expected = app_identity(args.objdir / 'dist/bin/application.ini')
    actual = app_identity(args.resources / 'application.ini')
    if actual['Name'].lower() != 'floorp':
        raise ValueError('Selected product is not branded Floorp')
    buildid2 = args.resources / 'browser/buildid2'
    if not buildid2.is_file() or not buildid2.read_text().strip():
        raise ValueError('Missing Floorp frontend buildid2; finalize product packaging first')
    with zipfile.ZipFile(args.resources / 'omni.ja') as archive:
        if not any('noraneko' in name or 'nora-content' in name for name in archive.namelist()):
            raise ValueError('Missing packaged Floorp frontend assets')
    if expected != actual:
        raise ValueError(f'Product and objdir identities differ: {actual} != {expected}')
    modules = module_inventory(args.modules)
    if modules != module_inventory(args.objdir / '_tests/modules'):
        raise ValueError('Product testing modules do not exactly match this objdir')
    if not (args.runtime / 'mach').is_file():
        raise ValueError('Missing Runtime mach')
    for suite in config['suites']:
        if any(Path(p).is_absolute() for p in [suite['path'], *suite.get('test_paths', [])]):
            raise ValueError('Harness test selections must be source-relative, never absolute')
        path = (args.runtime / suite['path']).resolve()
        if not path.is_relative_to(args.runtime) or not path.exists():
            raise ValueError(f'Invalid or missing suite path: {path}')
        if 'test_paths' in suite:
            if suite['kind'] != 'browser' or not path.is_dir() or not suite['test_paths']:
                raise ValueError('File selection requires a nonempty browser suite directory')
            registered = set()
            for manifest in suite['manifests']:
                registered.update(str((path / name).resolve()) for name in tomllib.loads((path / manifest).read_text()) if name != 'DEFAULT')
            selected = [(args.runtime / name).resolve() for name in suite['test_paths']]
            if len(set(selected)) != len(selected) or len(selected) != suite['registered_files']:
                raise ValueError('Duplicate selected tests or incorrect registered count')
            if any(not p.is_relative_to(path) or not p.is_file() or str(p) not in registered for p in selected):
                raise ValueError('Selected test must belong to the original suite manifest and directory')
        for manifest in suite['manifests']:
            if not (path / manifest).is_file():
                raise ValueError(f'Missing manifest: {path / manifest}')
    engine = {}
    for name in ('XUL', 'libxul.so'):
        product_engine = args.binary.parent / name
        object_engine = args.objdir / 'dist/bin' / name
        if product_engine.is_file() and object_engine.is_file():
            engine[name] = {'product_sha256': digest(product_engine), 'objdir_sha256': digest(object_engine)}
    return {'frontend_buildid2': buildid2.read_text().strip(), 'engine_hashes': engine, 'identity': actual, 'binary_sha256': digest(args.binary), 'mozinfo': info,
            'testing_modules': modules, 'config_status_sha256': digest(args.objdir / 'config.status'), 'mozconfig_path': str(args.objdir / '.mozconfig'), 'mozconfig_sha256': digest(args.objdir / '.mozconfig')}



def xpcshell_wrapper(args, suite, output):
    """Plan only: never write manifests during preflight or dry-run."""
    if len(suite['manifests']) != 1 or suite.get('test_paths'):
        raise ValueError('xpcshell wrapper requires one complete original manifest')
    source = args.runtime / suite['path'] / suite['manifests'][0]
    installed = args.objdir / '_tests/xpcshell' / suite['path'] / suite['manifests'][0]
    for path in (source, installed):
        data = tomllib.loads(path.read_text())
        if data.get('DEFAULT', {}).get('firefox-appdir') != 'browser':
            raise ValueError(f'Expected original firefox-appdir=browser: {path}')
        for key, values in data.items():
            if key.startswith('include:'):
                raise ValueError('Nested includes require explicit appdir audit')
            for name in ('firefox-appdir', 'floorp-appdir'):
                if name in values and values[name] != 'browser':
                    raise ValueError(f'Differing appdir override in {path}: {key}')
    # Installed manifests may contain harness-generated metadata; include them
    # verbatim, never regenerate tests or their predicates from source TOML.
    wrapper = output / (suite['id'] + '.manifest.toml')
    text = '[DEFAULT]\nfloorp-appdir = "browser"\n\n[' + json.dumps('include:' + str(installed)) + ']\n'
    return {'path': str(wrapper), 'text': text, 'original': str(installed),
            'original_sha256': digest(installed), 'source_sha256': digest(source)}


def prepare_xpcshell_wrapper(args, spec):
    # These are source-only Python libraries, not mach or a browser launch.
    for relative in ('testing/mozbase/manifestparser', 'third_party/python/tomlkit'):
        sys.path.insert(0, str(args.runtime / relative))
    from manifestparser import TestManifest
    original = Path(spec['original'])
    if digest(original) != spec['original_sha256']:
        raise ValueError('Installed manifest changed since planning')
    wrapper = Path(spec['path'])
    with wrapper.open('x') as stream:
        stream.write(spec['text'])
    before = TestManifest([str(original)], strict=True).tests
    after = TestManifest([str(wrapper)], strict=True).tests
    if len(before) != len(after) or not before:
        raise ValueError('Wrapper changed original test count')
    # These labels are relative to the manifest root, not execution locations.
    # path/here/manifest/head/support/predicates/expected must remain identical.
    labels = {'relpath', 'manifest_relpath', 'ancestor_manifest', 'floorp-appdir'}
    for old, new in zip(before, after):
        if new.get('floorp-appdir') != 'browser' or any(
            old.get(k) != new.get(k) for k in old.keys() | new.keys() if k not in labels
        ):
            raise ValueError('Wrapper changed original test metadata')
    return {'validated_tests': len(before), 'allowed_metadata_changes': sorted(labels),
            'wrapper_sha256': digest(wrapper)}


def command(args, config, suite, output):
    cmd = [str(args.python), str(args.runtime / 'mach')]
    kind = suite['kind']
    if kind == 'browser':
        cmd += ['mochitest', '--flavor', 'browser', '--appname', str(args.binary),
                '--testing-modules-dir', str(args.modules), '--headless']
    elif kind == 'xpcshell':
        # --app-binary makes Mozilla run Floorp --xpcshell, not a foreign shell.
        cmd = [str(getattr(args, 'xpcshell_python', None) or args.python),
               str(args.runtime / 'testing/xpcshell/runxpcshelltests.py'),
               '--verbose', '--app-binary', str(args.binary),
               '--build-info-json', str(args.objdir / 'mozinfo.json'),
               '--utility-path', str(args.objdir / 'dist/bin'),
               '--symbols-path', str(args.objdir / 'dist/crashreporter-symbols'),
               '--temp-dir', str(output / (suite['id'] + '.temp')),
               '--profile-name', 'firefox', '--no-logfiles', '--threads', '4',
                '--testing-modules-dir', str(args.modules), '--xre-path', str(args.resources),
                '--manifest', str(output / (suite['id'] + '.manifest.toml'))]
    elif kind == 'marionette':
        cmd += ['marionette-test', '--binary', str(args.binary), '--headless']
    else:
        raise ValueError(f'Unknown harness kind: {kind}')
    if kind != 'xpcshell':
        for key, value in config['browser_preferences'].items():
            cmd += ['--setpref', f'{key}={value}']
    cmd += ['--log-raw', str(output / (suite['id'] + '.jsonl')),
            '--log-tbpl', str(output / (suite['id'] + '.tbpl'))]
    if kind != 'xpcshell':
        cmd += list(suite.get('test_paths', [suite['path']]))
    return cmd


class Lock:
    """Atomic directory lock, never silently steals a potentially live lock."""
    def __init__(self, path):
        self.path = Path(path)
        self.retain = False
    def __enter__(self):
        self.path.mkdir()  # FileExistsError is deliberate, including stale locks.
        (self.path / 'owner.json').write_text(json.dumps({'pid': os.getpid(), 'started': timestamp()}))
        return self
    def __exit__(self, *_):
        if self.retain: return
        (self.path / 'owner.json').unlink()
        self.path.rmdir()


def timestamp(): return datetime.now(timezone.utc).isoformat()


def structured_summary(path):
    summary = {'test_end': {}, 'unexpected_statuses': 0, 'suite_end': False, 'observed_unique_test_end': 0, 'executed_unique_test_end': 0}
    seen = set()
    executed = set()
    if not path.exists(): return summary
    for line in path.read_text(errors='replace').splitlines():
        try: item = json.loads(line)
        except json.JSONDecodeError: continue
        if item.get('action') == 'suite_end': summary['suite_end'] = True
        if item.get('action') == 'test_end':
            seen.add(json.dumps(item.get('test'), sort_keys=True))
            summary['observed_unique_test_end'] = len(seen)
            status = item.get('status', 'UNKNOWN')
            if status not in ('SKIP', 'UNKNOWN'):
                executed.add(json.dumps(item.get('test'), sort_keys=True))
                summary['executed_unique_test_end'] = len(executed)
            summary['test_end'][status] = summary['test_end'].get(status, 0) + 1
        if item.get('action') in ('test_end', 'test_status') and 'expected' in item:
            summary['unexpected_statuses'] += 1
    return summary


def passed(exit_code, summary):
    return exit_code == 0 and summary['suite_end'] and summary['unexpected_statuses'] == 0 and summary['executed_unique_test_end'] > 0


def product_processes(binary):
    # Inspect only this supplied product, never kill arbitrary Firefox processes.
    listing = subprocess.check_output(['ps', '-axo', 'pid=,command='], text=True)
    result = []
    executable = str(binary)
    prefix = str(binary.parent) + '/'
    for line in listing.splitlines():
        fields = line.strip().split(None, 1)
        if len(fields) != 2: continue
        pid, command_line = fields
        if command_line == executable or command_line.startswith(executable + ' ') or command_line.startswith(prefix):
            result.append(int(pid))
    return result


def execute(cmd, cwd, log, env):
    # Best-effort cancellation. mozrunner may create separate sessions; interrupted
    # runs retain locks and require verification before another run.
    with log.open('wb') as stream:
        process = subprocess.Popen(cmd, cwd=cwd, env=env, stdout=stream,
                                   stderr=subprocess.STDOUT, start_new_session=True)
        try:
            return process.wait()
        except BaseException:
            try: os.killpg(process.pid, signal.SIGTERM)
            except ProcessLookupError: pass
            try: process.wait(timeout=15)
            except subprocess.TimeoutExpired:
                os.killpg(process.pid, signal.SIGKILL)
                process.wait()
            raise


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('runtime', 'objdir', 'binary', 'resources', 'modules', 'python', 'logs'):
        parser.add_argument('--' + name, type=lambda s: Path(s).expanduser().resolve(), required=True)
    parser.add_argument('--xpcshell-python', type=lambda s: Path(s).expanduser().absolute(),
                        help='Runtime common virtualenv Python for native xpcshell harness dependencies')
    parser.add_argument('--config', type=Path, default=Path(__file__).with_name('suites.json'))
    parser.add_argument('--suite', action='append', help='Suite id or P0/P1/P2; repeatable; default all')
    parser.add_argument('--p0-chunk-size', type=int, help='Split all original P0 manifest tests into independent native invocations of N files')
    parser.add_argument('--dry-run', action='store_true', help='Verify inputs and print plan; do not start harnesses')
    parser.add_argument('--lock', type=Path, default=Path(tempfile.gettempdir()) / 'floorp-native-product-tests.lock')
    args = parser.parse_args(argv)
    def interrupted(_signum, _frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    if os.name != 'posix':
        parser.error('This initial runner supports macOS/Linux process-group cleanup only')
    config = json.loads(args.config.read_text())
    if config.get('schema') != 1: parser.error('Unsupported config schema')
    if args.p0_chunk_size is not None:
        config = chunk_p0(config, args.runtime, args.p0_chunk_size)
    choices = args.suite or []
    known = {s['id'] for s in config['suites']} | {s['priority'] for s in config['suites']} | {s.get('selection_group') for s in config['suites']}
    if any(c not in known for c in choices): parser.error('Unknown suite selector')
    suites = [s for s in config['suites'] if not choices or s['id'] in choices or s['priority'] in choices or s.get('selection_group') in choices]
    provenance = preflight(args, config)
    plan = [{'id': s['id'], 'kind': s['kind'], 'manifests': s['manifests'], 'registered_files': s['registered_files'],
             'test_paths': s.get('test_paths'), 'command': command(args, config, s, args.logs)} for s in suites]
    for suite, entry in zip(suites, plan):
        if suite['kind'] == 'xpcshell':
            entry['manifest_wrapper'] = xpcshell_wrapper(args, suite, args.logs)
            probe = subprocess.run([str(args.xpcshell_python or args.python), '-c',
                'import sys; sys.path.insert(0, sys.argv[1]); import runxpcshelltests',
                str(args.runtime / 'testing/xpcshell')], capture_output=True, text=True)
            if probe.returncode:
                raise ValueError('Native xpcshell dependencies unavailable; supply --xpcshell-python from this Runtime common virtualenv: ' + probe.stderr[-1500:])
    if args.dry_run:
        print(json.dumps({'cwd': str(args.runtime), 'plan': plan, 'identity': provenance['identity']}, indent=2))
        return 0
    # No append/reuse of a prior run's evidence directory.
    with Lock(args.lock) as host_lock, Lock(args.objdir / '.native-product-tests.lock') as object_lock:
        if product_processes(args.binary):
            raise ValueError('Selected product is already running; close/verify it before testing')
        args.logs.mkdir(parents=True, exist_ok=False)
        env = os.environ.copy()
        env['MOZ_DISABLE_NONLOCAL_CONNECTIONS'] = '1'
        env['MOZCONFIG'] = str(args.objdir / '.mozconfig')  # Use recorded snapshot; never edit the user's source config.
        evidence = {'started': timestamp(), 'cwd': str(args.runtime), 'provenance': provenance,
                    'config': config, 'plan': plan, 'results': [], 'finished': None}
        evidence_path = args.logs / 'run.json'
        def save(): evidence_path.write_text(json.dumps(evidence, indent=2))
        save()
        code = 0
        try:
            for entry in plan:
                if 'manifest_wrapper' in entry:
                    entry['wrapper_validation'] = prepare_xpcshell_wrapper(args, entry['manifest_wrapper'])
                    (args.logs / (entry['id'] + '.temp')).mkdir()
                    save()
                log = args.logs / (entry['id'] + '.console.log')
                result = {'id': entry['id'], 'started': timestamp(), 'exit_code': None}
                evidence['results'].append(result); save()
                status = execute(entry['command'], args.runtime, log, env)
                result.update(exit_code=status, ended=timestamp(),
                              summary=structured_summary(args.logs / (entry['id'] + '.jsonl')))
                (args.logs / (entry['id'] + '.exit')).write_text(str(status) + '\n')
                summary = result['summary']
                result['passed'] = passed(status, summary)
                leftovers = product_processes(args.binary)
                if leftovers:
                    result['passed'] = False
                    evidence['leftover_product_pids'] = leftovers
                    evidence['manual_cleanup_required'] = True
                    host_lock.retain = object_lock.retain = True
                save()
                if not result['passed']: code = 1
                if leftovers: break
            return code
        except KeyboardInterrupt:
            evidence['interrupted'] = True
            evidence['manual_cleanup_required'] = True
            host_lock.retain = object_lock.retain = True
            return 130
        except BaseException:
            # After a launch attempt, detached mozrunner sessions may survive.
            evidence['manual_cleanup_required'] = True
            host_lock.retain = object_lock.retain = True
            raise
        finally:
            for result in evidence['results']:
                if result['exit_code'] is None:
                    result['summary'] = structured_summary(args.logs / (result['id'] + '.jsonl'))
                    result['passed'] = False
            evidence['finished'] = timestamp(); save()


if __name__ == '__main__':
    try: sys.exit(main())
    except (ValueError, OSError, KeyError, zipfile.BadZipFile, configparser.Error) as error:
        print(f'Native product test setup failed: {error}', file=sys.stderr)
        sys.exit(2)
