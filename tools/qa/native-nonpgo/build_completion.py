# SPDX-License-Identifier: MPL-2.0
"""Observe finished current-source build markers after owned-stage cleanup.

The snapshot is private. The public result proves neither build success nor
cache hits, an active phase, remaining work, or a duration to completion.
"""
from contextlib import ExitStack
import json
import math
import os
from pathlib import Path
import re
import stat
import time


MAX_BYTES = 2 * 1024**3
MAX_WHOLE_BYTES = 16 * 1024**2
MAX_LINE_BYTES = 1024**2
MAX_MARKERS = 250000
MAX_ROWS = 4000000
MAX_ENTRIES = 4096
PROCESSING_SECONDS = 30
TIERS = ('configure', 'pre-export', 'export', 'pre-compile', 'rust', 'compile',
         'misc', 'libs', 'tools', 'check')
KINDS = ('Object', 'Program', 'SharedLib', 'StaticLib', 'Rust')
TARGETS = {'XUL': 'SharedLib', 'xpcshell': 'Program', 'MacWebAppService.o': 'Object'}
PARTS = ('obj', '.mozbuild', 'logs', 'build')
PROFILE = re.compile(r'profile_log_[0-9]{8}_[0-9]{6}\.json\Z')


class _Unknown(Exception):
    def __init__(self, reason):
        self.reason = 'build-completion-' + reason


def _need(condition, reason):
    if not condition:
        raise _Unknown(reason)


def _identity(value, full=False):
    fields = ('st_dev', 'st_ino', 'st_uid')
    if full:
        fields += ('st_mode', 'st_size', 'st_mtime_ns', 'st_ctime_ns')
    return [getattr(value, field) for field in fields]


def _directory(stack, name, parent=None, owned=True):
    flags = os.O_RDONLY | os.O_DIRECTORY | os.O_NOFOLLOW | getattr(os, 'O_CLOEXEC', 0)
    descriptor = os.open(name, flags, dir_fd=parent)
    stack.callback(os.close, descriptor)
    value = os.fstat(descriptor)
    _need(stat.S_ISDIR(value.st_mode) and (not owned or value.st_uid == os.geteuid()), 'directory')
    return descriptor, _identity(value)


def _directories(stack, task):
    _need(isinstance(task, (Path, str)) and hasattr(os, 'O_NOFOLLOW'), 'input')
    path = Path(task)
    _need(path.is_absolute() and len(path.parts) > 1 and '..' not in path.parts, 'input')
    descriptor, _ = _directory(stack, '/', owned=False)
    for part in path.parts[1:]:
        descriptor, task_identity = _directory(stack, part, descriptor, owned=False)
    value = os.fstat(descriptor)
    _need(value.st_uid == os.geteuid(), 'directory')
    identities = [task_identity]
    for part in PARTS:
        try:
            descriptor, identity = _directory(stack, part, descriptor)
        except FileNotFoundError:
            return None, identities
        identities.append(identity)
    return descriptor, identities


def _profiles(descriptor):
    if descriptor is None:
        return {}
    found = {}
    with os.scandir(descriptor) as entries:
        for count, entry in enumerate(entries, 1):
            _need(count <= MAX_ENTRIES, 'directory-limit')
            if not PROFILE.fullmatch(entry.name):
                continue
            value = entry.stat(follow_symlinks=False)
            _need(stat.S_ISREG(value.st_mode) and value.st_uid == os.geteuid() and value.st_nlink == 1,
                  'file-kind')
            found[entry.name] = _identity(value, full=True)
    return found


def start_snapshot(task):
    """Capture private pre-build identities; do not place this dictionary in a report."""
    try:
        with ExitStack() as stack:
            descriptor, identities = _directories(stack, task)
            profiles = _profiles(descriptor)
            return {'schema': 1, 'status': 'captured', 'directoryIdentities': identities,
                    'profiles': profiles, 'wallTimeNs': time.time_ns()}
    except _Unknown as error:
        return {'schema': 1, 'status': 'unconfirmed', 'reason': error.reason}
    except Exception:
        return {'schema': 1, 'status': 'unconfirmed', 'reason': 'build-completion-snapshot-unavailable'}


def _empty(reason):
    return {'schema': 1, 'status': 'unconfirmed', 'reason': reason, 'profileFormat': None,
            'profileBytes': None, 'completeCoverage': False, 'finishedTierMarkers': None,
            'lastObservedFinishedTier': None, 'finishedTargetMarkers': None,
            'latestFinishedTargetByKind': None,
            'targets': {name: {'status': 'unconfirmed', 'markerKind': kind,
                              'startTimeMs': None, 'endTimeMs': None} for name, kind in TARGETS.items()},
            'evidenceScope': 'finished-markers-only', 'currentBuildPhase': 'UNKNOWN',
            'durationToCompletion': 'UNKNOWN', 'provesBuildCompletion': False,
            'provesCacheHit': False, 'rawPathsPIDsOrTextPublished': False}


