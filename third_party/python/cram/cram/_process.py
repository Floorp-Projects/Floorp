"""Utilities for running subprocesses"""

import os
import signal
import subprocess
import sys

from cram._encoding import fsdecode

__all__ = ['PIPE', 'STDOUT', 'execute']

PIPE = subprocess.PIPE
STDOUT = subprocess.STDOUT

def _makeresetsigpipe():
    """Make a function to reset SIGPIPE to SIG_DFL (for use in subprocesses).

    Doing subprocess.Popen(..., preexec_fn=makeresetsigpipe()) will prevent
    Python's SIGPIPE handler (SIG_IGN) from being inherited by the
    child process.
    """
    if (sys.platform == 'win32' or
        getattr(signal, 'SIGPIPE', None) is None): # pragma: nocover
        return None
    return lambda: signal.signal(signal.SIGPIPE, signal.SIG_DFL)

def execute(args, stdin=None, stdout=None, stderr=None, cwd=None, env=None):
    """Run a process and return its output and return code.

    stdin may either be None or a string to send to the process.

    stdout may either be None or PIPE. If set to PIPE, the process's output
    is returned as a string.

    stderr may either be None or STDOUT. If stdout is set to PIPE and stderr
    is set to STDOUT, the process's stderr output will be interleaved with
    stdout and returned as a string.

    cwd sets the process's current working directory.

    env can be set to a dictionary to override the process's environment
    variables.

    This function returns a 2-tuple of (output, returncode).
    """
    if sys.platform == 'win32': # pragma: nocover
        args = [fsdecode(arg) for arg in args]

    p = subprocess.Popen(args, stdin=PIPE, stdout=stdout, stderr=stderr,
                         cwd=cwd, env=env, bufsize=-1,
                         preexec_fn=_makeresetsigpipe(),
                         close_fds=os.name == 'posix')
    out, err = p.communicate(stdin)
    return out, p.returncode
