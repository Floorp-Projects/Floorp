# SPDX-License-Identifier: MPL-2.0
"""Bounded diagnostic shapes; arbitrary compiler text is never public evidence."""

import ast
from functools import lru_cache
import os
from pathlib import Path
import re
import stat


CHECKPOINT_SOURCES = tuple(Path(__file__).with_name(name) for name in
    ('checkpoint.py', 'checkpoint_replay.py', 'checkpoint_transfer.py'))
CHECKPOINT_SOURCE_LIMIT = 256 * 1024
CHECKPOINT_FAILURE = re.compile(r'(qa_core|support)\.Failure: (checkpoint-[a-z0-9]+(?:-[a-z0-9]+)*)')


def _source_checkpoint_codes(path):
    """Read one fixed source's literal require codes; never import or execute it."""
    try:
        fd = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(fd, 'rb') as stream:
            before = os.fstat(stream.fileno())
            if not stat.S_ISREG(before.st_mode) or not 0 < before.st_size <= CHECKPOINT_SOURCE_LIMIT:
                return frozenset()
            source = stream.read(CHECKPOINT_SOURCE_LIMIT + 1)
            after = os.fstat(stream.fileno())
        if (len(source) != before.st_size or
                (before.st_dev, before.st_ino, before.st_size, before.st_mtime_ns, before.st_ctime_ns) !=
                (after.st_dev, after.st_ino, after.st_size, after.st_mtime_ns, after.st_ctime_ns)):
            return frozenset()
        codes = set()
        for node in ast.walk(ast.parse(source.decode('utf-8'))):
            if (isinstance(node, ast.Call) and isinstance(node.func, ast.Name) and
                    node.func.id == 'require' and len(node.args) >= 2 and
                    isinstance(node.args[1], ast.Constant) and type(node.args[1].value) is str):
                code = node.args[1].value
                if len(code) <= 96 and re.fullmatch(r'checkpoint-[a-z0-9]+(?:-[a-z0-9]+)*', code):
                    codes.add(code)
        return frozenset(codes) if len(codes) <= 256 else frozenset()
    except (OSError, ValueError, UnicodeError, SyntaxError, RecursionError):
        return frozenset()


@lru_cache(maxsize=1)
def _checkpoint_codes():
    codes = frozenset().union(*(_source_checkpoint_codes(path) for path in CHECKPOINT_SOURCES))
    return codes if len(codes) <= 256 else frozenset()


def _checkpoint_phase(code):
    # These are code categories, not a claim about the last runtime operation.
    if code.startswith('checkpoint-replay-'):
        return 'replay-validation'
    if code.startswith(('checkpoint-transfer-', 'checkpoint-artifact-', 'checkpoint-api-',
                        'checkpoint-run-', 'checkpoint-job-')) or code in (
            'checkpoint-current-job', 'checkpoint-service-integrity', 'checkpoint-retention',
            'checkpoint-producer-job', 'checkpoint-expected-digest', 'checkpoint-download-disk',
            'checkpoint-download-redirect', 'checkpoint-download-protocol',
            'checkpoint-download-bound', 'checkpoint-raw-gzip'):
        return 'transfer-validation'
    if code in ('checkpoint-download-integrity', 'checkpoint-context-mismatch',
                'checkpoint-producer-completion', 'checkpoint-restore-disk',
                'checkpoint-state-not-empty', 'checkpoint-link-reference',
                'checkpoint-link-mismatch', 'checkpoint-link-mode-unavailable',
                'checkpoint-restored-tree', 'checkpoint-actual-config-mismatch'):
        return 'restore-validation'
    if code.startswith('checkpoint-reference-'):
        return 'reference-validation'
    if code.startswith('checkpoint-payload-'):
        return 'payload-validation'
    if code.startswith('checkpoint-manifest-') or code in (
            'checkpoint-schema', 'checkpoint-excluded-state', 'checkpoint-duplicate-row',
            'checkpoint-row', 'checkpoint-root-row', 'checkpoint-duplicate-or-hash', 'checkpoint-file-size'):
        return 'manifest-validation'
    if code in ('checkpoint-deadline', 'checkpoint-stored-cap', 'checkpoint-disk-reserve',
                'checkpoint-short-write', 'checkpoint-logical-cap', 'checkpoint-tree-bound'):
        return 'resource-bound'
    if code.startswith('checkpoint-rust-') or code in (
            'checkpoint-stage-completion', 'checkpoint-required-stage', 'checkpoint-boundary',
            'checkpoint-boundary-output-changed', 'checkpoint-build-observation-unavailable',
            'checkpoint-known-build-survivors', 'checkpoint-owned-survivors'):
        return 'build-boundary'
    return 'checkpoint-validation'


def _checkpoint_failure(line):
    if len(line) > 256:
        return None
    text = line.removesuffix('\n').removesuffix('\r')
    match = CHECKPOINT_FAILURE.fullmatch(text)
    if match is None or match[2] not in _checkpoint_codes():
        return None
    return {'kind': 'checkpoint-failure', 'failureCode': match[2],
            'phase': _checkpoint_phase(match[2]), 'phaseSource': 'fixed-failure-code',
            'exceptionKind': 'failure', 'exceptionType': match[1]+'.Failure', 'errno': None}


