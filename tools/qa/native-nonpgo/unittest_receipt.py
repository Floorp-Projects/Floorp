# SPDX-License-Identifier: MPL-2.0
"""Safe unittest failure identities; no arbitrary exception text or raw logs."""
import ast
import builtins
import hashlib
import os
from pathlib import Path
import re
import stat

from support import require

HERE = Path(__file__).resolve().parent


def source_identities():
    cases, phases, lines = set(), set(), {}
    for path in HERE.glob('*.py'):
        source = path.read_text()
        lines[path.name] = len(source.splitlines())
        if not path.name.startswith('test_'):
            continue
        tree = ast.parse(source)
        classes = {node.name: node for node in tree.body if isinstance(node, ast.ClassDef)}
        def methods(name, seen=None):
            seen = set() if seen is None else seen
            if name in seen or name not in classes:
                return set()
            seen.add(name)
            node = classes[name]
            result = {item.name for item in node.body if isinstance(item, ast.FunctionDef)
                      and item.name.startswith('test_')}
            for base in node.bases:
                if isinstance(base, ast.Name):
                    result.update(methods(base.id, seen))
            return result
        for name in classes:
            identity = path.stem+'.'+name
            for method in methods(name):
                cases.add((method, identity+'.'+method))
            phases.update((phase, identity) for phase in ('setUpClass','tearDownClass'))
        phases.update((phase, path.stem) for phase in ('setUpModule','tearDownModule'))
        cases.add((path.stem, 'unittest.loader._FailedTest.'+path.stem))
    return cases | phases, lines


def known_failure_codes():
    codes = set()
    for path in HERE.glob('*.py'):
        if path.name.startswith('test_'):
            continue
        for node in ast.walk(ast.parse(path.read_text())):
            if not isinstance(node, ast.Call) or not isinstance(node.func, ast.Name):
                continue
            index = 1 if node.func.id == 'require' else 0 if node.func.id == 'Failure' else None
            if index is not None and len(node.args) > index and isinstance(node.args[index], ast.Constant):
                value = node.args[index].value
                if isinstance(value, str) and re.fullmatch('[a-z][a-z0-9-]{0,95}', value):
                    codes.add(value)
    return codes


def receipt(path):
    result = {'complete': False, 'rawContentPublished': False, 'exceptionTextPublished': False,
              'total': None, 'failures': None, 'errors': None, 'skipped': None, 'status': 'unknown'}
    try:
        identities, source_lines = source_identities()
        exception_types = {name for name, value in vars(builtins).items()
                           if isinstance(value, type) and issubclass(value, BaseException)}
        exception_types.update(('support.Failure', 'subprocess.CalledProcessError',
                                'subprocess.TimeoutExpired', 'json.decoder.JSONDecodeError',
                                'ctypes.ArgumentError', 'unittest.case.SkipTest'))
        codes = known_failure_codes()
        checksum = hashlib.sha256()
        failures, counts, outcomes, current = [], [], [], None
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(descriptor, 'rb') as stream:
            info = os.fstat(stream.fileno())
            require(stat.S_ISREG(info.st_mode) and info.st_size <= 256*1024**2, 'unittest-log-bound')
            total = 0
            while raw := stream.readline(2*1024**2 + 1):
                require(len(raw) <= 2*1024**2 and raw.endswith(b'\n'), 'unittest-log-line')
                total += len(raw)
                checksum.update(raw)
                line = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', raw.decode('utf-8')).rstrip('\n\r')
                header = re.fullmatch(r'(ERROR|FAIL): ([A-Za-z_][A-Za-z_0-9]*) \(([A-Za-z_][A-Za-z_0-9.]*)\)(?: \(.*\))?', line)
                if header:
                    identity = header[3]
                    require((header[2], identity) in identities,
                            'unittest-log-test-identity')
                    require(len(failures) < 1000, 'unittest-log-case-cap')
                    current = {'outcome': header[1], 'test': identity, 'phase': header[2], 'frames': []}
                    failures.append(current)
                count = re.fullmatch(r'Ran ([0-9]+) tests? in ([0-9.]+)s', line)
                if count:
                    counts.append((int(count[1]), float(count[2])))
                ok = re.fullmatch(r'OK(?: \(skipped=([0-9]+)\))?', line)
                failed = re.fullmatch(r'FAILED \(([a-z]+=[0-9]+(?:, [a-z]+=[0-9]+)*)\)', line)
                if ok:
                    outcomes.append({'status': 'passed', 'failures': 0, 'errors': 0, 'skipped': int(ok[1] or 0)})
                if failed:
                    values = dict(item.split('=') for item in failed[1].split(', '))
                    require(set(values) <= {'failures','errors','skipped','expected failures','unexpected successes'},
                            'unittest-log-result-shape')
                    outcomes.append({'status': 'failed', **{key:int(values.get(key, 0)) for key in ('failures','errors','skipped')}})
                if current is None:
                    continue
                frame = re.fullmatch(r'  File "([^"]+)", line ([0-9]+), in ([A-Za-z_][A-Za-z_0-9]*|<module>)', line)
                if frame:
                    file = Path(frame[1])
                    if (file.name in source_lines and file.resolve().parent == HERE and
                            0 < int(frame[2]) <= source_lines[file.name] and len(current['frames']) < 32):
                        current['frames'].append({'file':file.name, 'line':int(frame[2])})
                exception = re.fullmatch(r'([A-Za-z_][A-Za-z_0-9.]*)(?:: (.*))?', line)
                if exception and exception[1] in exception_types:
                    current['exceptionType'] = exception[1]
                    if exception[1] == 'support.Failure' and exception[2] in codes:
                        current['failureCode'] = exception[2]
                    number = re.match(r'\[Errno ([0-9]+)\]', exception[2] or '')
                    if number:
                        current['errno'] = int(number[1])
            final = os.fstat(stream.fileno())
            require(total == info.st_size == final.st_size and info.st_mtime_ns == final.st_mtime_ns,
                    'unittest-log-changed')
        require(len(counts) == len(outcomes) == 1, 'unittest-log-result-missing')
        outcome = outcomes[0]
        require(sum(row['outcome']=='ERROR' for row in failures)==outcome['errors'] and
                sum(row['outcome']=='FAIL' for row in failures)==outcome['failures'], 'unittest-log-case-count')
        return {**result, **outcome, 'complete': True, 'total':counts[0][0], 'seconds':counts[0][1],
                'bytes':total, 'sha256':checksum.hexdigest(), 'cases':failures}
    except Exception as error:
        return {**result, 'errorType':type(error).__name__}
