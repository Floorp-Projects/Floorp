# SPDX-License-Identifier: MPL-2.0
"""One fixed, successful Rust boundary; no interrupted-state or cache fallback."""

import argparse
from collections import deque
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import shutil
import stat
import subprocess
import sys
import tarfile
import time

from support import digest, require, write_json
from failure_excerpt import classify, normalized_diagnostic


STATES = ('obj', 'mozbuild', 'cargo', 'rustup')
REFERENCES = ('source', 'tools', 'sdk')
NODE = 'toolkit/library/rust/target-objects'
MANIFEST_LIMIT = 512 * 1024**2
SUBSTS = ('MOZ_DEBUG ENABLE_TESTS MOZ_PROFILE_GENERATE MOZ_PROFILE_USE MOZ_PGO MOZ_LTO '
          'TARGET_CPU HOST_CPU_ARCH OS_ARCH OS_TARGET CC_TYPE CC_VERSION MOZ_APP_VERSION '
          'CC CXX HOST_CC HOST_CXX RUSTC CARGO NODEJS MACOS_SDK_DIR WASI_SYSROOT ONNX_RUNTIME '
          'MOZ_SANDBOX HOST_CC_TYPE HOST_CC_VERSION RUSTC_VERSION MOZ_MACBUNDLE_NAME '
          'MOZ_PACKAGER_FORMAT MOZ_APP_DISPLAYNAME MOZ_CHILD_PROCESS_NAME '
          'MOZ_CHILD_PROCESS_BUNDLENAME MOZ_EME_PROCESS_NAME_BRANDED MOZ_EME_PROCESS_BUNDLENAME '
          'MOZ_GPU_PROCESS_NAME_BRANDED MOZ_GPU_PROCESS_BUNDLENAME '
          'MOZ_PKCS11_PROCESS_NAME_BRANDED MOZ_PKCS11_PROCESS_BUNDLENAME '
          'RUST_TARGET MOZ_DEBUG_RUST MOZ_CARGO_DEFAULT_PROFILE_DIR').split()


def encoded(value):
    return json.dumps(value, sort_keys=True, separators=(',', ':')).encode()


def relative(value):
    require(isinstance(value, str) and value and '\\' not in value and
            all(ord(char) >= 32 for char in value), 'checkpoint-path')
    path = PurePosixPath(value)
    require(not path.is_absolute() and '..' not in path.parts and str(path) == value,
            'checkpoint-path')
    return path


def roots(source, task, sdk):
    result = {'source': Path(source), 'tools': task / 'tools', 'sdk': Path(sdk),
              'python': Path(sys.base_prefix)}
    result.update({name: task / name for name in STATES})
    require(all(path.is_absolute() and path == path.resolve() for path in result.values()),
            'checkpoint-root-canonical')
    require(all(not path.is_symlink() for path in result.values()), 'checkpoint-root-symlink')
    return result


def link_reference(path, mapping):
    target = path.resolve(strict=False)
    for name, root in sorted(mapping.items(), key=lambda row: len(str(row[1])), reverse=True):
        if target.is_relative_to(root):
            return [name, str(target.relative_to(root))]
    raise RuntimeError('checkpoint-external-link')


def stable_hash(path, info, deadline):
    value = hashlib.sha256()
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, 'rb') as stream:
        initial = os.fstat(stream.fileno())
        require((initial.st_dev, initial.st_ino, initial.st_size, initial.st_mtime_ns) ==
                (info.st_dev, info.st_ino, info.st_size, info.st_mtime_ns), 'checkpoint-file-changed')
        while data := stream.read(1024**2):
            require(time.monotonic() < deadline, 'checkpoint-deadline')
            value.update(data)
        final = os.fstat(stream.fileno())
        require((initial.st_size, initial.st_mtime_ns, initial.st_ctime_ns) ==
                (final.st_size, final.st_mtime_ns, final.st_ctime_ns), 'checkpoint-file-changed')
    return value.hexdigest()


def included(name, key, source):
    parts = PurePosixPath(key).parts
    if key.endswith('.private.log'):
        return False
    if name == 'mozbuild':
        identity = source.name + '-' + hashlib.sha256(str(source).encode()).hexdigest()[:12]
        prefix = 'srcdirs/' + identity
        leaf = prefix + '/_virtualenvs'
        return (key in ('.', 'srcdirs', prefix, leaf, prefix + '/topsrcdir.txt') or
                key.startswith(leaf + '/'))
    if name == 'obj' and parts and parts[0] == '.mozbuild':
        return (not any(part in ('logs', 'log') for part in parts) and
                parts[-1] not in ('last_log.json', 'build_resources.json', 'latest-command.json'))
    return True


