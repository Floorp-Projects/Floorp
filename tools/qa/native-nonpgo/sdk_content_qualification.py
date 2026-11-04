"""Offline SDK bytes qualification and review-only fixed-pin proposals.

No API/download/credential operation. Only two fixed wire origins are allowed.
Generated source is a proposal; this module never alters the production loader.
"""
import argparse
import ast
from contextlib import contextmanager, ExitStack
import copy
import difflib
import gzip
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import plistlib
import stat
import sys
import tarfile
import tempfile
import time
import zipfile
import zlib

QA = Path(__file__).resolve().parent
sys.path.insert(0, str(QA))
import sdk_source as sdk
from qa_core import Failure, require

MAX_SECONDS = 120
MAX_MEMBERS = 100000
MAX_PATH_BYTES = 4096
MAX_CANONICAL_BYTES = 64 * 1024**2
MAX_PLIST_BYTES = 1024**2
CHUNK = 1024**2
IDENTITY = ('st_dev', 'st_ino', 'st_mode', 'st_uid', 'st_nlink', 'st_size', 'st_mtime_ns', 'st_ctime_ns')
NEW_WIRE = {'artifact': 12225585543, 'run': 39332628358, 'attempt': 1,
    'name': 'macos-sdk', 'zipBytes': 105082683,
    'zipSHA256': '4d4b7e003737a3f6db74563d1d6aff4d9776fe05d721e26756a801e6b0a0c106',
    'expiresObserved': '2026-10-20T22:08:04Z'}
NEW_PRODUCER = 118334374715
NEW_PRODUCER_NAME = 'Mac build / download-macos-sdk'


def budget(deadline):
    require(time.monotonic() < deadline, 'sdk-qualification-deadline')


def baseline():
    raw = (QA / 'plan.json').read_bytes()
    require(hashlib.sha256(raw).hexdigest() == sdk.BASELINE_PLAN_SHA256, 'sdk-qualification-plan')
    plan = json.loads(raw)
    require(plan['runtime']['ref'] == sdk.RUNTIME_REF and
            plan['sdk']['tarSHA256'] == sdk.BASELINE_TAR_SHA256 and
            all(sdk._same(plan['sdk'][key], sdk.PIN[key]) for key in sdk.CONTENT_KEYS),
            'sdk-qualification-baseline')
    return plan


def origin(target):
    require(type(target) is str and target in ('retained', 'candidate'), 'sdk-qualification-target')
    pin = copy.deepcopy(sdk.PIN)
    if target == 'candidate':
        pin.update(NEW_WIRE)
        pin['tarSHA256'] = None  # Unknown until authenticated ZIP bytes are measured.
    return pin, (sdk.PRODUCER_JOB if target == 'retained' else NEW_PRODUCER), (
        sdk.PRODUCER_NAME if target == 'retained' else NEW_PRODUCER_NAME)


@contextmanager
def owned_file(path, maximum, *, deadline=None):
    path = Path(path)
    before = path.lstat()
    require(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid() and
            before.st_nlink == 1 and 0 < before.st_size <= maximum, 'sdk-qualification-file')
    fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(fd, 'rb') as stream:
        opened = os.fstat(stream.fileno())
        require(all(getattr(before, key) == getattr(opened, key) for key in IDENTITY), 'sdk-qualification-file-changed')
        deadline = time.monotonic() + MAX_SECONDS if deadline is None else deadline
        def fingerprint():
            budget(deadline)
            os.lseek(stream.fileno(), 0, os.SEEK_SET)
            content, count = hashlib.sha256(), 0
            while True:
                budget(deadline)
                block = os.read(stream.fileno(), min(CHUNK, before.st_size + 1 - count))
                budget(deadline)
                if not block:
                    break
                count += len(block)
                require(count <= before.st_size, 'sdk-qualification-file-changed')
                content.update(block)
            require(count == before.st_size, 'sdk-qualification-file-changed')
            return content.digest()
        initial = fingerprint()
        stream.seek(0)
        yield stream
        after, current = os.fstat(stream.fileno()), path.lstat()
        require(all(getattr(before, key) == getattr(after, key) == getattr(current, key) for key in IDENTITY),
                'sdk-qualification-file-changed')
        require(fingerprint() == initial, 'sdk-qualification-file-changed')
        after, current = os.fstat(stream.fileno()), path.lstat()
        require(all(getattr(before, key) == getattr(after, key) == getattr(current, key) for key in IDENTITY),
                'sdk-qualification-file-changed')


def checksum(stream, maximum, deadline, destination=None):
    digest, count = hashlib.sha256(), 0
    while True:
        budget(deadline)
        chunk = stream.read(CHUNK)
        budget(deadline)
        if not chunk:
            return count, digest.hexdigest()
        count += len(chunk)
        require(count <= maximum, 'sdk-qualification-size')
        digest.update(chunk)
        if destination is not None:
            destination.write(chunk)


