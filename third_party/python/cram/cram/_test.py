"""Utilities for running individual tests"""

import itertools
import os
import re
import time

from cram._encoding import b, bchr, bytestype, envencode, unicodetype
from cram._diff import esc, glob, regex, unified_diff
from cram._process import PIPE, STDOUT, execute

__all__ = ['test', 'testfile']

_needescape = re.compile(b(r'[\x00-\x09\x0b-\x1f\x7f-\xff]')).search
_escapesub = re.compile(b(r'[\x00-\x09\x0b-\x1f\\\x7f-\xff]')).sub
_escapemap = dict((bchr(i), b(r'\x%02x' % i)) for i in range(256))
_escapemap.update({b('\\'): b('\\\\'), b('\r'): b(r'\r'), b('\t'): b(r'\t')})

def _escape(s):
    """Like the string-escape codec, but doesn't escape quotes"""
    return (_escapesub(lambda m: _escapemap[m.group(0)], s[:-1]) +
            b(' (esc)\n'))

def test(lines, shell='/bin/sh', indent=2, testname=None, env=None,
         cleanenv=True, debug=False):
    r"""Run test lines and return input, output, and diff.

    This returns a 3-tuple containing the following:

        (list of lines in test, same list with actual output, diff)

    diff is a generator that yields the diff between the two lists.

    If a test exits with return code 80, the actual output is set to
    None and diff is set to [].

    Note that the TESTSHELL environment variable is available in the
    test (set to the specified shell). However, the TESTDIR and
    TESTFILE environment variables are not available. To run actual
    test files, see testfile().

    Example usage:

    >>> from cram._encoding import b
    >>> refout, postout, diff = test([b('  $ echo hi\n'),
    ...                               b('  [a-z]{2} (re)\n')])
    >>> refout == [b('  $ echo hi\n'), b('  [a-z]{2} (re)\n')]
    True
    >>> postout == [b('  $ echo hi\n'), b('  hi\n')]
    True
    >>> bool(diff)
    False

    lines may also be a single bytes string:

    >>> refout, postout, diff = test(b('  $ echo hi\n  bye\n'))
    >>> refout == [b('  $ echo hi\n'), b('  bye\n')]
    True
    >>> postout == [b('  $ echo hi\n'), b('  hi\n')]
    True
    >>> bool(diff)
    True
    >>> (b('').join(diff) ==
    ...  b('--- \n+++ \n@@ -1,2 +1,2 @@\n   $ echo hi\n-  bye\n+  hi\n'))
    True

    Note that the b() function is internal to Cram. If you're using Python 2,
    use normal string literals instead. If you're using Python 3, use bytes
    literals.

    :param lines: Test input
    :type lines: bytes or collections.Iterable[bytes]
    :param shell: Shell to run test in
    :type shell: bytes or str or list[bytes] or list[str]
    :param indent: Amount of indentation to use for shell commands
    :type indent: int
    :param testname: Optional test file name (used in diff output)
    :type testname: bytes or None
    :param env: Optional environment variables for the test shell
    :type env: dict or None
    :param cleanenv: Whether or not to sanitize the environment
    :type cleanenv: bool
    :param debug: Whether or not to run in debug mode (don't capture stdout)
    :type debug: bool
    :return: Input, output, and diff iterables
    :rtype: (list[bytes], list[bytes], collections.Iterable[bytes])
    """
    indent = b(' ') * indent
    cmdline = indent + b('$ ')
    conline = indent + b('> ')
    usalt = 'CRAM%s' % time.time()
    salt = b(usalt)

    if env is None:
        env = os.environ.copy()

    if cleanenv:
        for s in ('LANG', 'LC_ALL', 'LANGUAGE'):
            env[s] = 'C'
        env['TZ'] = 'GMT'
        env['CDPATH'] = ''
        env['COLUMNS'] = '80'
        env['GREP_OPTIONS'] = ''

    if isinstance(lines, bytestype):
        lines = lines.splitlines(True)

    if isinstance(shell, (bytestype, unicodetype)):
        shell = [shell]
    env['TESTSHELL'] = shell[0]

    if debug:
        stdin = []
        for line in lines:
            if not line.endswith(b('\n')):
                line += b('\n')
            if line.startswith(cmdline):
                stdin.append(line[len(cmdline):])
            elif line.startswith(conline):
                stdin.append(line[len(conline):])

        execute(shell + ['-'], stdin=b('').join(stdin), env=env)
        return ([], [], [])

    after = {}
    refout, postout = [], []
    i = pos = prepos = -1
    stdin = []
    for i, line in enumerate(lines):
        if not line.endswith(b('\n')):
            line += b('\n')
        refout.append(line)
        if line.startswith(cmdline):
            after.setdefault(pos, []).append(line)
            prepos = pos
            pos = i
            stdin.append(b('echo %s %s $?\n' % (usalt, i)))
            stdin.append(line[len(cmdline):])
        elif line.startswith(conline):
            after.setdefault(prepos, []).append(line)
            stdin.append(line[len(conline):])
        elif not line.startswith(indent):
            after.setdefault(pos, []).append(line)
    stdin.append(b('echo %s %s $?\n' % (usalt, i + 1)))

    output, retcode = execute(shell + ['-'], stdin=b('').join(stdin),
                              stdout=PIPE, stderr=STDOUT, env=env)
    if retcode == 80:
        return (refout, None, [])

    pos = -1
    ret = 0
    for i, line in enumerate(output[:-1].splitlines(True)):
        out, cmd = line, None
        if salt in line:
            out, cmd = line.split(salt, 1)

        if out:
            if not out.endswith(b('\n')):
                out += b(' (no-eol)\n')

            if _needescape(out):
                out = _escape(out)
            postout.append(indent + out)

        if cmd:
            ret = int(cmd.split()[1])
            if ret != 0:
                postout.append(indent + b('[%s]\n' % (ret)))
            postout += after.pop(pos, [])
            pos = int(cmd.split()[0])

    postout += after.pop(pos, [])

    if testname:
        diffpath = testname
        errpath = diffpath + b('.err')
    else:
        diffpath = errpath = b('')
    diff = unified_diff(refout, postout, diffpath, errpath,
                        matchers=[esc, glob, regex])
    for firstline in diff:
        return refout, postout, itertools.chain([firstline], diff)
    return refout, postout, []