def tree(root, mapping, deadline, entry_cap, logical_cap, typed_links=True, state_name=None):
    info = root.lstat()
    rows = [{'path': '.', 'kind': 'dir', 'mode': stat.S_IMODE(info.st_mode), 'mtimeNS': info.st_mtime_ns}]
    logical = 0
    require(root.is_dir() and not root.is_symlink(), 'checkpoint-tree-root')
    for directory, names, files in os.walk(root, followlinks=False):
        names[:] = sorted(name for name in names if name not in ('.git', '__pycache__'))
        if state_name is not None:
            names[:] = [name for name in names if included(state_name,
                        str((Path(directory) / name).relative_to(root)), mapping['source'])]
        for name in sorted(names + files):
            if name in ('.git', '__pycache__') or name.endswith('.pyc'):
                continue
            path = Path(directory) / name
            key = str(path.relative_to(root))
            if state_name is not None and not included(state_name, key, mapping['source']):
                continue
            relative(key)
            require(not typed_links or path.parent != root or
                    name not in ('.netrc', 'credentials', 'credentials.toml', '.aws'),
                    'checkpoint-sensitive-state')
            info = path.lstat()
            require(info.st_uid == os.geteuid() and not info.st_mode & 0o7000,
                    'checkpoint-owner-mode')
            require(len(rows) < entry_cap and time.monotonic() < deadline, 'checkpoint-tree-bound')
            row = {'path': key, 'mode': stat.S_IMODE(info.st_mode), 'mtimeNS': info.st_mtime_ns}
            if stat.S_ISLNK(info.st_mode):
                row.update(kind='link', target=os.readlink(path))
                if typed_links:
                    row['reference'] = link_reference(path, mapping)
            elif stat.S_ISDIR(info.st_mode):
                row['kind'] = 'dir'
            else:
                require(stat.S_ISREG(info.st_mode), 'checkpoint-special-file')
                logical += info.st_size
                require(logical <= logical_cap, 'checkpoint-logical-cap')
                row.update(kind='file', bytes=info.st_size, sha256=stable_hash(path, info, deadline))
            rows.append(row)
    return sorted(rows, key=lambda row: row['path'])


def content_rows(rows):
    return [{key: value for key, value in row.items() if key != 'mtimeNS'} for row in rows]


def restore_times(root, rows):
    for row in sorted(rows, key=lambda item: len(PurePosixPath(item['path']).parts), reverse=True):
        path = root / relative(row['path'])
        os.utime(path, ns=(row['mtimeNS'], row['mtimeNS']), follow_symlinks=False)
        require(path.lstat().st_mtime_ns == row['mtimeNS'], 'checkpoint-mtime-precision')


def assignment(path, prefix):
    matches = [line[len(prefix):] for line in path.read_text().splitlines() if line.startswith(prefix)]
    require(len(matches) == 1 and '\\' not in matches[0], 'checkpoint-graph-declaration')
    return matches[0].split()


def rust_output(task, data):
    substs = data['substs']
    require(substs['RUST_TARGET'] == 'x86_64-apple-darwin', 'checkpoint-rust-target')
    profile = 'debug' if substs.get('MOZ_DEBUG_RUST') else 'release'
    require(substs['MOZ_CARGO_DEFAULT_PROFILE_DIR'] == profile, 'checkpoint-rust-profile')
    return task / 'obj' / substs['RUST_TARGET'] / profile / 'libgkrust.a'


