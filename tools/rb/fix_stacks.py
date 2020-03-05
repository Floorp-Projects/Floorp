#!/usr/bin/python
# vim:sw=4:ts=4:et:
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script uses `fix-stacks` to post-process the entries produced by
# MozFormatCodeAddress().

from __future__ import absolute_import
from subprocess import Popen, PIPE
import os
import platform
import sys


fix_stacks = None


def fixSymbols(line, jsonMode=False):
    global fix_stacks

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

        fix_stacks = Popen(args, stdin=PIPE, stdout=PIPE, stderr=None)

    fix_stacks.stdin.write(line)
    fix_stacks.stdin.flush()
    return fix_stacks.stdout.readline()


if __name__ == "__main__":
    for line in sys.stdin:
        sys.stdout.write(fixSymbols(line))