def testfile(path, shell='/bin/sh', indent=2, env=None, cleanenv=True,
             debug=False, testname=None):
    """Run test at path and return input, output, and diff.

    This returns a 3-tuple containing the following:

        (list of lines in test, same list with actual output, diff)

    diff is a generator that yields the diff between the two lists.

    If a test exits with return code 80, the actual output is set to
    None and diff is set to [].

    Note that the TESTDIR, TESTFILE, and TESTSHELL environment
    variables are available to use in the test.

    :param path: Path to test file
    :type path: bytes or str
    :param shell: Shell to run test in
    :type shell: bytes or str or list[bytes] or list[str]
    :param indent: Amount of indentation to use for shell commands
    :type indent: int
    :param env: Optional environment variables for the test shell
    :type env: dict or None
    :param cleanenv: Whether or not to sanitize the environment
    :type cleanenv: bool
    :param debug: Whether or not to run in debug mode (don't capture stdout)
    :type debug: bool
    :param testname: Optional test file name (used in diff output)
    :type testname: bytes or None
    :return: Input, output, and diff iterables
    :rtype: (list[bytes], list[bytes], collections.Iterable[bytes])
    """
    f = open(path, 'rb')
    try:
        abspath = os.path.abspath(path)
        env = env or os.environ.copy()
        env['TESTDIR'] = envencode(os.path.dirname(abspath))
        env['TESTFILE'] = envencode(os.path.basename(abspath))
        if testname is None: # pragma: nocover
            testname = os.path.basename(abspath)
        return test(f, shell, indent=indent, testname=testname, env=env,
                    cleanenv=cleanenv, debug=debug)
    finally:
        f.close()
