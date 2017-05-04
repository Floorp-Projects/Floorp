#!/usr/bin/python
# This software may be used and distributed according to the terms of the
# GNU General Public License version 2 or any later version.

import os
import sys
sys.path.append(os.path.join(os.path.dirname(__file__), "..", "lint", "eslint"))
from hook_helper import is_lintable, runESLint


def output(message):
    print >> sys.stderr, message


def eslint():
    f = os.popen('git diff --cached --name-only --diff-filter=ACM')

    files = [file for file in f.read().splitlines() if is_lintable(file)]

    if len(files) == 0:
        return True

    print "Running ESLint..."

    return runESLint(output, files)


if __name__ == '__main__':
    if not eslint():
        output("Note: ESLint failed, but the commit will still happen. "
               "Please fix before pushing.")

    # We output successfully as that seems to be a better model. See
    # https://bugzilla.mozilla.org/show_bug.cgi?id=1230300#c4 for more
    # information.
