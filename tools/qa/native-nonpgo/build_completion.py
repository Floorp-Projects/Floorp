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
MAX_LINE_BYTES = 1024**2
MAX_MARKERS = 250000
MAX_ROWS = 4000000
MAX_ENTRIES = 4096
PROCESSING_SECONDS = 30
READ_BYTES = 64 * 1024
MAX_DEPTH = 64
MAX_OBJECT_KEYS = 1024
MAX_KEY_BYTES = 256
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


_FIELDS = ('data', 'name', 'startTime', 'endTime', 'phase', 'category', 'stack')
_THREAD = ('threads', 0)
_MARKERS = (*_THREAD, 'markers')
_TRACK = frozenset(((), ('meta',), ('threads',), _THREAD, (*_THREAD, 'stringArray'),
                    _MARKERS, (*_MARKERS, 'length'), *[(*_MARKERS, key) for key in _FIELDS]))
_NONSPACE = re.compile(rb'[^ \t\r\n]')
_SPECIAL = re.compile(rb'["\\\x00-\x1f]')
_DELIMITER = re.compile(rb'[ \t\r\n,\]}]')
_DECODER = json.JSONDecoder(object_pairs_hook=_pairs, parse_float=_float, parse_constant=_constant)
_UNDECODED = object()


def _key_size(key):
    try:
        _need(len(key.encode('utf-8')) <= MAX_KEY_BYTES, 'key-limit')
    except UnicodeError:
        raise _Unknown('json') from None


def _bounded_decoded(value, depth):
    """Bound a decoder's small value, including ignored nested containers."""
    _need(depth <= MAX_DEPTH, 'depth-limit')
    if type(value) is dict:
        _need(len(value) <= MAX_OBJECT_KEYS, 'key-limit')
        for key in value:
            _key_size(key)
        children = value.values()
    elif type(value) is list:
        _need(len(value) <= MAX_ROWS, 'row-limit')
        children = value
    else:
        return
    _need(not value or depth < MAX_DEPTH, 'depth-limit')
    for child in children:
        if type(child) in (dict, list):
            _bounded_decoded(child, depth + 1)


