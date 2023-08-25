#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import signal
import subprocess
import sys
from distutils.spawn import find_executable

here = os.path.dirname(os.path.realpath(__file__))
topsrcdir = os.path.join(here, os.pardir, os.pardir)


def run_process(cmd):
    proc = subprocess.Popen(cmd)

    # ignore SIGINT, the mozlint subprocess should exit quickly and gracefully
    orig_handler = signal.signal(signal.SIGINT, signal.SIG_IGN)
    proc.wait()
    signal.signal(signal.SIGINT, orig_handler)
    return proc.returncode


def run_mozlint(hooktype, args):
    if isinstance(hooktype, bytes):
        hooktype = hooktype.decode("UTF-8", "replace")

    python = find_executable("python3")
    if not python:
        print("error: Python 3 not detected on your system! Please install it.")
        sys.exit(1)

    cmd = [python, os.path.join(topsrcdir, "mach"), "lint"]

    if "commit" in hooktype:
        # don't prevent commits, just display the lint results
        run_process(cmd + ["--workdir=staged"])
        return False
    elif "push" in hooktype:
        return run_process(cmd + ["--outgoing"] + args)

    print("warning: '{}' is not a valid mozlint hooktype".format(hooktype))
    return False


def hg(ui, repo, **kwargs):
    hooktype = kwargs["hooktype"]
    return run_mozlint(hooktype, kwargs.get("pats", []))


def git():
    hooktype = os.path.basename(__file__)
    if hooktype == "hooks.py":
        hooktype = "pre-push"
    return run_mozlint(hooktype, [])


if __name__ == "__main__":
    sys.exit(git())