def verify_graph(task, source, data):
    obj = task / 'obj'
    for path, prefix in ((obj / 'root.mk', 'compile_targets := '),
                         (obj / 'root.mk', 'rust_targets := '),
                         (obj / 'root-deps.mk', 'recurse_compile: '),
                         (obj / 'root-deps.mk', 'recurse_rust: ')):
        require(NODE in assignment(path, prefix), 'checkpoint-rust-graph-node')
    require((obj / 'root.mk').read_text().splitlines().count('include root-deps.mk') == 1,
            'checkpoint-root-deps-include')
    backend = obj / 'toolkit/library/rust/backend.mk'
    lines = backend.read_text().splitlines()
    expected = rust_output(task, data)
    library = assignment(backend, 'RUST_LIBRARY_FILE := ')
    require(len(library) == 1, 'checkpoint-rust-library-declaration')
    expanded = library[0].replace('$(DEPTH)', str(obj))
    require(Path(expanded).resolve() == expected, 'checkpoint-rust-library-path')
    cargo = assignment(backend, 'CARGO_TARGET_DIR := ')
    require(len(cargo) == 1 and Path(cargo[0].replace('$(DEPTH)', str(obj))).resolve() == obj,
            'checkpoint-cargo-target-dir')
    require('CARGO_FILE := $(srcdir)/Cargo.toml' in lines and
            not any(line.startswith(('RUST_LIBRARY_CARGO_PROFILE_SUFFIX',
                                     'RUST_LIBRARY_CARGO_CRATE_TYPE')) for line in lines),
            'checkpoint-rust-backend-contract')
    features = assignment(backend, 'RUST_LIBRARY_FEATURES := ')
    require(bool(features) and (source / 'toolkit/library/rust/Cargo.toml').is_file() and
            (obj / '.cargo/config.toml').is_file() and not (obj / '.cargo/config').exists(),
            'checkpoint-cargo-configuration')
    return {'node': NODE, 'archive': str(expected), 'depInfo': str(expected.with_suffix('.d')),
            'profile': expected.parent.name, 'features': features,
            'cargoConfigSHA256': digest(obj / '.cargo/config.toml')}


def context(plan_path, source, task, sdk):
    mapping = roots(source, task, sdk)
    return {'planSHA256': digest(plan_path), 'roots': {key: str(value) for key, value in mapping.items()},
            'fixture': os.environ['FLOORP_FIXTURE_SHA'], 'harness': os.environ['HARNESS_SHA'],
            'runtime': os.environ['RUNTIME_SHA'], 'repository': os.environ['GITHUB_REPOSITORY'],
            'run': os.environ['GITHUB_RUN_ID'], 'attempt': os.environ['GITHUB_RUN_ATTEMPT'],
            'head': os.environ['GITHUB_SHA'], 'image': os.environ['ImageVersion'],
            'pythonSHA256': digest(Path(sys.executable).resolve())}


def owned_processes():
    from darwin_activity import probe
    result = subprocess.run(['/bin/ps', '-axo', 'pid=,uid=,ruid='], capture_output=True,
                            text=True, check=True, timeout=3)
    require(len(result.stdout) <= 1024**2, 'checkpoint-process-inventory-size')
    births = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        require(len(fields) == 3 and fields[0].isdecimal() and
                all(re.fullmatch(r'-?[0-9]{1,10}', item) for item in fields[1:]),
                'checkpoint-process-inventory-format')
        pid, uid, ruid = map(int, fields)
        # Apple's ps formats uid_t with %d: nobody (uint32 -2) is printed -2.
        # Keep malformed/out-of-range/invalid uid_t unknown rather than zero.
        require(0 <= pid < 2**31 and all(-2**31 <= value < 2**32 - 1 and value != -1
                                        for value in (uid, ruid)), 'checkpoint-process-inventory-format')
        uid, ruid = uid % 2**32, ruid % 2**32
        if uid != os.geteuid() and ruid != os.getuid():
            continue
        require(pid > 0, 'checkpoint-process-worker-pid')
        status, bsd, reason = probe().bsd(pid)
        require(status in ('measured', 'gone'), 'checkpoint-process-identity-unavailable')
        if status == 'measured':
            require(bsd.uid == os.geteuid() and bsd.ruid == os.getuid(), 'checkpoint-process-owner-changed')
            births.add((bsd.pid, bsd.start_sec, bsd.start_usec))
    return births