class _JSONStream:
    """Validate JSON and index selected spans without retaining the document.

    pread gives every column its own cursor over the same verified descriptor.
    No independent pathname open, shared descriptor offset, or temporary copy
    is needed. Object keys, depth and individual decoded values are bounded.
    """
    def __init__(self, descriptor, start, end, deadline):
        self.fd, self.pos, self.end, self.deadline = descriptor, start, end, deadline
        self.base, self.buffer, self.index = start, b'', {}
        self.text = None
        self.failed_batch_base = None

    def buffered(self):
        if self.pos == self.end:
            return False
        _need(0 <= self.pos < self.end, 'json')
        if not self.base <= self.pos < self.base + len(self.buffer):
            _budget(self.deadline)
            size = min(READ_BYTES, MAX_LINE_BYTES, self.end - self.pos)
            self.buffer = os.pread(self.fd, size, self.pos)
            _need(len(self.buffer) == size, 'file-changed')
            self.base = self.pos
            self.text = self.buffer.decode('ascii') if self.buffer.isascii() else None
        return True

    def peek(self):
        return self.buffer[self.pos - self.base] if self.buffered() else None

    def whitespace(self):
        while self.buffered():
            match = _NONSPACE.search(self.buffer, self.pos - self.base)
            self.pos = self.base + (match.start() if match else len(self.buffer))
            if match:
                break

    def take(self, byte):
        _need(self.peek() == byte, 'json')
        self.pos += 1

    def raw(self, start, limit=None):
        limit = MAX_LINE_BYTES if limit is None else limit
        size = self.pos - start
        _need(0 < size <= limit, 'value-limit')
        if self.base <= start and self.pos <= self.base + len(self.buffer):
            return self.buffer[start - self.base:self.pos - self.base]
        raw = os.pread(self.fd, size, start)
        _need(len(raw) == size, 'file-changed')
        return raw

    def scalar(self):
        start = self.pos
        if self.peek() == 34:
            self.pos += 1
            while True:
                _need(self.buffered(), 'json')
                match = _SPECIAL.search(self.buffer, self.pos - self.base)
                if match is None:
                    self.pos = self.base + len(self.buffer)
                else:
                    self.pos = self.base + match.start() + 1
                    byte = self.buffer[match.start()]
                    if byte == 34:
                        break
                    _need(byte == 92 and self.peek() is not None, 'json')
                    self.pos += 1
                _need(self.pos - start <= MAX_LINE_BYTES, 'value-limit')
        else:
            while self.buffered():
                match = _DELIMITER.search(self.buffer, self.pos - self.base)
                self.pos = self.base + (match.start() if match else len(self.buffer))
                _need(self.pos - start <= MAX_LINE_BYTES, 'value-limit')
                if match:
                    break
        return _decode(self.raw(start))

    @staticmethod
    def child(path, key):
        candidate = (*path, key) if path is not None else None
        return candidate if candidate in _TRACK else None

    def small_value(self, depth):
        # The upstream serializer escapes non-ASCII by default. On an ASCII
        # chunk, character offsets equal verified byte offsets. Decode only a
        # complete value contained in this bounded chunk; otherwise use the
        # structural cursor. Reuse that value when iterating marker columns.
        if self.text is None:
            return _UNDECODED
        start = self.pos - self.base
        try:
            value, end = _DECODER.raw_decode(self.text, start)
        except (ValueError, RecursionError):
            return _UNDECODED
        if end == len(self.buffer):
            if self.base + end != self.end:
                return _UNDECODED
        elif self.buffer[end] not in b' \t\r\n,]}':
            return _UNDECODED
        _need(end - start <= MAX_LINE_BYTES, 'value-limit')
        _bounded_decoded(value, depth)
        self.pos = self.base + end
        return value

    def array_batch(self, depth):
        # Validate a complete comma-terminated prefix in C. A failed candidate
        # falls back once per chunk, including delimiters inside strings or
        # nested containers. Never batch a child whose span must be indexed.
        self.whitespace()
        if not self.buffered() or self.text is None or self.failed_batch_base == self.base:
            return _UNDECODED
        start = self.pos - self.base
        ending = {123: b'},', 91: b'],'}.get(self.buffer[start])
        cut = self.buffer.rfind(ending if ending else b',', start)
        if ending and cut >= 0:
            cut += 1
        if cut <= start:
            return _UNDECODED
        try:
            values = _decode(b'[' + self.buffer[start:cut] + b']')
        except _Unknown:
            self.failed_batch_base = self.base
            return _UNDECODED
        if not values:
            return _UNDECODED
        _bounded_decoded(values, depth)
        self.pos = self.base + cut + 1
        return values

    def value(self, path=None, depth=0):
        _budget(self.deadline)
        _need(depth <= MAX_DEPTH, 'depth-limit')
        self.whitespace()
        start, byte, count, keys, scalar = self.pos, self.peek(), 0, None, None
        if path is None:
            decoded = self.small_value(depth)
            if decoded is not _UNDECODED:
                return decoded
        if byte == 123:
            self.pos += 1
            keys = set()
            self.whitespace()
            if self.peek() != 125:
                while True:
                    self.whitespace()
                    _need(self.peek() == 34, 'json')
                    key = self.scalar()
                    _key_size(key)
                    _need(key not in keys, 'json')
                    keys.add(key)
                    _need(len(keys) <= MAX_OBJECT_KEYS, 'key-limit')
                    self.whitespace(); self.take(58)
                    self.value(self.child(path, key), depth + 1)
                    self.whitespace()
                    if self.peek() != 44:
                        break
                    self.pos += 1
            self.take(125)
            kind = 'object'
        elif byte == 91:
            self.pos += 1
            self.whitespace()
            if self.peek() != 93:
                while True:
                    _need(count < MAX_ROWS, 'row-limit')
                    if self.child(path, count) is None:
                        batch = self.array_batch(depth)
                        if batch is not _UNDECODED:
                            count += len(batch)
                            _need(count <= MAX_ROWS, 'row-limit')
                            self.whitespace()
                            continue
                    self.value(self.child(path, count), depth + 1)
                    count += 1
                    self.whitespace()
                    if self.peek() != 44:
                        break
                    self.pos += 1
            self.take(93)
            kind = 'array'
        else:
            scalar = self.scalar()
            kind = 'scalar'
        if path is not None:
            self.index[path] = {'kind': kind, 'start': start, 'end': self.pos,
                                'count': count, 'keys': keys, 'value': scalar}
        return scalar if kind == 'scalar' else _UNDECODED

    def finish(self):
        self.whitespace()
        _need(self.pos == self.end, 'json')


