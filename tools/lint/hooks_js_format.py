#!/usr/bin/env python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import subprocess
from subprocess import check_output, CalledProcessError
import sys

here = os.path.dirname(os.path.realpath(__file__))
topsrcdir = os.path.join(here, os.pardir, os.pardir)

EXTRA_PATHS = ("python/mozversioncontrol", "python/mozbuild", "testing/mozbase/mozfile",)
sys.path[:0] = [os.path.join(topsrcdir, p) for p in EXTRA_PATHS]

from mozversioncontrol import get_repository_object, InvalidRepoPath


def run_js_format(hooktype, changedFiles):
    try:
        vcs = get_repository_object(topsrcdir)
    except InvalidRepoPath:
        return

    if not changedFiles:
        # No files have been touched
        return

    extensions = (".js", ".jsx", ".jsm")
    path_list = []
    for filename in sorted(changedFiles):
        # Ignore files unsupported in eslint and prettier
        if filename.decode().endswith(extensions):
            path_list.append(filename)

    if not path_list:
        # No files have been touched
        return

    arguments = ["eslint", "--fix"] + path_list
    # On windows we need this to call the command in a shell, see Bug 1511594
    if os.name == "nt":
        js_format_cmd = ["sh", "mach"] + arguments
    else:
        js_format_cmd = [os.path.join(topsrcdir, "mach")] + arguments
    if "commit" in hooktype:
        # don't prevent commits, just display the eslint and prettier results
        subprocess.call(js_format_cmd)

        vcs.add_remove_files(*path_list)

        return False
    print("warning: '{}' is not a valid js-format hooktype".format(hooktype))
    return False


def git():
    hooktype = os.path.basename(__file__)
    if hooktype == "hooks_js_format.py":
        hooktype = "pre-push"

    try:
        changedFiles = check_output(
            ["git", "diff", "--staged", "--diff-filter=d", "--name-only", "HEAD"]
        ).split()
        # TODO we should detect if we are in a "add -p" mode and show a warning
        return run_js_format(hooktype, changedFiles)

    except CalledProcessError:
        print("Command to retrieve local files failed")
        return 1


if __name__ == "__main__":
    sys.exit(git())
