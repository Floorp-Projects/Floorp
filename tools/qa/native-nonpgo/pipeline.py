# SPDX-License-Identifier: MPL-2.0
import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import platform
import plistlib
import re
import shlex
import shutil
import struct
import subprocess
import sys
import time
from support import (Failure, Runner, clean_environment, digest, download,
                     extract_tar, require, scan_tar, write_json)
from owned_native import OwnedNative
from checkpoint_public import public_restore
from sdk_source import load_sdk_source, validate_acquisition_receipt


HERE = Path(__file__).resolve().parent


class CheckpointComplete(Exception):
    pass


def capture(args, cwd=None):
    value = subprocess.check_output(args, cwd=cwd, timeout=30, stderr=subprocess.DEVNULL)
    require(len(value) < 2 * 1024**2, 'capture-limit')
    return value.decode().strip()


def preflight(plan, source, fixture, harness, task):
    require(sys.platform == 'darwin' and platform.machine() == 'x86_64', 'runner-platform')
    require(platform.python_version() == plan['runner']['python'], 'runner-python')
    require(capture(['/usr/bin/sw_vers', '-productVersion']) == plan['runner']['osVersion'], 'runner-os')
    require(os.environ.get('ImageVersion') == plan['runner']['imageVersion'], 'runner-image')
    require(os.environ.get('GITHUB_RUN_ATTEMPT') == '1', 'retry-forbidden')
    require(shutil.disk_usage(task).free >= plan['limits']['minFreeBeforeBuildGiB'] * 1024**3,
            'runner-free-disk')
    require(capture(['git', 'rev-parse', 'HEAD'], source) == plan['runtime']['ref'], 'source-commit')
    require(capture(['git', 'rev-parse', 'HEAD^{tree}'], source) == plan['runtime']['tree'], 'source-tree')
    require(not capture(['git', 'status', '--porcelain', '--untracked-files=no'], source), 'source-dirty')
    modes = subprocess.check_output(['git', 'ls-tree', '-r', '--format=%(objectmode)', 'HEAD'],
                                    cwd=source, timeout=30)
    require(len(modes) < 8 * 1024**2 and b'160000' not in modes.splitlines(), 'unmaterialized-source-gitlink')
    require(capture(['git', 'rev-parse', 'HEAD'], harness) == plan['harness']['commit'], 'harness-commit')
    expected_fixture = os.environ.get('FLOORP_FIXTURE_SHA', '')
    require(re.fullmatch('[0-9a-f]{40}', expected_fixture) and
            capture(['git', 'rev-parse', 'HEAD'], fixture) == expected_fixture, 'helper-fixture-commit')
    for row in plan['harness']['files']:
        require(digest(harness / row['path']) == row['sha256'], 'harness-file-hash')
    for row in plan['runtime']['patches']:
        require(digest(source / row['path']) == row['sha256'], 'source-patch-hash')
    pin = plan['runtime']['firstCausePatch']
    require(digest(fixture / pin['path']) == pin['sha256'], 'first-cause-patch-hash')
    require(digest(source / pin['hostFile']) == pin['baseHostSHA256'], 'base-host-hash')
    graphics = ctypes.CDLL('/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics')
    graphics.CGPreflightPostEventAccess.restype = ctypes.c_bool
    require(graphics.CGPreflightPostEventAccess(), 'native-input-permission-unavailable')
    return {'host': 'x86_64-apple-darwin', 'os': plan['runner']['osVersion'],
            'image': os.environ['ImageVersion'], 'python': platform.python_version(),
            'freeGiB': round(shutil.disk_usage(task).free / 1024**3, 2),
            'nativeInputPreflight': True, 'sourceTree': plan['runtime']['tree'],
            'fixture': expected_fixture, 'newArtifacts': 0}


def prepare_tools(plan, task, runner, env):
    tools = task / 'tools'
    tools.mkdir()
    archives = task / 'downloads'
    archives.mkdir()
    receipts = []
    for row in plan['toolchains']['taskclusterPackages']:
        path = archives / (row['name'] + '.tar.zst')
        url = ('https://firefox-ci-tc.services.mozilla.com/api/queue/v1/task/' + row['task'] +
               '/runs/' + str(row['run']) + '/artifacts/' + row['artifact'])
        receipt = download(url, path, row['sha256'], row['bytes'])
        tar_path = archives / (row['name'] + '.tar')
        with tar_path.open('xb') as output:
            process = subprocess.Popen(['/usr/local/bin/zstd', '-dc', str(path)], stdout=output,
                                       stderr=subprocess.DEVNULL, env=env)
            try:
                process.wait(timeout=180)
            except BaseException:
                process.kill()
                process.wait()
                raise
        require(process.returncode == 0 and tar_path.stat().st_size < 4 * 1024**3, 'toolchain-decompress')
        staging = archives / (row['name'] + '-extracted')
        tree = extract_tar(tar_path, staging)
        for entry in staging.iterdir():
            require(not (tools / entry.name).exists(), 'toolchain-root-collision')
            shutil.move(entry, tools / entry.name)
        staging.rmdir()
        path.unlink()
        tar_path.unlink()
        receipts.append({'name': row['name'], 'task': row['task'], 'run': row['run'],
                         **receipt, 'tree': tree})
    rust = plan['toolchains']['rust']
    for name, row in rust['packages'].items():
        path = archives / (name + '.tar.xz')
        receipt = download(row['url'], path, row['sha256'])
        staging = archives / (name + '-extracted')
        tree = extract_tar(path, staging)
        roots = list(staging.iterdir())
        require(len(roots) == 1 and (roots[0] / 'install.sh').is_file(), 'rust-installer-layout')
        runner.run('install-' + name, ['/bin/sh', str(roots[0] / 'install.sh'),
                   '--prefix=' + str(tools / 'rust'), '--disable-ldconfig'], roots[0], env, plan['limits']['jobMinutes'])
        shutil.rmtree(staging)
        path.unlink()
        receipts.append({'name': name, 'version': row['version'], **receipt, 'tree': tree,
                         'sourceManifestSHA256': rust['manifestSHA256']})
    write_json(task / 'toolchain-receipts.json', receipts)
    return tools


