#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
from subprocess import check_output, CalledProcessError
import sys

here = os.path.dirname(os.path.realpath(__file__))
topsrcdir = os.path.join(here, os.pardir, os.pardir)

EXTRA_PATHS = ("python/mozversioncontrol",)
sys.path[:0] = [os.path.join(topsrcdir, p) for p in EXTRA_PATHS]

from mozversioncontrol import get_repository_object, InvalidRepoPath


def run_clang_format(hooktype, changedFiles):
    try:
        vcs = get_repository_object(topsrcdir)
    except InvalidRepoPath:
        return

    if not changedFiles:
        # No files have been touched
        return

    arguments = ["clang-format", "-p"] + changedFiles
    # On windows we need this to call the command in a shell, see Bug 1511594
    if os.name == "nt":
        clang_format_cmd = ["sh", "mach"] + arguments
    else:
        clang_format_cmd = [os.path.join(topsrcdir, "mach")] + arguments
    if "commit" in hooktype:
        # don't prevent commits, just display the clang-format results
        subprocess.call(clang_format_cmd)
        # Add the modified files back to the repo (expect a string)
        vcs.add_remove_files(" ".join(changedFiles))
        return False
    print("warning: '{}' is not a valid clang-format hooktype".format(hooktype))
    return False


def hg(ui, repo, node, **kwargs):
    print(
        "warning: this hook has been deprecated. Please use the hg extension instead.\n"
        "please add 'clang-format = ~/.mozbuild/version-control-tools/hgext/clang-format'"
        " to hgrc\n"
        "Or run 'mach bootstrap'"
    )
    return False


def git():
    hooktype = os.path.basename(__file__)
    if hooktype == "hooks_clang_format.py":
        hooktype = "pre-push"

    try:
        changedFiles = check_output(
            ["git", "diff", "--staged", "--name-only", "HEAD"]
        ).split()
        # TODO we should detect if we are in a "add -p" mode and show a warning
        return run_clang_format(hooktype, changedFiles)

    except CalledProcessError:
        print("Command to retrieve local files failed")
        return 1


if __name__ == "__main__":
    sys.exit(git())
