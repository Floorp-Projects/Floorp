# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Installs dependencies at runtime to simplify deployment.

This module tries to make sure we have all dependencies installed on
all our environments.
"""
import sys
import os
import subprocess


PY3 = sys.version_info.major == 3
TOPDIR = os.path.join(os.path.dirname(__file__), "..")


def update_pip():
    """ Pip is updated using 19.2.3 version, along with setuptools.

    See Bug 1585554 and Bug 1585038 for why we have to pick them at pythonhosted.org
    Hopefully we will be able to get rid of this once we have those packages
    everywhere in our infra.
    """
    import pip

    if pip.__version__ != "19.2.3":
        subprocess.check_call(
            [
                sys.executable,
                "-m",
                "pip",
                "install",
                "--no-cache-dir",
                "--index-url",
                "https://pypi.python.org/simple",
                "--upgrade",
                (
                    "https://files.pythonhosted.org/packages/30/db/"
                    "9e38760b32e3e7f40cce46dd5fb107b8c73840df38f0046"
                    "d8e6514e675a1/pip-19.2.3-py2.py3-none-any.whl"
                ),
                (
                    "https://files.pythonhosted.org/packages/b2/"
                    "86/095d2f7829badc207c893dd4ac767e871f6cd547145df"
                    "797ea26baea4e2e/setuptools-41.2.0-py2.py3-none-any.whl"
                ),
            ]
        )

        return True
    return False


def install_reqs():
    """ We install requirements one by one, with no cache, and in isolated mode.
    """
    try:
        import yaml  # NOQA

        return False
    except Exception:
        if not os.path.exists(os.path.join(TOPDIR, "mozfile")):
            req_file = PY3 and "local-requirements.txt" or "local-py2-requirements.txt"
        else:
            req_file = PY3 and "requirements.txt" or "py2-requirements.txt"

        req_file = os.path.join(TOPDIR, req_file)

        with open(req_file) as f:
            reqs = [req for req in f.read().split("\n") if req.strip() != ""]
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
                        "https://pypi.pub.build.mozilla.org/pub",
                        req,
                    ]
                )

        return True


def check():
    """ Called by the runner.

    The check function will restart the app after pip has been updated,
    and then after all deps have been installed.
    """
    if update_pip():
        os.execl(sys.executable, sys.executable, *sys.argv)
        os._exit(0)
    if install_reqs():
        os.execl(sys.executable, sys.executable, *sys.argv)
        os._exit(0)