def prepare_harness(plan, harness, fixture, runner, env):
    pin = plan['harness']['startupPatch']
    patch = fixture / pin['path']
    target = harness / pin['target']
    require(digest(patch) == pin['sha256'], 'harness-startup-patch-hash')
    require(digest(target) == pin['baseSHA256'], 'harness-startup-base-hash')
    for row in pin.get('additionalFiles', []):
        require(digest(harness / row['target']) == row['baseSHA256'], 'harness-additional-base-hash')
    helper = pin.get('inventoryHelper')
    if helper:
        require(digest(fixture / helper['source']) == helper['sha256'] and
                not (harness / helper['target']).exists(), 'harness-inventory-helper-binding')
    runner.run('harness-startup-check', ['git', 'apply', '--check', str(patch)],
               harness, env, plan['limits']['jobMinutes'])
    runner.run('harness-startup-apply', ['git', 'apply', str(patch)],
               harness, env, plan['limits']['jobMinutes'])
    require(digest(target) == pin['patchedSHA256'], 'harness-startup-patched-hash')
    for row in pin.get('additionalFiles', []):
        require(digest(harness / row['target']) == row['patchedSHA256'], 'harness-additional-patched-hash')
    if helper:
        shutil.copyfile(fixture / helper['source'], harness / helper['target'])
        require(digest(harness / helper['target']) == helper['sha256'], 'harness-inventory-helper-hash')
    result = {'commit': plan['harness']['commit'], 'patchSHA256': pin['sha256'],
              'baseSHA256': pin['baseSHA256'], 'patchedSHA256': pin['patchedSHA256']}
    recording = plan['harness'].get('failureDiagnosticsPatch')
    if recording:
        recording_patch = fixture / recording['path']
        require(digest(recording_patch) == recording['sha256'], 'harness-recording-patch-hash')
        for row in recording['targets']:
            require(digest(harness / row['target']) == row['baseSHA256'], 'harness-recording-base-hash')
        for row in recording['helpers']:
            require(digest(fixture / row['source']) == row['sha256'] and
                    not (harness / row['target']).exists(), 'harness-recording-helper-binding')
        runner.run('harness-recording-check', ['git', 'apply', '--check', str(recording_patch)],
                   harness, env, plan['limits']['jobMinutes'])
        runner.run('harness-recording-apply', ['git', 'apply', str(recording_patch)],
                   harness, env, plan['limits']['jobMinutes'])
        for row in recording['targets']:
            require(digest(harness / row['target']) == row['patchedSHA256'], 'harness-recording-patched-hash')
        for row in recording['helpers']:
            shutil.copyfile(fixture / row['source'], harness / row['target'])
            require(digest(harness / row['target']) == row['sha256'], 'harness-recording-helper-hash')
        result['failureRecording'] = {'patchSHA256': recording['sha256'],
            'targets': [{'target':row['target'],'sha256':row['patchedSHA256']}
                        for row in recording['targets']]}
    return result


def prepare_source(plan, source, fixture, task, tools, sdk, runner, env):
    for phase in ('upstream', 'debug'):
        for i, row in enumerate(plan['runtime']['patches']):
            if row['phase'] == phase:
                runner.run('patch-' + str(i), ['git', 'apply', '--ignore-space-change',
                           '--ignore-whitespace', row['path']], source, env, plan['limits']['jobMinutes'])
    runner.run('baseline-provenance', [sys.executable, '.github/workflows/scripts/record_runtime_source.py',
               '--output', str(task / 'baseline-source.json')], source, env, plan['limits']['jobMinutes'])
    pin = plan['runtime']['firstCausePatch']
    runner.run('first-cause-patch', ['git', 'apply', '--check', str(fixture / pin['path'])], source, env, plan['limits']['jobMinutes'])
    runner.run('apply-first-cause', ['git', 'apply', str(fixture / pin['path'])], source, env, plan['limits']['jobMinutes'])
    require(digest(source / pin['hostFile']) == pin['patchedHostSHA256'], 'patched-host-hash')
    branding = source / '.github/assets/branding'
    require(branding.is_dir(), 'branding-missing')
    shutil.copytree(branding, source / 'browser/branding', dirs_exist_ok=True)
    config = (source / '.github/workflows/mozconfigs/macosx64-x86_64.mozconfig').read_text()
    require(config.count('ac_add_options --disable-tests') == 1 and
            config.count('ac_add_options --enable-bootstrap') == 1, 'base-mozconfig-layout')
    config = config.replace('ac_add_options --disable-tests', 'ac_add_options --enable-tests')
    config = config.replace('ac_add_options --enable-bootstrap', 'ac_add_options --disable-bootstrap')
    options = ['--enable-debug', '--with-branding=browser/branding/floorp-daylight',
               '--enable-chrome-format=flat', '--enable-linker=lld', '--with-macos-sdk=' + str(sdk),
               '--with-wasi-sysroot=' + str(tools / 'sysroot-wasm32-wasi'),
               '--with-onnx-runtime=' + str(tools / 'onnxruntime-x86_64-apple-darwin')]
    config += '\n' + '\n'.join('ac_add_options ' + shlex.quote(value) for value in options) + '\n'
    config += 'mk_add_options MOZ_OBJDIR=' + shlex.quote(str(task / 'obj')) + '\n'
    config += "mk_add_options 'export MOZ_DISABLE_CONTENT_SANDBOX=1'\n"
    config += "mk_add_options MOZ_MAKE_FLAGS=-j3\n"
    (source / 'mozconfig').write_text(config)
    channel = source / 'build/application.ini.in'
    old = 'https://@MOZ_APPUPDATE_HOST@/update/6/%PRODUCT%/%VERSION%/%BUILD_ID%/%BUILD_TARGET%/%LOCALE%/%CHANNEL%/%OS_VERSION%/%SYSTEM_CAPABILITIES%/%DISTRIBUTION%/%DISTRIBUTION_VERSION%/update.xml'
    channel.write_text(channel.read_text().replace(old, 'https://%NORA_UPDATE_HOST%update.xml'))
    write_json(task / 'source.json', {'runtime': plan['runtime']['ref'], 'tree': plan['runtime']['tree'],
               'patchSHA256': pin['sha256'], 'patchedHostSHA256': pin['patchedHostSHA256'],
               'mozconfigSHA256': digest(source / 'mozconfig'), 'hostChangedFromOriginal': True,
               'bootstrapLoaderChangedToFixedPaths': True, 'baselineProvenanceVerified': True,
               'baselineProvenanceSHA256': digest(task / 'baseline-source.json'),
               'upstreamCommit': plan['runtime']['upstream']['upstream']['commit'],
               'originalPatchCount': len(plan['runtime']['patches']),
               'patchSetSHA256': hashlib.sha256(json.dumps(plan['runtime']['patches'],
                   sort_keys=True, separators=(',', ':')).encode()).hexdigest()})


