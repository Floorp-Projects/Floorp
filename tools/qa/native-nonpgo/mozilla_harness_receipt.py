# SPDX-License-Identifier: MPL-2.0
"""Read current Mozilla mozlog JSONL without publishing private log content.

The caller separately validates the real child exit code and owned cleanup.
This reader grants neither native authentication nor build qualification.
"""
import hashlib
import json
import os
from pathlib import Path
import stat


MAX_BYTES = 16 * 1024**2
MAX_LINE_BYTES = 256 * 1024
MAX_RECORDS = 200000
MAX_TESTS = 128
END_STATUSES = ('PASS', 'FAIL', 'OK', 'ERROR', 'TIMEOUT', 'CRASH', 'ASSERT',
                'PRECONDITION_FAILED', 'SKIP')
SUB_STATUSES = tuple(value for value in END_STATUSES if value not in ('OK', 'CRASH')) + ('NOTRUN',)
FAILURE_ACTIONS = ('crash', 'shutdown_failure', 'valgrind_error', 'lsan_leak', 'tsan_error')
PASSIVE_ACTIONS = ('process_start', 'add_subsuite', 'buffering_on', 'buffering_off', 'shutdown')


class _Invalid(Exception):
    def __init__(self, reason):
        self.reason = 'mozilla-harness-' + reason


def _need(condition, reason):
    if not condition:
        raise _Invalid(reason)


def _text(value, maximum=4096):
    return type(value) is str and 0 < len(value) <= maximum and all(ord(c) >= 32 and ord(c) != 127 for c in value)


def _integer(value):
    return type(value) is int and value >= 0


def _inputs(expected, mode, debug_marker, failure_marker):
    _need(type(mode) is str and mode in ('positive', 'negative-control'), 'input')
    _need(type(expected) in (list, tuple, set, frozenset) and 0 < len(expected) <= MAX_TESTS and
          all(_text(value) for value in expected), 'input')
    selected = set(expected)
    _need(len(selected) == len(expected), 'input')
    _need(debug_marker is None or _text(debug_marker, 256), 'input')
    _need((mode == 'positive' and failure_marker is None) or
          (mode == 'negative-control' and len(selected) == 1 and _text(failure_marker, 256)), 'input')
    return selected


def _identity(value):
    return tuple(getattr(value, key) for key in ('st_dev', 'st_ino', 'st_mode', 'st_uid',
                                               'st_size', 'st_mtime_ns', 'st_ctime_ns'))


def _read(path):
    _need(type(path) is str or isinstance(path, Path), 'input')
    _need(hasattr(os, 'O_NOFOLLOW') and hasattr(os, 'O_NONBLOCK'), 'file-platform')
    path = Path(path)
    before = path.lstat()
    _need(stat.S_ISREG(before.st_mode) and before.st_uid == os.geteuid(), 'file-kind')
    _need(0 < before.st_size <= MAX_BYTES, 'file-size')
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK |
                         getattr(os, 'O_CLOEXEC', 0))
    try:
        opened = os.fstat(descriptor)
        _need(_identity(before) == _identity(opened), 'file-changed')
        chunks, count = [], 0
        while True:
            block = os.read(descriptor, min(65536, MAX_BYTES + 1 - count))
            if not block:
                break
            chunks.append(block)
            count += len(block)
            _need(count <= MAX_BYTES, 'file-size')
        after = os.fstat(descriptor)
        _need(_identity(before) == _identity(after) == _identity(path.lstat()) and count == before.st_size,
              'file-changed')
    finally:
        os.close(descriptor)
    return b''.join(chunks)


def _pairs(values):
    result = {}
    for key, value in values:
        _need(key not in result, 'json')
        result[key] = value
    return result


def _constant(value):
    raise _Invalid('json')


def _records(raw):
    _need(raw.endswith(b'\n'), 'partial-log')
    try:
        text = raw.decode('utf-8', errors='strict')
    except UnicodeDecodeError:
        raise _Invalid('utf8') from None
    lines = text.split('\n')[:-1]
    _need(0 < len(lines) <= MAX_RECORDS, 'record-cap')
    for line in lines:
        _need(bool(line) and len(line.encode('utf-8')) <= MAX_LINE_BYTES, 'line-size')
        try:
            value = json.loads(line, object_pairs_hook=_pairs, parse_constant=_constant)
        except (ValueError, RecursionError):
            raise _Invalid('json') from None
        _need(type(value) is dict and _text(value.get('action'), 80) and _integer(value.get('time')),
              'event-shape')
        yield value