def _budget(deadline):
    _need(time.monotonic() <= deadline, 'processing-limit')


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        _need(key not in result, 'json')
        result[key] = value
    return result


def _float(text):
    value = float(text)
    _need(math.isfinite(value), 'json')
    return value


def _constant(_):
    raise _Unknown('json')


def _decode(raw):
    try:
        return json.loads(raw.decode('utf-8'), object_pairs_hook=_pairs,
                          parse_float=_float, parse_constant=_constant)
    except (ValueError, UnicodeError, RecursionError):
        raise _Unknown('json') from None


def _time(value):
    _need(type(value) in (int, float) and math.isfinite(value) and value >= 0, 'marker-time')
    return value


def _meta(value):
    _need(type(value) is dict and value.get('product') == 'mach' and
          type(value.get('version')) is int and value['version'] == 27 and
          type(value.get('preprocessedProfileVersion')) is int and
          value['preprocessedProfileVersion'] == 47, 'metadata')


class _Completed:
    def __init__(self):
        self.tiers = dict.fromkeys(TIERS, 0)
        self.kinds = dict.fromkeys(KINDS, 0)
        self.latest = dict.fromkeys(KINDS)
        self.last_tier = None
        self.targets = _empty('unused')['targets']
        self.markers = 0
        self.rows = 0

    def marker(self, name, start, end, data):
        self.rows += 1
        _need(self.rows <= MAX_ROWS, 'row-limit')
        _need(type(name) is str and type(data) is dict and type(data.get('type')) is str, 'marker-schema')
        start = _time(start)
        phase = name == 'Phase' or data['type'] == 'Phase'
        if phase:
            _need(name == 'Phase' and data['type'] == 'Phase' and data.get('phase') in (*TIERS, 'teardown'),
                  'phase-schema')
        elif name in KINDS:
            _need(data['type'] == 'Text' and type(data.get('text')) is str, 'target-schema')
        if end is None:
            return
        end = _time(end)
        _need(end >= start, 'marker-time')
        times = {'startTimeMs': start, 'endTimeMs': end}
        if (phase and data['phase'] in TIERS) or name in KINDS:
            self.markers += 1
            _need(self.markers <= MAX_MARKERS, 'marker-limit')
        if phase:
            tier = data['phase']
            if tier == 'teardown':
                return
            self.tiers[tier] += 1
            if self.last_tier is None or end > self.last_tier['endTimeMs']:
                self.last_tier = {'tier': tier, **times}
        elif name in KINDS:
            self.kinds[name] += 1
            if self.latest[name] is None or end > self.latest[name]['endTimeMs']:
                self.latest[name] = times
            target = data['text']
            if TARGETS.get(target) == name:
                current = self.targets[target]
                if current['endTimeMs'] is None or end > current['endTimeMs']:
                    self.targets[target] = {'status': 'observed-finished', 'markerKind': name, **times}

    def result(self, kind, size):
        return _empty('build-completion-profile-parsed') | {
            'status': 'observed', 'profileFormat': kind, 'profileBytes': size, 'completeCoverage': True,
            'finishedTierMarkers': self.tiers, 'lastObservedFinishedTier': self.last_tier,
            'finishedTargetMarkers': self.kinds, 'latestFinishedTargetByKind': self.latest,
            'targets': self.targets}


def _whole(value, completed, deadline):
    _need(type(value) is dict, 'profile-schema')
    _meta(value.get('meta'))
    threads = value.get('threads')
    _need(type(threads) is list and len(threads) == 1 and type(threads[0]) is dict, 'profile-schema')
    thread = threads[0]
    strings, markers = thread.get('stringArray'), thread.get('markers')
    _need(type(strings) is list and all(type(item) is str for item in strings) and type(markers) is dict,
          'profile-schema')
    fields = ('data', 'name', 'startTime', 'endTime', 'phase', 'category', 'stack')
    length = markers.get('length')
    _need(set(markers) == {*fields, 'length'} and type(length) is int and 0 <= length <= MAX_ROWS and
          all(type(markers[key]) is list and len(markers[key]) == length for key in fields), 'profile-schema')
    for index in range(length):
        _budget(deadline)
        name = markers['name'][index]
        phase = markers['phase'][index]
        end = markers['endTime'][index]
        _need(type(name) is int and 0 <= name < len(strings) and type(phase) is int and
              ((phase == 0 and end is None) or (phase == 1 and end is not None)), 'profile-schema')
        completed.marker(strings[name], markers['startTime'][index], end, markers['data'][index])