def verify_config(plan, task, tools, sdk):
    data = json.loads((task / 'effective-config.private.log').read_text())
    substs = data['substs']
    mozinfo = json.loads((task / 'obj/mozinfo.json').read_text())
    require(mozinfo['debug'] is True and mozinfo['pgo'] is False and mozinfo['artifact'] is False,
            'compiled-profile')
    require(substs['MOZ_DEBUG'] in (True, '1', 1) and substs['ENABLE_TESTS'] in (True, '1', 1),
            'compiled-debug-tests')
    require(all(not substs.get(key) for key in
                ('MOZ_PROFILE_GENERATE', 'MOZ_PROFILE_USE', 'MOZ_PGO', 'MOZ_LTO')), 'compiled-pgo-or-lto')
    for key, expected in {'TARGET_CPU': 'x86_64', 'HOST_CPU_ARCH': 'x86_64', 'OS_ARCH': 'Darwin',
                           'OS_TARGET': 'Darwin', 'CC_TYPE': 'clang', 'CC_VERSION': '22.1.8',
                           'MOZ_APP_VERSION': '158.0.1'}.items():
        require(str(substs.get(key)) == expected, 'compiled-config-' + key.lower())
    for key, value in (('CC', tools / 'clang/bin/clang'), ('CXX', tools / 'clang/bin/clang++'),
                       ('HOST_CC', tools / 'clang/bin/clang'), ('HOST_CXX', tools / 'clang/bin/clang++'),
                       ('RUSTC', tools / 'rust/bin/rustc'),
                       ('CARGO', tools / 'rust/bin/cargo'), ('NODEJS', tools / 'node/bin/node')):
        actual = substs[key]
        command = actual if isinstance(actual, list) else shlex.split(actual)
        require(bool(command) and all(isinstance(item, str) for item in command),
                'compiler-command-missing')
        require(Path(command[0]).resolve() == value.resolve(), 'compiled-tool-path-' + key.lower())
        if key in ('CC', 'CXX', 'HOST_CC', 'HOST_CXX'):
            target_flags = [item for item in command[1:] if item.startswith('-mmacosx-version-min=')]
            require(target_flags == ['-mmacosx-version-min=' + plan['criteria']['macOSMin']],
                    'compiled-deployment-target-' + key.lower())
    for key, value in (('MACOS_SDK_DIR', sdk), ('WASI_SYSROOT', tools / 'sysroot-wasm32-wasi'),
                       ('ONNX_RUNTIME', tools / 'onnxruntime-x86_64-apple-darwin/libonnxruntime.dylib')):
        require(Path(substs[key]).resolve() == value.resolve(), 'compiled-input-path-' + key.lower())
    require(substs.get('MOZ_SANDBOX') in (True, 1, '1'), 'compiled-sandbox')
    record = {key: substs.get(key) for key in ('MOZ_DEBUG', 'ENABLE_TESTS', 'TARGET_CPU', 'HOST_CPU_ARCH',
              'OS_ARCH', 'OS_TARGET', 'CC_TYPE', 'CC_VERSION', 'HOST_CC_TYPE', 'HOST_CC_VERSION',
              'RUSTC_VERSION', 'MOZ_APP_VERSION', 'MOZ_MACBUNDLE_NAME', 'MOZ_PACKAGER_FORMAT')}
    record['macOSMinimumFromFourCompilerFlags'] = plan['criteria']['macOSMin']
    record.update({'debug': True, 'pgo': False, 'artifact': False})
    record.update({'allFourCompilerPathsPinned': True, 'sdkPathPinned': True,
                   'onnxPathPinned': True, 'wasiPathPinned': True, 'compiledSandbox': True})
    helpers = []
    display = substs['MOZ_APP_DISPLAYNAME']
    for prefix, bundle, suffix in (
        ('MOZ_CHILD_PROCESS', 'plugin-container.app', None),
        ('MOZ_EME_PROCESS', 'media-plugin-helper.app', ' Media Plugin Helper'),
        ('MOZ_GPU_PROCESS', 'gpu-helper.app', ' GPU Helper'),
        ('MOZ_PKCS11_PROCESS', 'security-module-helper.app', ' Security Module Helper')):
        name_key = prefix + ('_NAME' if suffix is None else '_NAME_BRANDED')
        name, actual_bundle = substs.get(name_key), substs.get(prefix + '_BUNDLENAME')
        if not name and not actual_bundle:
            require(suffix is not None, 'plugin-container-config-missing')
            continue
        require(actual_bundle == bundle and name == ('plugin-container' if suffix is None else display + suffix),
                'native-helper-config')
        require('/' not in name and '\\' not in name and all(ord(c) >= 32 for c in name), 'native-helper-name')
        helpers.append('Contents/MacOS/' + bundle + '/Contents/MacOS/' + name)
    record['nativeHelpers'] = helpers
    write_json(task / 'effective-config.json', record)