def full_log_receipt(path):
    first, last, counts, matches = [], deque(maxlen=8), {}, 0
    checksum = hashlib.sha256()
    result = {'complete': False, 'rawContentPublished': False, 'freeTextPublished': False}
    try:
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(descriptor, 'rb') as stream:
            info = os.fstat(stream.fileno())
            require(stat.S_ISREG(info.st_mode) and info.st_size <= 256 * 1024**2,
                    'checkpoint-log-bound')
            number, total = 0, 0
            while line := stream.readline(2 * 1024**2 + 1):
                require(len(line) <= 2 * 1024**2 and line.endswith(b'\n'), 'checkpoint-log-line')
                number += 1
                total += len(line)
                checksum.update(line)
                text = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', line.decode('utf-8'))
                values = classify(text)
                if re.search(r'\berror(?:\[[^\]]*\])?:|\bError [0-9]+\b|\bfatal:', text, re.I) and not any(
                        value['kind'] != 'termination' for value in values):
                    values.append({'kind': 'unclassified-error-marker'})
                for value in values:
                    counts[value['kind']] = counts.get(value['kind'], 0) + 1
                    record = {'line': number, **value, 'normalizedDiagnostic': normalized_diagnostic(text)}
                    matches += 1
                    if len(first) < 16:
                        first.append(record)
                    last.append(record)
            final = os.fstat(stream.fileno())
            require(total == info.st_size == final.st_size and info.st_mtime_ns == final.st_mtime_ns,
                    'checkpoint-log-changed')
        return {**result, 'complete': True, 'bytes': total, 'sha256': checksum.hexdigest(),
                'lines': number, 'diagnosticCounts': counts, 'matchingRecords': matches,
                'recordsTruncated': matches > 16, 'records': first if matches <= 16 else first[:8] + list(last),
                'scope': 'complete stream scan; marker classes do not replace the actual command exit'}
    except Exception as error:
        return {**result, 'errorType': type(error).__name__}


class Capped:
    def __init__(self, stream, maximum, task, reserve, deadline):
        self.stream, self.maximum, self.task = stream, maximum, task
        self.reserve, self.deadline, self.count = reserve, deadline, 0

    def write(self, data):
        require(time.monotonic() < self.deadline, 'checkpoint-deadline')
        require(self.count + len(data) <= self.maximum, 'checkpoint-stored-cap')
        require(shutil.disk_usage(self.task).free >= self.reserve + len(data), 'checkpoint-disk-reserve')
        count = self.stream.write(data)
        require(count == len(data), 'checkpoint-short-write')
        self.count += count
        return count

    def flush(self):
        self.stream.flush()


def pack(plan_path, source, task, sdk, deadline):
    plan = json.loads(plan_path.read_text())
    spec = plan['checkpoint']
    boundary = json.loads((task / 'checkpoint-boundary.json').read_text())
    require(boundary['status'] == 'passed' and boundary['node'] == NODE and
            boundary['newOwnedSurvivors'] == 0 and not boundary['interrupted'], 'checkpoint-boundary')
    mapping = roots(source, task, sdk)
    data = json.loads((task / 'effective-config.private.log').read_text())
    manifest = {'schema': 1, 'context': context(plan_path, source, task, sdk),
                'buildDate': os.environ['MOZ_BUILD_DATE'], 'boundary': boundary,
                'substs': {key: data['substs'].get(key) for key in SUBSTS}, 'references': {}, 'states': {}}
    scan_deadline = min(deadline, float((task / 'started-monotonic').read_text()) + 260 * 60)
    for name in REFERENCES:
        manifest['references'][name] = tree(mapping[name], mapping, scan_deadline,
                                            spec['maxReferenceEntries'], 8 * 1024**3, typed_links=False)
    for name in STATES:
        if mapping[name].exists():
            manifest['states'][name] = tree(mapping[name], mapping, scan_deadline,
                                            spec['maxEntries'], spec['maxLogicalBytes'], state_name=name)
    require('obj' in manifest['states'] and 'mozbuild' in manifest['states'], 'checkpoint-required-state')
    require(any(row['kind'] == 'file' and '/_virtualenvs/' in row['path']
                for row in manifest['states']['mozbuild']), 'checkpoint-python-venv-missing')
    count = sum(len(rows) for rows in manifest['states'].values())
    logical = sum(row.get('bytes', 0) for rows in manifest['states'].values() for row in rows)
    require(count <= spec['maxEntries'] and logical <= spec['maxLogicalBytes'], 'checkpoint-payload-bound')
    body = encoded(manifest)
    require(len(body) <= MANIFEST_LIMIT, 'checkpoint-manifest-size')
    path = task / spec['artifactName']
    with path.open('xb') as output:
        capped = Capped(output, spec['maxStoredBytes'], task,
                        plan['limits']['minFreeDuringBuildGiB'] * 1024**3, deadline)
        with tarfile.open(fileobj=capped, mode='w|gz', format=tarfile.PAX_FORMAT, compresslevel=1) as archive:
            header = tarfile.TarInfo('checkpoint.json')
            header.size, header.mode = len(body), 0o600
            archive.addfile(header, io.BytesIO(body))
            for name, rows in manifest['states'].items():
                for row in rows:
                    if row['kind'] != 'file':
                        continue
                    file = mapping[name] / row['path']
                    require(stable_hash(file, file.lstat(), deadline) == row['sha256'], 'checkpoint-file-changed')
                    header = tarfile.TarInfo('payload/' + name + '/' + row['path'])
                    header.size, header.mode = row['bytes'], 0o600
                    with file.open('rb') as stream:
                        archive.addfile(header, stream)
        output.flush()
        os.fsync(output.fileno())
    require(path.stat().st_size <= spec['maxStoredBytes'], 'checkpoint-stored-cap')
    inspect(path, spec, deadline, expected_manifest=hashlib.sha256(body).hexdigest())
    receipt = {'bytes': path.stat().st_size, 'sha256': digest(path),
               'manifestSHA256': hashlib.sha256(body).hexdigest(), 'entries': count,
               'logicalBytes': logical, 'buildDate': manifest['buildDate'], 'rawSingleFile': True,
               'qualification': spec['qualification'], 'freshObjdir': False, 'sameJob': False}
    write_json(task / 'checkpoint-ready.json', receipt)
    return receipt


