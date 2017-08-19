"""The command line interface implementation"""

import os
import sys

from cram._encoding import b, bytestype, stdoutb
from cram._process import execute

__all__ = ['runcli']

def _prompt(question, answers, auto=None):
    """Write a prompt to stdout and ask for answer in stdin.

    answers should be a string, with each character a single
    answer. An uppercase letter is considered the default answer.

    If an invalid answer is given, this asks again until it gets a
    valid one.

    If auto is set, the question is answered automatically with the
    specified value.
    """
    default = [c for c in answers if c.isupper()]
    while True:
        sys.stdout.write('%s [%s] ' % (question, answers))
        sys.stdout.flush()
        if auto is not None:
            sys.stdout.write(auto + '\n')
            sys.stdout.flush()
            return auto

        answer = sys.stdin.readline().strip().lower()
        if not answer and default:
            return default[0]
        elif answer and answer in answers.lower():
            return answer

def _log(msg=None, verbosemsg=None, verbose=False):
    """Write msg to standard out and flush.

    If verbose is True, write verbosemsg instead.
    """
    if verbose:
        msg = verbosemsg
    if msg:
        if isinstance(msg, bytestype):
            stdoutb.write(msg)
        else: # pragma: nocover
            sys.stdout.write(msg)
        sys.stdout.flush()

def _patch(cmd, diff):
    """Run echo [lines from diff] | cmd -p0"""
    out, retcode = execute([cmd, '-p0'], stdin=b('').join(diff))
    return retcode == 0

def runcli(tests, quiet=False, verbose=False, patchcmd=None, answer=None):
    """Run tests with command line interface input/output.

    tests should be a sequence of 2-tuples containing the following:

        (test path, test function)

    This function yields a new sequence where each test function is wrapped
    with a function that handles CLI input/output.

    If quiet is True, diffs aren't printed. If verbose is True,
    filenames and status information are printed.

    If patchcmd is set, a prompt is written to stdout asking if
    changed output should be merged back into the original test. The
    answer is read from stdin. If 'y', the test is patched using patch
    based on the changed output.
    """
    total, skipped, failed = [0], [0], [0]

    for path, test in tests:
        def testwrapper():
            """Test function that adds CLI output"""
            total[0] += 1
            _log(None, path + b(': '), verbose)

            refout, postout, diff = test()
            if refout is None:
                skipped[0] += 1
                _log('s', 'empty\n', verbose)
                return refout, postout, diff

            abspath = os.path.abspath(path)
            errpath = abspath + b('.err')

            if postout is None:
                skipped[0] += 1
                _log('s', 'skipped\n', verbose)
            elif not diff:
                _log('.', 'passed\n', verbose)
                if os.path.exists(errpath):
                    os.remove(errpath)
            else:
                failed[0] += 1
                _log('!', 'failed\n', verbose)
                if not quiet:
                    _log('\n', None, verbose)

                errfile = open(errpath, 'wb')
                try:
                    for line in postout:
                        errfile.write(line)
                finally:
                    errfile.close()

                if not quiet:
                    origdiff = diff
                    diff = []
                    for line in origdiff:
                        stdoutb.write(line)
                        diff.append(line)

                    if (patchcmd and
                        _prompt('Accept this change?', 'yN', answer) == 'y'):
                        if _patch(patchcmd, diff):
                            _log(None, path + b(': merged output\n'), verbose)
                            os.remove(errpath)
                        else:
                            _log(path + b(': merge failed\n'))

            return refout, postout, diff

        yield (path, testwrapper)

    if total[0] > 0:
        _log('\n', None, verbose)
        _log('# Ran %s tests, %s skipped, %s failed.\n'
             % (total[0], skipped[0], failed[0]))
