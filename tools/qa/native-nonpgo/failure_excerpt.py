# SPDX-License-Identifier: MPL-2.0
"""Bounded diagnostic shapes; arbitrary compiler text is never public evidence."""

import os
from pathlib import Path
import re
import stat


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
        for number, line in enumerate(data.decode('utf-8', errors='replace').splitlines(), 1):
            line = re.sub(r'\x1b\[[0-?]*[ -/]*[@-~]', '', line)
            for value in classify(line):
                records.append({'tailLine': number, **value,
                                'normalizedDiagnostic': normalized_diagnostic(line)})
        selected = records if len(records) <= 16 else records[:8] + records[-8:]
        return {**result, 'available': True, 'fileBytes': info.st_size,
                'tailBytesRead': len(data), 'tailTruncated': bool(start),
                'matchingRecords': len(records), 'recordsTruncated': len(records) > 16,
                'records': selected}
    except (OSError, ValueError) as error:
        return {**result, 'available': False, 'errorType': type(error).__name__}
