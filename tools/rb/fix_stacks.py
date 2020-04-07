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


def fixSymbols(line, jsonMode=False, slowWarning=False, breakpadSymsDir=None):
    global fix_stacks

    result = line_re.search(line)
    if result is None:
        return line

    if not fix_stacks:
        # Look in MOZ_FETCHES_DIR (for automation), then in MOZBUILD_STATE_PATH
        # (for a local build where the user has that set), then in ~/.mozbuild
        # (for a local build with default settings).
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
        if jsonMode:
            args.append('-j')
        if breakpadSymsDir:
            # `fileid` should be packaged next to `fix_stacks.py`.
            here = os.path.dirname(__file__)
            fileid_exe = os.path.join(here, 'fileid')
            if platform.system() == 'Windows':
                fileid_exe = fileid_exe + '.exe'

            args.append('-b')
            args.append(breakpadSymsDir + "," + fileid_exe)

        fix_stacks = Popen(args, stdin=PIPE, stdout=PIPE, stderr=None)

        if slowWarning:
            print("Initializing stack-fixing for the first stack frame, this may take a while...")

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


if __name__ == "__main__":
    for line in sys.stdin:
        sys.stdout.write(fixSymbols(line))