def _parse(raw, expected, debug_marker, failure_marker):
    counts = {'expectedTests': len(expected), 'listedTests': 0, 'suiteStarts': 0, 'suiteEnds': 0,
              'testStarts': 0, 'testEnds': 0, 'subtests': 0, 'unexpectedSubtests': 0,
              'unexpectedTestEnds': 0, 'unexpectedFailSubtests': 0, 'unexpectedFailTestEnds': 0,
              'skips': 0, 'crashes': 0, 'timeouts': 0,
              'otherFailures': 0, 'errorLogs': 0, 'nonzeroProcessExits': 0,
              'testEndStatuses': dict.fromkeys(END_STATUSES, 0),
              'subtestStatuses': dict.fromkeys(SUB_STATUSES, 0)}
    started, ended, groups = set(), set(), set()
    in_suite = False
    debug_found = failure_found = False
    for row in _records(raw):
        action = row['action']
        if action == 'suite_start':
            _need(not in_suite and counts['suiteStarts'] == counts['suiteEnds'] == 0, 'suite-order')
            tests = row.get('tests')
            _need(type(tests) is dict and bool(tests) and all(_text(group) and type(ids) is list
                  and all(_text(test) for test in ids) for group, ids in tests.items()), 'suite-tests')
            listed = [test for ids in tests.values() for test in ids]
            _need(len(listed) == len(set(listed)) and set(listed) == expected, 'selected-tests')
            counts['listedTests'], counts['suiteStarts'], in_suite = len(listed), 1, True
        elif action == 'suite_end':
            _need(in_suite and counts['suiteEnds'] == 0 and started == ended == expected and not groups,
                  'suite-order')
            counts['suiteEnds'], in_suite = 1, False
        elif action in ('test_start', 'test_status', 'test_end'):
            test = row.get('test')
            _need(in_suite and type(test) is str and test in expected, 'test-order')
            if action == 'test_start':
                _need(test not in started, 'duplicate-test')
                started.add(test)
                counts['testStarts'] += 1
                continue
            _need(test in started and test not in ended, 'test-order')
            allowed = SUB_STATUSES if action == 'test_status' else END_STATUSES
            status = row.get('status')
            _need(type(status) is str and status in allowed, 'status-shape')
            if 'expected' in row:
                _need(type(row['expected']) is str and row['expected'] in allowed, 'status-shape')
            if 'known_intermittent' in row:
                _need(type(row['known_intermittent']) is list and not row['known_intermittent'],
                      'intermittent-status')
            if 'message' in row:
                _need(type(row['message']) is str, 'status-shape')
            subtest = action == 'test_status'
            if subtest:
                _need(type(row.get('subtest')) is str, 'status-shape')
                counts['subtests'] += 1
            else:
                ended.add(test)
                counts['testEnds'] += 1
            counts['subtestStatuses' if subtest else 'testEndStatuses'][status] += 1
            unexpected = 'expected' in row and row['expected'] != status
            counts['unexpectedSubtests' if subtest else 'unexpectedTestEnds'] += int(unexpected)
            counts['unexpectedFailSubtests' if subtest else 'unexpectedFailTestEnds'] += int(
                status == 'FAIL' and row.get('expected') == 'PASS')
            counts['skips'] += int(status == 'SKIP')
            counts['crashes'] += int(status == 'CRASH')
            counts['timeouts'] += int(status == 'TIMEOUT')
            counts['otherFailures'] += int(status not in ('PASS', 'FAIL', 'SKIP', 'CRASH', 'TIMEOUT'))
            if subtest and status == 'FAIL' and row.get('expected') == 'PASS' and failure_marker is not None:
                failure_found |= row['subtest'] == failure_marker or row.get('message') == failure_marker
        elif action == 'log':
            _need(type(row.get('level')) is str and row['level'] in
                  ('DEBUG', 'INFO', 'WARNING', 'ERROR', 'CRITICAL') and type(row.get('message')) is str,
                  'log-shape')
            message = row['message']
            counts['errorLogs'] += int(row['level'] in ('ERROR', 'CRITICAL') or
                                       message.startswith('TEST-UNEXPECTED'))
            counts['crashes'] += int('PROCESS-CRASH' in message)
            counts['timeouts'] += int('TEST-UNEXPECTED-TIMEOUT' in message)
            active = started - ended
            debug_found |= (debug_marker is not None and row['level'] == 'INFO' and message == debug_marker and
                            in_suite and len(active) == 1 and
                            ('test' not in row or (type(row['test']) is str and row['test'] in active)))
        elif action == 'process_output':
            _need(type(row.get('data')) is str, 'process-shape')
            counts['errorLogs'] += int('TEST-UNEXPECTED' in row['data'])
            counts['crashes'] += int('PROCESS-CRASH' in row['data'])
            counts['timeouts'] += int('TEST-UNEXPECTED-TIMEOUT' in row['data'])
        elif action == 'process_exit':
            _need(type(row.get('exitcode')) is int, 'process-shape')
            counts['nonzeroProcessExits'] += int(row['exitcode'] != 0)
        elif action == 'assertion_count':
            _need(all(_integer(row.get(key)) for key in ('count', 'min_expected', 'max_expected')) and
                  row['min_expected'] <= row['max_expected'], 'assertion-shape')
            counts['otherFailures'] += int(not row['min_expected'] <= row['count'] <= row['max_expected'])
        elif action in ('group_start', 'group_end'):
            name = row.get('name')
            _need(in_suite and _text(name), 'group-order')
            if action == 'group_start':
                _need(name not in groups, 'group-order')
                groups.add(name)
            else:
                _need(name in groups, 'group-order')
                groups.remove(name)
        elif action in FAILURE_ACTIONS:
            counts['crashes' if action == 'crash' else 'otherFailures'] += 1
        elif action not in PASSIVE_ACTIONS:
            raise _Invalid('unknown-action')
    _need(not in_suite and counts['suiteStarts'] == counts['suiteEnds'] == 1 and started == ended == expected,
          'incomplete-suite')
    return counts, debug_found, failure_found


