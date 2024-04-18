# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Smoke test script for Fuzzing

This script can be used to perform simple calls using `jsshell`
or whatever other tools you may add.

The call is done via `taskcluster/kinds/fuzzing/kind.yml` and
files contained in the `target.jsshell.zip` and `target.fuzztest.tests.tar.gz`
build artifacts are downloaded to run things.

Everything included in this directory will be added in
`target.fuzztest.tests.tar.gz` at build time, so you can add more scripts and
tools if you need. They will be located in `$MOZ_FETCHES_DIR` and follow the
same directory structure than the source tree.
"""
import os
import os.path
import shlex
import shutil
import subprocess
import sys


def run_jsshell(command, label=None):
    """Invokes `jsshell` with command.

    This function will use the `JSSHELL` environment variable,
    and fallback to a `js` executable if it finds one
    """
    shell = os.environ.get("JSSHELL")
    if shell is None:
        shell = shutil.which("js")
        if shell is None:
            raise FileNotFoundError(shell)
    else:
        if not os.path.exists(shell) or not os.path.isfile(shell):
            raise FileNotFoundError(shell)

    if label is None:
        label = command
    sys.stdout.write(label)
    cmd = [shell] + shlex.split(command)
    sys.stdout.flush()
    try:
        subprocess.check_call(cmd)
    finally:
        sys.stdout.write("\n")
        sys.stdout.flush()


def smoke_test():
    # first, let's make sure it catches crashes so we don't have false
    # positives.
    try:
        run_jsshell("-e 'crash();'", "Testing for crash\n")
    except subprocess.CalledProcessError:
        pass
    else:
        raise Exception("Could not get the process to crash")

    # now let's proceed with some tests
    run_jsshell("--fuzzing-safe -e 'print(\"PASSED\")'", "Simple Fuzzing...")

    # add more smoke tests here


if __name__ == "__main__":
    # if this calls raises an error, the job will turn red in the CI.
    smoke_test()