def inspect(path, spec, deadline, expected_manifest, staging=None):
    with tarfile.open(path, 'r|gz') as archive:
        header = archive.next()
        require(header is not None and header.name == 'checkpoint.json' and header.isfile() and
                0 < header.size <= MANIFEST_LIMIT, 'checkpoint-manifest-header')
        body = archive.extractfile(header).read()
        require(hashlib.sha256(body).hexdigest() == expected_manifest, 'checkpoint-manifest-hash')
        manifest = json.loads(body)
        require(manifest['schema'] == 1 and {'obj', 'mozbuild'}.issubset(manifest['states']) and
                set(manifest['states']).issubset(STATES), 'checkpoint-schema')
        expected, all_rows, logical = {}, set(), 0
        for name, rows in manifest['states'].items():
            for row in rows:
                key = str(relative(row['path']))
                require(included(name, key, Path(manifest['context']['roots']['source'])),
                        'checkpoint-excluded-state')
                require((name, key) not in all_rows, 'checkpoint-duplicate-row')
                all_rows.add((name, key))
                require(row['kind'] in ('file', 'dir', 'link') and
                        isinstance(row['mode'], int) and 0 <= row['mode'] <= 0o777 and
                        isinstance(row['mtimeNS'], int), 'checkpoint-row')
                require(key != '.' or row['kind'] == 'dir', 'checkpoint-root-row')
                if row['kind'] == 'file':
                    member = 'payload/' + name + '/' + key
                    require(member not in expected and re.fullmatch('[0-9a-f]{64}', row['sha256']),
                            'checkpoint-duplicate-or-hash')
                    require(type(row['bytes']) is int and row['bytes'] >= 0, 'checkpoint-file-size')
                    logical += row['bytes']
                    expected[member] = row
        require(sum(len(rows) for rows in manifest['states'].values()) <= spec['maxEntries'] and
                logical <= spec['maxLogicalBytes'], 'checkpoint-payload-bound')
        seen = set()
        while (member := archive.next()) is not None:
            require(time.monotonic() < deadline, 'checkpoint-deadline')
            require(member.name in expected and member.name not in seen and member.isfile() and
                    member.size == expected[member.name]['bytes'], 'checkpoint-payload-member')
            seen.add(member.name)
            checksum = hashlib.sha256()
            destination = None
            if staging is not None:
                target = staging / member.name.removeprefix('payload/')
                target.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
                destination = target.open('xb')
            try:
                with archive.extractfile(member) as stream:
                    while data := stream.read(1024**2):
                        require(time.monotonic() < deadline, 'checkpoint-deadline')
                        checksum.update(data)
                        if destination is not None:
                            destination.write(data)
            finally:
                if destination is not None:
                    destination.close()
            require(checksum.hexdigest() == expected[member.name]['sha256'], 'checkpoint-payload-content')
        require(seen == set(expected), 'checkpoint-payload-incomplete')
    return manifest