def _parse(stream, size, deadline):
    first = stream.readline(MAX_LINE_BYTES + 1)
    completed = _Completed()
    try:
        meta = _decode(first)
    except _Unknown:
        meta = None
    if type(meta) is dict and meta.get('type') == 'meta':
        _need(len(first) <= MAX_LINE_BYTES and first.endswith(b'\n'), 'line-limit')
        _meta(meta)
        line = stream.readline(MAX_LINE_BYTES + 1)
        _need(len(line) <= MAX_LINE_BYTES and line.endswith(b'\n'), 'truncated')
        thread = _decode(line)
        _need(type(thread) is dict and thread.get('type') == 'thread' and thread.get('processName') == 'mach',
              'thread-schema')
        read_bytes = len(first) + len(line)
        while True:
            _budget(deadline)
            line = stream.readline(MAX_LINE_BYTES + 1)
            if not line:
                break
            read_bytes += len(line)
            _need(read_bytes <= MAX_BYTES and len(line) <= MAX_LINE_BYTES, 'line-limit')
            _need(line.endswith(b'\n'), 'truncated')
            row = _decode(line)
            _need(type(row) is dict and set(row) in
                  ({'type', 'name', 'startTime', 'endTime', 'data'},
                   {'type', 'name', 'startTime', 'endTime', 'data', 'category'}) and row['type'] == 'marker',
                  'marker-schema')
            completed.marker(row['name'], row['startTime'], row['endTime'], row['data'])
        _need(read_bytes == size, 'file-changed')
        kind = 'jsonl'
    else:
        _need(size <= MAX_WHOLE_BYTES, 'whole-size-limit')
        stream.seek(0)
        raw = stream.read(MAX_WHOLE_BYTES + 1)
        _need(len(raw) == size and len(raw) <= MAX_WHOLE_BYTES, 'file-changed')
        _whole(_decode(raw), completed, deadline)
        kind = 'whole-profile'
    _budget(deadline)
    return completed.result(kind, size)


def collect(task, snapshot):
    """Read once after exact leader reap and verified group cleanup; return safe evidence."""
    try:
        _need(type(snapshot) is dict and type(snapshot.get('schema')) is int and snapshot['schema'] == 1 and
              snapshot.get('status') == 'captured' and type(snapshot.get('wallTimeNs')) is int and
              snapshot['wallTimeNs'] > 0 and type(snapshot.get('profiles')) is dict and
              type(snapshot.get('directoryIdentities')) is list and
              1 <= len(snapshot['directoryIdentities']) <= len(PARTS) + 1, 'snapshot')
        _need(all(type(item) is list and len(item) == 3 and all(type(n) is int and n >= 0 for n in item)
                  for item in snapshot['directoryIdentities']) and
              all(type(name) is str and PROFILE.fullmatch(name) and type(item) is list and len(item) == 7 and
                  all(type(n) is int and n >= 0 for n in item) for name, item in snapshot['profiles'].items()),
              'snapshot')
        deadline = time.monotonic() + PROCESSING_SECONDS
        with ExitStack() as stack:
            descriptor, identities = _directories(stack, task)
            previous = snapshot['directoryIdentities']
            _need(identities[:len(previous)] == previous, 'directory-changed')
            profiles = _profiles(descriptor)
            names = [name for name in profiles if name not in snapshot['profiles']]
            _need(len(names) == 1, 'profile-ambiguous' if len(names) > 1 else 'profile-missing')
            name = names[0]
            before = os.stat(name, dir_fd=descriptor, follow_symlinks=False)
            _need(before.st_mtime_ns >= snapshot['wallTimeNs'] and before.st_ctime_ns >= snapshot['wallTimeNs'],
                  'profile-stale')
            _need(0 < before.st_size <= MAX_BYTES, 'file-size')
            flags = os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK | getattr(os, 'O_CLOEXEC', 0)
            opened = os.open(name, flags, dir_fd=descriptor)
            stack.callback(os.close, opened)
            stream = stack.enter_context(os.fdopen(opened, 'rb', closefd=False))
            _need(_identity(before, True) == _identity(os.fstat(stream.fileno()), True) == profiles[name],
                  'file-changed')
            result = _parse(stream, before.st_size, deadline)
            _need(_identity(before, True) == _identity(os.fstat(stream.fileno()), True) ==
                  _identity(os.stat(name, dir_fd=descriptor, follow_symlinks=False), True), 'file-changed')
            _, final_identities = _directories(stack, task)
            _need(final_identities == identities, 'directory-changed')
            _need(_profiles(descriptor) == profiles, 'file-changed')
            _budget(deadline)
            return result
    except _Unknown as error:
        return _empty(error.reason)
    except Exception:
        return _empty('build-completion-unavailable')
