#!/usr/bin/env python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


"""
installation script for talos. This script:
- creates a virtualenv in the current directory
- sets up talos in development mode: `python setup.py develop`
- downloads pageloader and packages to talos/page_load_test/pageloader.xpi
"""
from __future__ import absolute_import

import os
import six
import subprocess
import sys

try:
    from subprocess import check_call as call
except ImportError:
    from subprocess import call

# globals
here = os.path.dirname(os.path.abspath(__file__))
VIRTUALENV = "https://raw.github.com/pypa/virtualenv/1.10/virtualenv.py"


def which(binary, path=os.environ["PATH"]):
    dirs = path.split(os.pathsep)
    for dir in dirs:
        if os.path.isfile(os.path.join(dir, path)):
            return os.path.join(dir, path)
        if os.path.isfile(os.path.join(dir, path + ".exe")):
            return os.path.join(dir, path + ".exe")


def main(args=sys.argv[1:]):

    # sanity check
    # ensure setup.py exists
    setup_py = os.path.join(here, "setup.py")
    assert os.path.exists(setup_py), "setup.py not found"

    # create a virtualenv
    virtualenv = which("virtualenv") or which("virtualenv.py")
    if virtualenv:
        call([virtualenv, "--system-site-packages", here])
    else:
        process = subprocess.Popen(
            [sys.executable, "-", "--system-site-packages", here], stdin=subprocess.PIPE
        )
        stdout, stderr = process.communicate(
            input=six.moves.urllib.request.urlopen(VIRTUALENV).read()
        )

    # find the virtualenv's python
    for i in ("bin", "Scripts"):
        bindir = os.path.join(here, i)
        if os.path.exists(bindir):
            break
    else:
        raise AssertionError("virtualenv binary directory not found")
    for i in ("python", "python.exe"):
        virtualenv_python = os.path.join(bindir, i)
        if os.path.exists(virtualenv_python):
            break
    else:
        raise AssertionError("virtualenv python not found")

    # install talos into the virtualenv
    call([os.path.abspath(virtualenv_python), "setup.py", "develop"], cwd=here)


if __name__ == "__main__":
    main()