def restore(plan_path, source, task, sdk, deadline):
    plan = json.loads(plan_path.read_text())
    spec = plan['checkpoint']
    ready = json.loads((task / 'checkpoint-download.json').read_text())
    path = task / spec['artifactName']
    require(path.stat().st_size == ready['bytes'] <= spec['maxStoredBytes'] and
            digest(path) == ready['sha256'], 'checkpoint-download-integrity')
    manifest = inspect(path, spec, deadline, ready['manifestSHA256'])
    require(manifest['context'] == context(plan_path, source, task, sdk), 'checkpoint-context-mismatch')
    require(manifest['buildDate'] == os.environ['MOZ_BUILD_DATE'] and
            manifest['boundary']['status'] == 'passed' and not manifest['boundary']['interrupted'] and
            manifest['boundary']['newOwnedSurvivors'] == 0, 'checkpoint-producer-completion')
    mapping = roots(source, task, sdk)
    require(set(manifest['references']) == set(REFERENCES), 'checkpoint-reference-roots')
    for name in REFERENCES:
        actual = tree(mapping[name], mapping, deadline, spec['maxReferenceEntries'], 8 * 1024**3,
                      typed_links=False)
        require(content_rows(actual) == content_rows(manifest['references'][name]), 'checkpoint-reference-content')
    logical = sum(row.get('bytes', 0) for rows in manifest['states'].values() for row in rows)
    require(shutil.disk_usage(task).free >= path.stat().st_size + logical +
            plan['limits']['minFreeDuringBuildGiB'] * 1024**3, 'checkpoint-restore-disk')
    require(all(not mapping[name].exists() and not mapping[name].is_symlink() for name in STATES),
            'checkpoint-state-not-empty')
    staging = task / 'checkpoint-restore'
    staging.mkdir(mode=0o700, exist_ok=False)
    inspect(path, spec, deadline, ready['manifestSHA256'], staging)
    for name, rows in manifest['states'].items():
        root = staging / name
        root.mkdir(exist_ok=True, mode=0o700)
        for row in rows:
            target = root / relative(row['path'])
            if row['kind'] == 'dir':
                target.mkdir(parents=True, exist_ok=True, mode=0o700)
        root.rename(mapping[name])
    for name, rows in manifest['states'].items():
        for row in rows:
            target = mapping[name] / row['path']
            if row['kind'] == 'link':
                require(isinstance(row['target'], str) and len(row['target']) <= 4096 and
                        row['reference'][0] in mapping, 'checkpoint-link-reference')
                target.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
                target.symlink_to(row['target'])
                require(link_reference(target, mapping) == row['reference'], 'checkpoint-link-mismatch')
                if stat.S_IMODE(target.lstat().st_mode) != row['mode']:
                    require(hasattr(os, 'lchmod'), 'checkpoint-link-mode-unavailable')
                    os.lchmod(target, row['mode'])
            else:
                target.chmod(row['mode'])
        restore_times(mapping[name], rows)
        require(tree(mapping[name], mapping, deadline, spec['maxEntries'], spec['maxLogicalBytes'],
                     state_name=name) == rows,
                'checkpoint-restored-tree')
    for name in REFERENCES:
        restore_times(mapping[name], manifest['references'][name])
    write_json(task / 'checkpoint-expected-config.json', {'substs': manifest['substs']})
    return {'buildDate': manifest['buildDate'], 'producerBoundary': manifest['boundary'],
            'archiveSHA256': ready['sha256'], 'manifestSHA256': ready['manifestSHA256'],
            'qualification': spec['qualification'], 'freshObjdir': False, 'sameJob': False}


def verify_restored_config(task):
    actual = json.loads((task / 'effective-config.private.log').read_text())['substs']
    expected = json.loads((task / 'checkpoint-expected-config.json').read_text())['substs']
    require({key: actual.get(key) for key in SUBSTS} == expected,
            'checkpoint-actual-config-mismatch')


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('mode', choices=('pack', 'restore'))
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--task', type=Path, required=True)
    args = parser.parse_args()
    plan_path = Path(__file__).with_name('plan.json')
    plan = json.loads(plan_path.read_text())
    sdk = Path(json.loads((args.task / 'sdk-materialized.json').read_text())['path'])
    minute = plan['checkpoint']['producerPackDeadlineMinutes' if args.mode == 'pack'
                                else 'consumerRestoreDeadlineMinutes']
    deadline = float((args.task / 'started-monotonic').read_text()) + minute * 60
    result = (pack if args.mode == 'pack' else restore)(plan_path, args.source, args.task, sdk, deadline)
    write_json(args.task / ('checkpoint-' + args.mode + '.json'), result)
    print(json.dumps({'checkpoint': args.mode, 'result': result}), flush=True)


if __name__ == '__main__':
    main()
