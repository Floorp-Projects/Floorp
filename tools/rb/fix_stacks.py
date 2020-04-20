#!/usr/bin/python
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses `fix-stacks` to post-process the entries produced by
# MozFormatCodeAddress().

from __future__ import absolute_import, print_function
from subprocess import Popen, PIPE
import os
import platform
import re
import sys

# Matches lines produced by MozFormatCodeAddress(), e.g.
# `#01: ???[tests/example +0x43a0]`.
line_re = re.compile("#\d+: .+\[.+ \+0x[0-9A-Fa-f]+\]")

fix_stacks = None

print_slow_warning = False


# Initialize stack-fixing machinery. Spawns a `fix-stacks` process that will
# persist as long as this Python process does. Returns a pair `(fix, finish)`
# `fix` is the function to pass each fixable line; `finish` is the function to
# call once fixing is finished.
#
# WARNING: on Windows, when this function is called, the spawned `fix-stacks`
# process will inherit any open file descriptors. This can cause problems as
# seen in bug 1626272. Specifically, if file F has a file descriptor open when
# this function is called, `fix-stacks` will inherit that file descriptor, and
# subsequent attempts to access F from Python code can cause errors of the form
# "The process cannot access the file because it is being used by another
# process". Therefore, this function should be called without any file
# descriptors being open.
#
# This appears to be a Windows-only, Python2-only problem. In Python3, file
# descriptors are non-inheritable by default on all platforms, and `Popen` has
# `close_fds=True` as the default, either of which would avoid the problem.
# Unfortunately in Python2 on Windows, file descriptors are inheritable, and
# `close_fds=True` is unusable (as the `subprocess` docs explain: "on Windows,
# you cannot set `close_fds` to true and also redirect the standard handles by
# setting `stdin`, `stdout` or `stderr`", and we need to redirect those
# standard handles). All this is badly broken, but we have to work around it.
def init(json_mode=False, slow_warning=False, breakpad_syms_dir=None):
    global fix_stacks
    global print_slow_warning

    # Look in MOZ_FETCHES_DIR (for automation), then in MOZBUILD_STATE_PATH
    # (for a local build where the user has that set), then in ~/.mozbuild (for
    # a local build with default settings).
    base = os.environ.get(
        'MOZ_FETCHES_DIR',
        os.environ.get(
            'MOZBUILD_STATE_PATH',
            os.path.expanduser('~/.mozbuild')
        )
    )
    fix_stacks_exe = base + '/fix-stacks/fix-stacks'
    if platform.system() == 'Windows':
        fix_stacks_exe = fix_stacks_exe + '.exe'

    if not (os.path.isfile(fix_stacks_exe) and os.access(fix_stacks_exe, os.X_OK)):
        raise Exception('cannot find `fix-stacks`; please run `./mach bootstrap`')

    args = [fix_stacks_exe]
    if json_mode:
        args.append('-j')
    if breakpad_syms_dir:
        # `fileid` should be packaged next to `fix_stacks.py`.
        here = os.path.dirname(__file__)
        fileid_exe = os.path.join(here, 'fileid')
        if platform.system() == 'Windows':
            fileid_exe = fileid_exe + '.exe'

        args.append('-b')
        args.append(breakpad_syms_dir + "," + fileid_exe)

    fix_stacks = Popen(args, stdin=PIPE, stdout=PIPE, stderr=None)
    print_slow_warning = slow_warning

    return (fix, finish)


def fix(line):
    global print_slow_warning

    result = line_re.search(line)
    if result is None:
        return line

    # Note that this warning isn't printed until we hit our first stack frame
    # that requires fixing.
    if print_slow_warning:
        print("Initializing stack-fixing for the first stack frame, this may take a while...")
        print_slow_warning = False

    # Sometimes `line` is lacking a trailing newline. If we pass such a `line`
    # to `fix-stacks` it will wait until it receives a newline, causing this
    # script to hang. So we add a newline if one is missing and then remove it
    # from the output.
    is_missing_newline = not line.endswith('\n')
    if is_missing_newline:
        line = line + "\n"
    fix_stacks.stdin.write(line)
    fix_stacks.stdin.flush()
    out = fix_stacks.stdout.readline()
    if is_missing_newline:
        out = out[:-1]
    return out


# Shut down the `fix-stacks` process.
def finish():
    fix_stacks.terminate()


if __name__ == "__main__":
    init()
    for line in sys.stdin:
        sys.stdout.write(fix(line))
    finish()