def macho(path, kind):
    with path.open('rb') as stream:
        header = stream.read(32)
        require(len(header) == 32 and struct.unpack('<IIII', header[:16])[:2] ==
                (0xFEEDFACF, 0x01000007), 'macho-x86-64')
        ncmds, size = struct.unpack_from('<II', header, 16)
        require(ncmds < 4096 and size < 2 * 1024**2, 'macho-load-command-limit')
        commands = stream.read(size)
        sections, uuid, cursor = [], None, 0
        for _ in range(ncmds):
            require(cursor + 8 <= len(commands), 'macho-command-bounds')
            command, length = struct.unpack_from('<II', commands, cursor)
            require(length >= 8 and cursor + length <= len(commands), 'macho-command-size')
            if command == 0x1B:
                require(length == 24, 'macho-uuid-size')
                uuid = commands[cursor + 8:cursor + 24].hex()
            if command == 0x19:
                require(length >= 72, 'macho-segment-size')
                count = struct.unpack_from('<I', commands, cursor + 64)[0]
                require(count <= 10000 and length == 72 + count * 80, 'macho-section-table')
                for index in range(count):
                    start = cursor + 72 + index * 80
                    name = commands[start:start + 32].hex()
                    address, nbytes, offset = struct.unpack_from('<QQI', commands, start + 32)
                    flags = struct.unpack_from('<I', commands, start + 64)[0]
                    if flags & 0xFF in (1, 0xC, 0x12):
                        value = 'zerofill'
                    else:
                        require(offset + nbytes <= path.stat().st_size, 'macho-section-bounds')
                        stream.seek(offset)
                        import hashlib
                        hasher, remaining = hashlib.sha256(), nbytes
                        while remaining:
                            data = stream.read(min(remaining, 1024**2))
                            require(bool(data), 'macho-section-truncated')
                            hasher.update(data)
                            remaining -= len(data)
                        value = hasher.hexdigest()
                    sections.append([name, address, nbytes, flags, value])
            cursor += length
        import hashlib
        section_hash = hashlib.sha256(json.dumps(sections, separators=(',', ':')).encode()).hexdigest()
    require(struct.unpack_from('<IIII', header)[3] == kind, 'macho-filetype')
    return {'bytes': path.stat().st_size, 'sha256': digest(path), 'filetype': kind,
            'uuid': uuid, 'sectionsSHA256': section_hash, 'sections': len(sections)}