def receipt(path, expected_test_ids, *, mode='positive', debug_marker=None, failure_marker=None):
    """Return safe evidence; negative control retains actual failed status.

    Unreadable, unstable, malformed, partial, or ambiguous records yield
    unavailable with unknown counts. Only a complete exact test lifecycle is
    evaluated. Raw IDs, messages, paths, process metadata, and markers stay out.
    """
    result = {'schema': 1, 'mode': mode if type(mode) is str and mode in
              ('positive', 'negative-control') else None, 'status': 'unavailable',
              'reason': None, 'expectationSatisfied': False, 'completeLog': False,
              'bytes': None, 'sha256': None, 'counts': None,
              'markers': {'debug': None, 'failure': None},
              'nativeQualification': False, 'buildQualification': False}
    try:
        selected = _inputs(expected_test_ids, mode, debug_marker, failure_marker)
        raw = _read(path)
        result.update(bytes=len(raw), sha256=hashlib.sha256(raw).hexdigest())
        counts, debug_found, failure_found = _parse(raw, selected, debug_marker, failure_marker)
        result.update(completeLog=True, counts=counts,
                      markers={'debug': debug_found if debug_marker is not None else None,
                               'failure': failure_found if failure_marker is not None else None})
        clean = not any(counts[key] for key in ('skips', 'crashes', 'timeouts', 'otherFailures',
                                               'errorLogs', 'nonzeroProcessExits'))
        if mode == 'positive':
            satisfied = (clean and counts['testEndStatuses']['PASS'] == len(selected) and
                counts['subtestStatuses']['PASS'] == counts['subtests'] and
                counts['unexpectedSubtests'] == counts['unexpectedTestEnds'] == 0)
            result.update(status='passed' if satisfied else 'failed', expectationSatisfied=satisfied,
                          reason='mozilla-harness-positive-passed' if satisfied else 'mozilla-harness-positive-failed')
        else:
            satisfied = (clean and counts['testEndStatuses']['FAIL'] == 1 and
                counts['subtestStatuses']['FAIL'] == 1 and
                counts['subtestStatuses']['PASS'] + 1 == counts['subtests'] and
                counts['unexpectedSubtests'] == counts['unexpectedTestEnds'] == 1 and
                counts['unexpectedFailSubtests'] == counts['unexpectedFailTestEnds'] == 1 and failure_found)
            result.update(status='failed', expectationSatisfied=satisfied,
                          reason='mozilla-harness-negative-control-detected' if satisfied else
                          'mozilla-harness-negative-control-mismatch')
        if debug_marker is not None and not debug_found:
            result.update(status='failed', expectationSatisfied=False, reason='mozilla-harness-debug-marker-missing')
    except _Invalid as error:
        result['reason'] = error.reason
    except OSError:
        result['reason'] = 'mozilla-harness-file-read'
    return result