PATTERNS = (
    ('termination', 'keyboard-interrupt', r'\bKeyboardInterrupt\b'),
    ('termination', 'interrupted', r'\b(?:interrupted by user|Interrupt: 2|signal SIGINT)\b'),
    ('termination', 'terminated', r'\b(?:Terminated: 15|signal SIGTERM)\b'),
    ('termination', 'killed', r'\b(?:Killed: 9|signal SIGKILL)\b'),
    ('termination', 'segmentation-fault', r'\b(?:Segmentation fault|signal SIGSEGV)\b'),
    ('termination', 'aborted', r'\b(?:Abort trap|signal SIGABRT)\b'),
    ('driver-error', 'frontend-failed', r'\berror: clang frontend command failed\b'),
    ('driver-error', 'unable-to-execute', r'\berror: unable to execute command\b'),
    ('source-error', 'undeclared-identifier', r'\berror: use of undeclared identifier\b'),
    ('source-error', 'unknown-type', r'\berror: unknown type name\b'),
    ('source-error', 'no-matching-function', r'\berror: no matching (?:function|member function)\b'),
    ('source-error', 'missing-member', r'\berror: no member named\b'),
    ('source-error', 'missing-header', r'\bfatal error:.*\bfile not found\b'),
    ('linker-error', 'undefined-symbols', r'\bUndefined symbols\b'),
    ('resource-error', 'disk-full', r'\bNo space left on device\b'),
    ('resource-error', 'memory-exhaustion', r'\b(?:out of memory|cannot allocate memory)\b'),
)

VOCABULARY = frozenset(('error fatal use of undeclared identifier unknown type name no matching '
    'member function file not found undefined symbols space left on device out memory cannot allocate '
    'clang frontend command failed unable to execute segmentation fault signal interrupt interrupted '
    'terminated killed aborted by user keyboardinterrupt expected unqualified declaration definition '
    'incompatible incomplete invalid conversion from to for in is was were a an the and with '
    'argument arguments parameter parameters too many few must be at least return value '
    'lvalue rvalue const static struct class template instantiation ambiguous reference candidate '
    'match does requires but provided non assertion attribute unsupported target linker exit code '
    'thread panic unreachable redefinition conflicting types format string operand operator using '
    'call expression constant after before scope required declared initialize initialization '
    'out line any this that mismatched rustc llvm as or directory such').split())


def normalized_diagnostic(line):
    line = re.sub(r"'(?:[^'\\]|\\.)*'|\"(?:[^\"\\]|\\.)*\"|`[^`]*`", ' REDACTED ', line)
    line = re.sub(r'https?://\S+|(?:/|[A-Za-z]:[\\/])\S*|\b\S+=\S+', ' REDACTED ', line)
    start = re.search(r'\b(?:fatal )?error(?:\[E[0-9]{4}\])?:', line, re.I)
    if start:
        line = line[start.start():]
    tokens = []
    for match in re.finditer(r'[A-Za-z_][A-Za-z_0-9]*|[0-9]+', line):
        word = match[0]
        token = word.lower() if word.lower() in VOCABULARY else word if re.fullmatch(r'E[0-9]{4}', word) else '<redacted>'
        if not tokens or token != '<redacted>' or tokens[-1] != token:
            tokens.append(token)
        if len(tokens) >= 48 or len(' '.join(tokens)) >= 240:
            break
    return ' '.join(tokens)[:240]


def classify(line):
    checkpoint = _checkpoint_failure(line)
    if checkpoint is not None:
        return [checkpoint]
    matches = [{'kind': kind, 'messageClass': label} for kind, label, pattern in PATTERNS
               if re.search(pattern, line)]
    rust = re.search(r'\berror\[(E[0-9]{4})\]:', line)
    if rust:
        matches.append({'kind': 'rust-source-error', 'diagnosticCode': rust[1]})
    if not matches and re.search(r'(?:\berror:|\berror\[|\bError [0-9]+\b)', line, re.I):
        matches.append({'kind': 'unclassified-error-marker'})
    return matches


def excerpt(path):
    result = {'rawContentPublished': False, 'freeTextPublished': False,
              'readLimitBytes': 131072, 'recordLimit': 16}
    try:
        fd = os.open(Path(path), os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(fd, 'rb') as stream:
            info = os.fstat(stream.fileno())
            if not stat.S_ISREG(info.st_mode):
                return {**result, 'available': False, 'errorType': 'non-regular-file'}
            start = max(0, info.st_size - result['readLimitBytes'])
            stream.seek(start)
            data = stream.read(min(info.st_size, result['readLimitBytes']))
        if start:
            data = data.partition(b'\n')[2]
        records = []
        for number, line in enumerate(data.decode('utf-8', errors='replace').splitlines(keepends=True), 1):
            complete = line.endswith('\n')
            line = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', line)
            for value in classify(line):
                if value['kind'] == 'checkpoint-failure' and not complete:
                    continue
                records.append({'tailLine': number, **value,
                                'normalizedDiagnostic': normalized_diagnostic(line)})
        selected = records if len(records) <= 16 else records[:8] + records[-8:]
        return {**result, 'available': True, 'fileBytes': info.st_size,
                'tailBytesRead': len(data), 'tailTruncated': bool(start),
                'matchingRecords': len(records), 'recordsTruncated': len(records) > 16,
                'records': selected}
    except (OSError, ValueError) as error:
        return {**result, 'available': False, 'errorType': type(error).__name__}