class RawTar:
    def __init__(self, stream, maximum, deadline):
        self.stream, self.maximum, self.deadline = stream, maximum, deadline
        self.count, self.digest = 0, hashlib.sha256()

    def read(self, size):
        require(type(size) is int and 0 <= size <= CHUNK, 'sdk-qualification-read-bound')
        budget(self.deadline)
        data = self.stream.read(size)
        self.count += len(data)
        require(self.count <= self.maximum, 'sdk-qualification-raw-limit')
        budget(self.deadline)
        self.digest.update(data)
        return data


def canonical_tar(compressed, pin, deadline):
    rows, seen, logical, versions = [], set(), 0, []
    anchor = sdk.UNCOMPRESSED_TAR
    with gzip.GzipFile(fileobj=compressed, mode='rb') as inflated:
        raw = RawTar(inflated, anchor['bytes'], deadline)
        with tarfile.open(fileobj=raw, mode='r|') as archive:
            for member in archive:
                budget(deadline)
                name = PurePosixPath(member.name)
                require(not name.is_absolute() and '..' not in name.parts and '\\' not in member.name and
                        len(member.name.encode()) <= MAX_PATH_BYTES and all(ord(c) >= 32 for c in member.name),
                        'sdk-qualification-member-path')
                normalized = str(name)
                require(normalized not in seen and len(rows) < MAX_MEMBERS, 'sdk-qualification-members')
                seen.add(normalized)
                require(member.isfile() or member.isdir() or member.issym() or member.islnk(), 'sdk-qualification-member-type')
                if member.isfile():
                    logical += member.size
                    require(logical <= sdk.MAX_UNCOMPRESSED_BYTES, 'sdk-qualification-logical-limit')
                    content, count, settings = hashlib.sha256(), 0, bytearray()
                    is_settings = name.name == 'SDKSettings.plist'
                    require(not is_settings or member.size <= MAX_PLIST_BYTES, 'sdk-qualification-settings-limit')
                    with archive.extractfile(member) as source:
                        while chunk := source.read(CHUNK):
                            budget(deadline)
                            count += len(chunk); content.update(chunk)
                            if is_settings:
                                settings.extend(chunk)
                    require(count == member.size, 'sdk-qualification-member-size')
                    value, kind, size = content.hexdigest(), 'file', member.size
                    if is_settings:
                        data = plistlib.loads(settings)
                        require(type(data) is dict and type(data.get('Version')) is str, 'sdk-qualification-version')
                        versions.append(data['Version'])
                elif member.isdir():
                    value, kind, size = '', 'dir', 0
                else:
                    target = PurePosixPath(member.linkname)
                    require(not target.is_absolute() and '\\' not in member.linkname and
                            len(member.linkname.encode()) <= MAX_PATH_BYTES, 'sdk-qualification-link')
                    parts = list(name.parent.parts) if member.issym() else []
                    for part in target.parts:
                        if part == '..':
                            require(bool(parts), 'sdk-qualification-link')
                            parts.pop()
                        elif part != '.':
                            parts.append(part)
                    value = hashlib.sha256(member.linkname.encode()).hexdigest()
                    kind, size = ('symlink' if member.issym() else 'hardlink'), member.size
                rows.append([normalized, kind, size, value])
        # Include TAR padding/trailing bytes and validate the complete gzip footer.
        while raw.read(CHUNK):
            pass
    require(raw.count == anchor['bytes'] and raw.digest.hexdigest() == anchor['sha256'], 'sdk-qualification-raw-anchor')
    encoded = json.dumps(sorted(rows), ensure_ascii=False, separators=(',', ':')).encode()
    require(len(encoded) <= MAX_CANONICAL_BYTES, 'sdk-qualification-canonical-limit')
    tree = hashlib.sha256(encoded).hexdigest()
    require(tree == pin['draftReaderTreeSHA256'], 'sdk-qualification-tree')
    require(versions == [pin['version']], 'sdk-qualification-version')
    budget(deadline)
    return {'uncompressedTar': dict(anchor), 'tree': {'members': len(rows), 'logicalBytes': logical,
            'canonicalSHA256': tree}, 'version': versions[0]}