def _array_values(descriptor, span, deadline):
    reader = _JSONStream(descriptor, span['start'] + 1, span['end'] - 1, deadline)
    remaining = span['count']
    while remaining:
        reader.whitespace()
        batch = reader.array_batch(0)
        if batch is not _UNDECODED:
            _need(len(batch) < remaining, 'profile-schema')
            remaining -= len(batch)
            yield from batch
            continue
        start = reader.pos
        value = reader.value()
        if value is _UNDECODED:
            value = _decode(reader.raw(start))
        reader.whitespace()
        remaining -= 1
        if remaining:
            reader.take(44)
        else:
            reader.finish()
        yield value
    reader.finish()


def _whole_stream(stream, size, completed, deadline):
    reader = _JSONStream(stream.fileno(), 0, size, deadline)
    reader.value(())
    reader.finish()
    index = reader.index
    def span(path, kind):
        item = index.get(path)
        _need(item is not None and item['kind'] == kind, 'profile-schema')
        return item
    span((), 'object')
    meta = span(('meta',), 'object')
    _need(meta['end'] - meta['start'] <= MAX_LINE_BYTES, 'value-limit')
    reader.pos = meta['end']
    _meta(_decode(reader.raw(meta['start'])))
    _need(span(('threads',), 'array')['count'] == 1, 'profile-schema')
    span(_THREAD, 'object')
    strings = span((*_THREAD, 'stringArray'), 'array')
    _need(span(_MARKERS, 'object')['keys'] == {*_FIELDS, 'length'}, 'profile-schema')
    length = span((*_MARKERS, 'length'), 'scalar')['value']
    _need(type(length) is int and 0 <= length <= MAX_ROWS, 'profile-schema')
    columns = {key: span((*_MARKERS, key), 'array') for key in _FIELDS}
    _need(all(item['count'] == length for item in columns.values()), 'profile-schema')
    names = ('Other', 'Phase', *KINDS)
    codes = bytearray()
    for value in _array_values(reader.fd, strings, deadline):
        _need(type(value) is str, 'profile-schema')
        codes.append(names.index(value) if value in names else 0)
    values = [_array_values(reader.fd, columns[key], deadline)
              for key in ('data', 'name', 'startTime', 'endTime', 'phase')]
    for data, name, start, end, phase in zip(*values):
        _budget(deadline)
        _need(type(name) is int and 0 <= name < len(codes) and type(phase) is int and
              ((phase == 0 and end is None) or (phase == 1 and end is not None)), 'profile-schema')
        completed.marker(names[codes[name]], start, end, data)


def _parse(stream, size, deadline):
    first = stream.readline(MAX_LINE_BYTES + 1)
    completed = _Completed()
    try:
        meta = _decode(first) if len(first) <= MAX_LINE_BYTES else None
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
        _whole_stream(stream, size, completed, deadline)
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