def validation_outcome(report):
    monitor_failed = (report.get('monitorFailure') is not None or
                      report.get('mozilla', {}).get('monitorFailure') is not None)
    return {'execution': 'monitor-inconclusive' if monitor_failed else report['status'],
            'nativeAuthentication': 'passed' if report.get('native', {}).get('status') == 'passed' else 'not-verified',
            'monitorFailureEstablishesProductFailure': False}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--fixture', type=Path, required=True)
    parser.add_argument('--harness', type=Path, required=True)
    parser.add_argument('--task', type=Path, required=True)
    parser.add_argument('--checkpoint-role', choices=('producer', 'consumer'))
    parser.add_argument('--restore-only', action='store_true')
    parser.add_argument('--mozilla-only', action='store_true')
    parser.add_argument('--replay-origin', action='store_true')
    parser.add_argument('--sdk-source', type=Path)
    args = parser.parse_args()
    require(not args.restore_only or args.checkpoint_role == 'consumer', 'checkpoint-restore-only-role')
    require(not args.replay_origin or args.checkpoint_role == 'consumer', 'checkpoint-replay-role')
    require(not args.mozilla_only or (args.checkpoint_role == 'consumer' and not args.restore_only),
            'mozilla-smoke-consumer-role')
    task = args.task.resolve()
    plan_bytes = (HERE / 'plan.json').read_bytes()
    plan = json.loads(plan_bytes)
    plan_hash = hashlib.sha256(plan_bytes).hexdigest()
    require((task / 'started-monotonic').is_file(), 'monotonic-origin-missing')
    started = float((task / 'started').read_text())
    runner = Runner(task, started, plan['limits'])
    report = {'status': 'failed', 'newArtifacts': 0, 'retry': False, 'productQualification': False,
              'priorExecution': plan.get('recovery'), 'timePolicy': plan.get('timePolicy')}
    stage = 'preflight'
    sdk_source = None
    checkpoint_role = args.checkpoint_role
    if checkpoint_role:
        report.update(qualification=plan['checkpoint']['qualification'], freshObjdir=False,
                      sameJob=False, checkpointRole=checkpoint_role,
                      criteria={**plan['criteria'], 'freshObjdir': False, 'sameJob': False},
                      checkpointStorage={'authorizedArtifacts': 1, 'retentionDays': 1,
                                         'maxStoredBytes': plan['checkpoint']['maxStoredBytes']})
        runner.phase_deadline_minutes = plan['checkpoint'][
            'producerBuildDeadlineMinutes' if checkpoint_role == 'producer' else 'consumerRestoreDeadlineMinutes']
    try:
        sdk_source = load_sdk_source(HERE / 'plan.json', args.sdk_source)
        (task / 'tmp').mkdir(mode=0o700)
        basic_env = clean_environment(task)
        basic_env['PATH'] += ':/usr/local/bin'
        report['preflight'] = preflight(plan, args.source, args.fixture, args.harness, task)
        if checkpoint_role == 'producer':
            runner.observed_processes = set()
            report['checkpointBoundaryContract'] = plan['checkpoint']['boundaryContract']
        stage = 'harness'
        report['harness'] = prepare_harness(plan, args.harness, args.fixture, runner, basic_env)
        stage = 'sdk'
        sdk_command = [sys.executable, str(HERE / 'materialize.py'), 'sdk', '--task', str(task)]
        if args.sdk_source is not None:
            sdk_command += ['--sdk-source', str(args.sdk_source.resolve())]
        runner.run('sdk-materialize', sdk_command, task, basic_env, plan['limits']['jobMinutes'])
        sdk = Path(json.loads((task / 'sdk-materialized.json').read_text())['path'])
        require(sdk.resolve().is_relative_to(task), 'sdk-materialized-path')
        stage = 'toolchains'
        runner.run('tools-materialize', [sys.executable, str(HERE / 'materialize.py'), 'tools',
                   '--task', str(task)], task, basic_env, plan['limits']['jobMinutes'])
        tools = task / 'tools'
        env = clean_environment(task, tools)
        env['PATH'] += ':/usr/local/bin'
        env['MOZ_BUILD_DATE'] = time.strftime('%Y%m%d%H%M%S', time.gmtime(started))
        if checkpoint_role == 'consumer':
            date = os.environ.get('CHECKPOINT_BUILD_DATE', '')
            require(re.fullmatch('[0-9]{14}', date), 'checkpoint-build-date')
            env['MOZ_BUILD_DATE'] = date
        os.environ['MOZ_BUILD_DATE'] = env['MOZ_BUILD_DATE']
        checkpoint_env = {**env, **{key: os.environ[key] for key in
                          ('FLOORP_FIXTURE_SHA', 'HARNESS_SHA', 'RUNTIME_SHA', 'GITHUB_REPOSITORY',
                           'GITHUB_RUN_ID', 'GITHUB_RUN_ATTEMPT', 'GITHUB_SHA', 'ImageVersion')
                          if key in os.environ}}
        for name, command, expected in (
            ('clang', [str(tools / 'clang/bin/clang'), '--version'], 'clang version 22.1.8'),
            ('rust', [str(tools / 'rust/bin/rustc'), '--version'], 'rustc 1.95.0 (59807616e 2026-04-14)'),
            ('node', [str(tools / 'node/bin/node'), '--version'], 'v22.16.0'),
            ('nasm', [str(tools / 'nasm/nasm'), '-v'], 'NASM version 3.02')):
            path = runner.run('version-' + name, command, task, env, plan['limits']['jobMinutes'])
            require(expected in path.read_text(), 'tool-version-' + name)
        stage = 'prepare-source'
        prepare_source(plan, args.source, args.fixture, task, tools, sdk, runner, env)
        require(not (task / 'obj').exists(), 'objdir-not-fresh')
        if checkpoint_role == 'consumer':
            stage = 'checkpoint-restore'
            runner.run(stage, [sys.executable, str(HERE / 'checkpoint.py'), 'restore',
                       '--source', str(args.source), '--task', str(task)] +
                       (['--replay-origin'] if args.replay_origin else []), task, checkpoint_env,
                       plan['limits']['jobMinutes'], phase_deadline_minutes=plan['checkpoint']['consumerRestoreDeadlineMinutes'])
            stage = 'checkpoint-restore-public-evidence'
            report['checkpointRestore'] = public_restore(json.loads((task / 'checkpoint-restore.json').read_text()))
            if report['checkpointRestore'].get('replay') is not None:
                report['checkpointReplay'] = report['checkpointRestore']['replay']
                report['qualification'] = report['checkpointRestore']['qualification']
            stage = 'effective-config'
            runner.run('effective-config', [sys.executable, 'mach', 'environment', '--format', 'json', '--verbose'],
                       args.source, env, plan['limits']['jobMinutes'])
            from checkpoint import verify_restored_config
            stage = 'checkpoint-effective-config-validation'
            verify_restored_config(task)
            report['resumedEffectiveConfigVerified'] = True
        else:
            stage = 'configure'
            runner.run('configure', [sys.executable, 'mach', 'configure'], args.source, env, plan['limits']['jobMinutes'])
            runner.run('effective-config', [sys.executable, 'mach', 'environment', '--format', 'json', '--verbose'],
                       args.source, env, plan['limits']['jobMinutes'])
        stage = 'effective-config-validation'
        verify_config(plan, task, tools, sdk)
        if args.restore_only:
            report.update(status='restore-ready', restoreOnly=True,
                          fullHostXUL='NOT_TESTED', nativeAuthentication='NOT_TESTED',
                          qualification='verified-origin-checkpoint-restore-only')
            raise CheckpointComplete
        if checkpoint_role == 'producer':
            from checkpoint import full_log_receipt, verify_graph
            stage = 'checkpoint-exports'
            runner.run(stage, [sys.executable, 'mach', 'build', '-j', '3', 'pre-export', 'export'],
                       args.source, env, plan['limits']['jobMinutes'],
                       phase_deadline_minutes=plan['checkpoint']['producerBuildDeadlineMinutes'])
            data = json.loads((task / 'effective-config.private.log').read_text())
            boundary = verify_graph(task, args.source, data)
            archive = Path(boundary['archive'])
            require(not archive.exists() and not archive.is_symlink(), 'checkpoint-rust-not-fresh')
            stage = 'checkpoint-rust'
            runner.run(stage, [sys.executable, 'mach', 'build', '-j', '3', '-C', '.', boundary['node']],
                       args.source, env, plan['limits']['jobMinutes'],
                       phase_deadline_minutes=plan['checkpoint']['producerBuildDeadlineMinutes'])
            depinfo = Path(boundary['depInfo'])
            require(archive.is_file() and not archive.is_symlink() and archive.stat().st_size > 8 and
                    depinfo.is_file() and not depinfo.is_symlink() and depinfo.stat().st_size > 0,
                    'checkpoint-rust-output')
            with archive.open('rb') as stream:
                require(stream.read(8) == b'!<arch>\n', 'checkpoint-rust-archive-format')
            runner.run('checkpoint-rust-archive', ['/usr/bin/ar', '-t', str(archive)], task, env,
                       plan['limits']['jobMinutes'],
                       phase_deadline_minutes=plan['checkpoint']['producerBuildDeadlineMinutes'])
            host = task / 'obj/widget/cocoa/MacWebAppService.o'
            require(not host.exists() and not host.is_symlink(), 'checkpoint-host-already-built')
            require(not any((task / 'obj/dist/bin').glob('XUL*')), 'checkpoint-xul-already-built')
            stage = 'checkpoint-build-completion'
            report['checkpointStageObservations'] = {
                'status': runner.observation_status, 'identities': len(runner.observed_processes),
                'scope': 'sampled measured stage-group identities; absence is not proof of unrelated ancestry'}
            from checkpoint import completed_build
            completion = report['checkpointBuildCompletion'] = {}
            boundary.update(completed_build(runner, completion))
            boundary.update(archiveSHA256=digest(archive), depInfoSHA256=digest(depinfo),
                            logs={name: full_log_receipt(task / (name + '.private.log'))
                                  for name in ('checkpoint-exports', 'checkpoint-rust')})
            require(all(row['complete'] for row in boundary['logs'].values()), 'checkpoint-log-unavailable')
            write_json(task / 'checkpoint-boundary.json', boundary)
            stage = 'checkpoint-pack'
            runner.run(stage, [sys.executable, str(HERE / 'checkpoint.py'), 'pack', '--source',
                       str(args.source), '--task', str(task)], task, checkpoint_env, plan['limits']['jobMinutes'],
                       phase_deadline_minutes=plan['checkpoint']['producerPackDeadlineMinutes'])
            ready = json.loads((task / 'checkpoint-ready.json').read_text())
            report.update(status='checkpoint-ready', checkpoint=ready, nativeAuthentication='NOT_TESTED')
            with Path(os.environ['GITHUB_OUTPUT']).open('a') as output:
                for key in ('bytes', 'sha256', 'manifestSHA256', 'buildDate'):
                    output.write(key + '=' + str(ready[key]) + '\n')
            raise CheckpointComplete
        runner.phase_deadline_minutes = None
        stage = 'full-build'
        runner.run('full-build', [sys.executable, 'mach', 'build', '-j', '3'], args.source, env,
                   plan['limits']['buildDeadlineMinutes'],
                   phase_deadline_minutes=plan['checkpoint']['consumerBuildDeadlineMinutes'] if checkpoint_role else None)
        if checkpoint_role:
            runner.phase_deadline_minutes = plan['checkpoint']['consumerNativeDeadlineMinutes']
        object_path = task / 'obj/widget/cocoa/MacWebAppService.o'
        report['hostObject'] = macho(object_path, 1)
        require(object_path.stat().st_mtime >= started, 'object-not-fresh')
        if args.mozilla_only:
            from mozilla_harness_smoke import run_smoke
            stage = 'mozilla-harness'
            report['mozilla'] = {}
            run_smoke(args.source, HERE, task, runner, env,
                      plan['checkpoint']['consumerNativeDeadlineMinutes'], macho, report['mozilla'])
            report.update(status='mozilla-ready', mozillaOnly=True, fullHostXUL='passed',
                          stagePackage='NOT_TESTED', nativeAuthentication='NOT_TESTED',
                          qualification='verified-origin-current-mozilla-xpcshell-smoke')
            raise CheckpointComplete
        stage = 'stage-package'
        runner.run('stage-package', [sys.executable, 'mach', 'build', '--allow-subdirectory-build',
                   'browser/installer/stage-package'], args.source, env, plan['limits']['buildDeadlineMinutes'],
                   phase_deadline_minutes=plan['checkpoint']['consumerPackageDeadlineMinutes'] if checkpoint_role else None)
        apps = list((task / 'obj/dist/floorp').glob('*.app'))
        require(len(apps) == 1, 'staged-app-layout')
        app = apps[0]
        report['linkedXUL'] = macho(app / 'Contents/MacOS/XUL', 6)
        for link in app.rglob('*'):
            if link.is_symlink():
                require(link.resolve().is_relative_to(app.resolve()) and link.resolve().exists(), 'app-external-link')
        disposable = task / 'validation.app'
        shutil.copytree(app, disposable, symlinks=True)
        require(digest(disposable / 'Contents/MacOS/XUL') == report['linkedXUL']['sha256'], 'copy-xul-binding')
        runner.run('adhoc-copy', ['/usr/bin/codesign', '--force', '--deep', '--timestamp=none',
                   '--sign', '-', str(disposable)], task, basic_env, plan['limits']['jobMinutes'])
        runner.run('verify-copy', ['/usr/bin/codesign', '--verify', '--deep', '--strict', str(disposable)],
                   task, basic_env, plan['limits']['jobMinutes'])
        report['signedCopyXUL'] = macho(disposable / 'Contents/MacOS/XUL', 6)
        require(report['signedCopyXUL']['sectionsSHA256'] == report['linkedXUL']['sectionsSHA256'] and
                report['signedCopyXUL']['uuid'] == report['linkedXUL']['uuid'] and
                report['linkedXUL']['uuid'] is not None, 'ad-hoc-code-binding')
        stage = 'native-suite'
        config = json.loads((task / 'effective-config.json').read_text())
        runner.native_registry = OwnedNative(task, disposable, args.harness, started, config['nativeHelpers'])
        runner.run('native-suite', [sys.executable, str(HERE / 'native_suite.py'), '--browser', str(disposable),
                   '--harness', str(args.harness), '--task', str(task), '--build-id', env['MOZ_BUILD_DATE']],
                   task, clean_environment(task),
                   plan['limits']['nativeDeadlineMinutes'], plan['limits']['maxNativePrivateBytes'],
                   phase_deadline_minutes=plan['checkpoint']['consumerNativeDeadlineMinutes'] if checkpoint_role else None)
        report['native'] = json.loads((task / 'native-summary.json').read_text())
        require(report['native']['status'] == 'passed', 'native-summary')
        require(digest(disposable / 'Contents/MacOS/XUL') == report['signedCopyXUL']['sha256'], 'post-test-copy-binding')
        if checkpoint_role:
            runner.phase_deadline_minutes = plan['checkpoint']['finalDeadlineMinutes']
        runner.run('verify-copy-after', ['/usr/bin/codesign', '--verify', '--deep', '--strict', str(disposable)],
                   task, basic_env, plan['limits']['cleanupDeadlineMinutes'])
        report['status'] = 'passed'
    except CheckpointComplete:
        pass
    except BaseException as error:
        code = str(error) if isinstance(error, Failure) else type(error).__name__
        runner.fail(stage, code)
        native_summary = task / 'native-summary.json'
        if native_summary.is_file() and native_summary.stat().st_size < 65536:
            try:
                report['native'] = json.loads(native_summary.read_text())
            except (ValueError, OSError):
                report['nativeEvidenceInvalid'] = True
        if stage in ('configure', 'full-build', 'stage-package', 'toolchains'):
            paths = {'configure': 'configure', 'full-build': 'full-build',
                     'stage-package': 'stage-package', 'toolchains': 'tools-materialize'}
            try:
                path = task / (paths[stage] + '.private.log')
                with path.open('rb') as stream:
                    stream.seek(max(0, path.stat().st_size - 131072))
                    text = stream.read(131072).decode('utf-8', errors='replace')
                markers = {name: len(re.findall(pattern, text, re.I)) for name, pattern in (
                    ('error-marker', r'error:'), ('rust-error', r'error\[E[0-9]{4}\]'),
                    ('missing-header', r'fatal error:.*file not found'),
                    ('undefined-symbol', r'Undefined symbols|undefined reference'),
                    ('linker-error', r'ld:.*error'), ('disk-full', r'No space left on device'),
                    ('memory-exhaustion', r'out of memory|cannot allocate memory'))}
                report['buildDiagnosticMarkers'] = {key: value for key, value in markers.items() if value}
                report['buildDiagnosticMarkerScope'] = 'Post-cleanup tail text matches; not a compiler-failure verdict.'
                report['rustErrorCodes'] = sorted(set(re.findall(r'error\[(E[0-9]{4})\]', text)))[:16]
            except OSError:
                report['buildDiagnosticUnavailable'] = True
    finally:
        for key, path in (('monitorPreflight', 'monitor-preflight.json'),
                           ('sdk', 'sdk-materialized.json'), ('toolchains', 'toolchain-receipts.json'),
                           ('effectiveConfig', 'effective-config.json'), ('source', 'source.json'),
                           ('checkpointReplayContext', 'checkpoint-replay-context.json')):
            try:
                candidate = task / path
                if candidate.is_file() and candidate.stat().st_size < 65536:
                    value = json.loads(candidate.read_text())
                    if key == 'sdk':
                        value.pop('path', None)
                        if args.sdk_source is None:
                            value.update({'artifact': plan['sdk']['artifact'], 'sourceRun': plan['sdk']['run'],
                                          'zipSHA256': plan['sdk']['zipSHA256'], 'tarSHA256': plan['sdk']['tarSHA256']})
                        else:
                            require(sdk_source is not None, 'sdk-source-not-validated')
                            acquired = validate_acquisition_receipt(
                                json.loads((task / 'sdk-download/receipt.json').read_text()), sdk_source)
                            value.update({'artifact': acquired['artifact'], 'sourceRun': acquired['run'],
                                          'attempt': acquired['attempt'], 'zipSHA256': acquired['zipSHA256'],
                                          'tarSHA256': acquired['tarSHA256'],
                                          'acquisitionSource': {key: sdk_source[key] for key in
                                              ('qualification', 'sourceSHA256', 'baselinePlanSHA256',
                                               'uncompressedTar', 'producerJob')},
                                          'baselineTarSHA256': plan['sdk']['tarSHA256']})
                    report[key] = value
            except (Failure, ValueError, OSError) as error:
                report['status'] = 'failed'
                report[key + 'EvidenceError'] = type(error).__name__
        report['run'] = {key: os.environ.get(name) for key, name in
            (('repository', 'GITHUB_REPOSITORY'), ('id', 'GITHUB_RUN_ID'), ('attempt', 'GITHUB_RUN_ATTEMPT'),
             ('executionHead', 'GITHUB_SHA'), ('fixture', 'FLOORP_FIXTURE_SHA'))}
        report['planSHA256'] = plan_hash
        report['nativeCleanup'] = runner.native_cleanup
        report['monitorDiagnostics'] = runner.monitor_diagnostics
        report['monitorFailure'] = runner.monitor_failure
        report['firstFailure'] = runner.failure
        report['stages'] = runner.records
        if checkpoint_role:
            from checkpoint import full_log_receipt
            report['checkpointStageLogs'] = {
                row['stage']: full_log_receipt(task / (row['stage'] + '.private.log'))
                for row in runner.records if row['stage'] in
                ('checkpoint-exports', 'checkpoint-rust', 'checkpoint-pack', 'checkpoint-restore',
                 'full-build', 'stage-package', 'native-suite')}
        report['elapsedMinutes'] = round(runner.elapsed() / 60, 2)
        if report['status'] == 'checkpoint-ready':
            complete = (all(key in report for key in ('monitorPreflight', 'harness', 'sdk', 'toolchains',
                                                     'source', 'effectiveConfig', 'checkpoint')) and
                        report.get('monitorPreflight', {}).get('status') == 'passed' and
                        len(report.get('toolchains', [])) == 11 and runner.failure is None)
            if not complete:
                runner.fail('final-proof', 'checkpoint-proof-incomplete')
                report['status'], report['firstFailure'] = 'failed', runner.failure
        if report['status'] == 'restore-ready':
            complete = (args.restore_only and checkpoint_role == 'consumer' and
                        all(key in report for key in ('monitorPreflight', 'harness', 'sdk', 'toolchains',
                                                     'source', 'effectiveConfig', 'checkpointRestore')) and
                        report.get('monitorPreflight', {}).get('status') == 'passed' and
                        len(report.get('toolchains', [])) == 11 and
                        report.get('resumedEffectiveConfigVerified') is True and
                        report.get('native') is None and runner.failure is None and
                        not any(row['stage'] in ('full-build', 'stage-package', 'native-suite')
                                for row in runner.records))
            if not complete:
                runner.fail('final-proof', 'checkpoint-restore-proof-incomplete')
                report['status'], report['firstFailure'] = 'failed', runner.failure
        if report['status'] == 'passed':
            complete = (all(key in report for key in ('monitorPreflight', 'harness', 'sdk', 'toolchains', 'source', 'effectiveConfig', 'native')) and
                        report.get('monitorPreflight', {}).get('status') == 'passed' and
                        len(report.get('toolchains', [])) == 11 and runner.native_cleanup is not None and
                        runner.native_cleanup['allOwnedGone'] and runner.native_cleanup['censusComplete'] and
                        runner.native_cleanup['outlivedHarness'] == 0)
            if not complete:
                runner.fail('final-proof', 'proof-incomplete')
                report['status'], report['firstFailure'] = 'failed', runner.failure
        if report['status'] == 'mozilla-ready':
            mozilla = report.get('mozilla', {})
            complete = (args.mozilla_only and checkpoint_role == 'consumer' and
                        all(key in report for key in ('monitorPreflight', 'harness', 'sdk', 'toolchains',
                                                     'source', 'effectiveConfig', 'checkpointRestore', 'hostObject')) and
                        report.get('monitorPreflight', {}).get('status') == 'passed' and
                        len(report.get('toolchains', [])) == 11 and
                        report.get('resumedEffectiveConfigVerified') is True and
                        mozilla.get('status') == 'passed' and
                        mozilla.get('actualCompiledDebug') is True and
                        mozilla.get('actualCurrentSourceUpstreamTest') is True and
                        mozilla.get('intentionalFailureDetected') is True and
                        mozilla.get('allCasesExactReaped') is True and
                        mozilla.get('allCasesGroupsGone') is True and
                        len(mozilla.get('cases', [])) == 3 and report.get('native') is None and
                        runner.failure is None and
                        not any(row['stage'] in ('stage-package', 'native-suite') for row in runner.records))
            if not complete:
                runner.fail('final-proof', 'mozilla-smoke-proof-incomplete')
                report['status'], report['firstFailure'] = 'failed', runner.failure
        report['validationOutcome'] = validation_outcome(report)
        try:
            write_json(task / 'summary.json', report)
        except OSError as error:
            report['status'] = 'failed'
            report['evidenceWriteError'] = error.errno
            report['validationOutcome'] = validation_outcome(report)
        finally:
            print(json.dumps(report), flush=True)
    return 0 if report['status'] in ('passed', 'checkpoint-ready', 'restore-ready', 'mozilla-ready') and runner.failure is None else 1


if __name__ == '__main__':
    raise SystemExit(main())