def qualify(zip_path, tar_path=None, *, target='retained'):
    """Verify fixed wire bytes and all existing content anchors; never acquire bytes."""
    deadline = time.monotonic() + MAX_SECONDS
    try:
        baseline()
        pin, producer, producer_name = origin(target)
        with ExitStack() as stack:
            wire = stack.enter_context(owned_file(zip_path, pin['zipBytes'], deadline=deadline))
            size, sha = checksum(wire, pin['zipBytes'], deadline)
            require(size == pin['zipBytes'] and sha == pin['zipSHA256'], 'sdk-qualification-zip')
            wire.seek(0)
            payload = stack.enter_context(owned_file(tar_path, pin['tarBytes'], deadline=deadline)) if tar_path is not None else stack.enter_context(tempfile.TemporaryFile('w+b'))
            with zipfile.ZipFile(wire) as archive:
                require(archive.namelist() == [pin['entry']], 'sdk-qualification-zip-layout')
                info = archive.getinfo(pin['entry'])
                require(info.file_size == pin['tarBytes'] and not info.flag_bits & 1, 'sdk-qualification-zip-entry')
                with archive.open(info) as entry:
                    count, tar_sha = checksum(entry, pin['tarBytes'], deadline, payload if tar_path is None else None)
                require(count == pin['tarBytes'], 'sdk-qualification-tar-size')
            if pin['tarSHA256'] is not None:
                require(tar_sha == pin['tarSHA256'], 'sdk-qualification-tar')
            if tar_path is not None:
                payload.seek(0)
                require(checksum(payload, pin['tarBytes'], deadline) == (count, tar_sha), 'sdk-qualification-tar-binding')
            payload.seek(0)
            content = canonical_tar(payload, pin, deadline)
            pin['tarSHA256'] = tar_sha
        budget(deadline)
        config = {'schema': 1, 'qualification': sdk.QUALIFICATION, 'baselinePlanSHA256': sdk.BASELINE_PLAN_SHA256,
            'baselineTarSHA256': sdk.BASELINE_TAR_SHA256, 'runtimeRef': sdk.RUNTIME_REF,
            'producerJob': producer, 'pin': pin, 'uncompressedTar': dict(sdk.UNCOMPRESSED_TAR)}
        return {'schema': 1, 'status': 'OFFLINE_CONTENT_QUALIFIED_PIN_REVIEW_REQUIRED', 'target': target,
            'artifact': pin['artifact'], 'run': pin['run'], 'producerJob': producer, 'producerName': producer_name,
            'zip': {'bytes': size, 'sha256': sha}, 'tar': {'bytes': count, 'sha256': tar_sha}, **content,
            'sourceCandidate': config, 'sourceAutomaticallyAdopted': False, 'payloadAcquired': False,
            'qualification': 'SDK bytes only; not fresh API/expiry admission, build or native authentication.'}
    except Failure:
        raise
    except (OSError, EOFError, ValueError, TypeError, KeyError, tarfile.TarError, zipfile.BadZipFile, zlib.error):
        raise Failure('sdk-qualification-read') from None


def propose(zip_path, tar_path=None, *, target='candidate'):
    """Generate only fixed constant edits after requalifying the actual local bytes."""
    result = qualify(zip_path, tar_path, target=target)
    original = Path(sdk.__file__).read_text()
    tree = ast.parse(original)
    expected = {'PIN': sdk.PIN, 'PRODUCER_JOB': sdk.PRODUCER_JOB, 'PRODUCER_NAME': sdk.PRODUCER_NAME}
    changes = {'PIN': result['sourceCandidate']['pin'], 'PRODUCER_JOB': result['producerJob'],
               'PRODUCER_NAME': result['producerName']}
    edits = []
    for node in tree.body:
        if isinstance(node, ast.Assign) and len(node.targets) == 1 and isinstance(node.targets[0], ast.Name) and node.targets[0].id in expected:
            key = node.targets[0].id
            require(sdk._same(ast.literal_eval(node.value), expected[key]), 'sdk-qualification-module-binding')
            edits.append((node.lineno - 1, node.end_lineno, key + ' = ' + repr(changes[key]) + '\n'))
    require(len(edits) == 3, 'sdk-qualification-module-binding')
    lines = original.splitlines(keepends=True)
    for first, last, text in sorted(edits, reverse=True):
        lines[first:last] = [text]
    candidate = ''.join(lines)
    for module in (tree, ast.parse(candidate)):
        module.body = [node for node in module.body if not (isinstance(node, ast.Assign) and
            isinstance(node.targets[0], ast.Name) and node.targets[0].id in changes)]
    require(ast.dump(tree) == ast.dump(module), 'sdk-qualification-module-delta')
    return result, candidate, ''.join(difflib.unified_diff(original.splitlines(keepends=True), candidate.splitlines(keepends=True),
        fromfile='current/sdk_source.py', tofile='NOT_ADMITTED/sdk_source.py'))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--zip', required=True, type=Path)
    parser.add_argument('--tar', type=Path)
    parser.add_argument('--target', required=True, choices=('retained', 'candidate'))
    parser.add_argument('--output', required=True, type=Path)
    args = parser.parse_args()
    result, module, patch = propose(args.zip, args.tar, target=args.target)
    args.output.mkdir(mode=0o700, exist_ok=False)
    for name, value in (('qualification.json', json.dumps(result, indent=2, sort_keys=True)+'\n'),
            ('sdk_source.candidate.json', json.dumps(result['sourceCandidate'], indent=2, sort_keys=True)+'\n'),
            ('sdk_source.candidate.py', module), ('sdk_source-pin-update.patch', patch)):
        fd = os.open(args.output/name, os.O_WRONLY|os.O_CREAT|os.O_EXCL|os.O_NOFOLLOW, 0o600)
        with os.fdopen(fd, 'w') as stream:
            stream.write(value)
    print(json.dumps({'status': result['status'], 'artifact': result['artifact'], 'tar': result['tar'],
        'sourceAutomaticallyAdopted': False, 'stage': 'NOT_ADMITTED'}))
    return 0


if __name__ == '__main__':
    try:
        raise SystemExit(main())
    except Exception as error:
        print(json.dumps({'status':'unavailable','code':error.args[0] if isinstance(error, Failure) else 'sdk-qualification-unavailable',
            'sourceAutomaticallyAdopted':False}))
        raise SystemExit(1)
