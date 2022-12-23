# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Installs dependencies at runtime to simplify deployment.

This module tries to make sure we have all dependencies installed on
all our environments.
"""
import os
import subprocess
import sys

PY3 = sys.version_info.major == 3
TOPDIR = os.path.join(os.path.dirname(__file__), "..")


def install_reqs():
    """We install requirements one by one, with no cache, and in isolated mode."""
    try:
        import yaml  # NOQA

        return False
    except Exception:
        # we're detecting here that this is running in Taskcluster
        # by checking for the presence of the mozfile directory
        # that was decompressed from target.condprof.tests.tar.gz
        run_in_ci = os.path.exists(os.path.join(TOPDIR, "mozfile"))

        # On Python 2 we only install what's required for condprof.client
        # On Python 3 it's the full thing
        if not run_in_ci:
            req_files = PY3 and ["base.txt", "local.txt"] or ["local-client.txt"]
        else:
            req_files = PY3 and ["base.txt", "ci.txt"] or ["ci-client.txt"]

        for req_file in req_files:
            req_file = os.path.join(TOPDIR, "requirements", req_file)

            with open(req_file) as f:
                reqs = [
                    req
                    for req in f.read().split("\n")
                    if req.strip() != "" and not req.startswith("#")
                ]
                for req in reqs:
                    subprocess.check_call(
                        [
                            sys.executable,
                            "-m",
                            "pip",
                            "install",
                            "--no-cache-dir",
                            "--isolated",
                            "--find-links",
                            "https://pypi.pub.build.mozilla.org/pub/",
                            req,
                        ]
                    )

        return True


def check():
    """Called by the runner.

    The check function will restart the app after
    all deps have been installed.
    """
    if install_reqs():
        os.execl(sys.executable, sys.executable, *sys.argv)
        os._exit(0)
