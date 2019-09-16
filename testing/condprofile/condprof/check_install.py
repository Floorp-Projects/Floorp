# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
""" Installs dependencies at runtime to simplify deployment.
"""
import sys

PY3 = sys.version_info.major == 3


def install_reqs():
    try:
        import yaml  # NOQA
    except Exception:
        import subprocess
        import sys
        import os

        root = os.path.join(os.path.dirname(__file__), "..")
        if not os.path.exists(os.path.join(root, "mozfile")):
            req_file = PY3 and "local-requirements.txt" or "local-py2-requirements.txt"
        else:
            req_file = PY3 and "requirements.txt" or "py2-requirements.txt"

        # We are forcing --index-url and --isolated here, so pip can grab stuff
        # on python.org.
        # Options to be removed once Bug 1577815 is resolved
        subprocess.check_call(
            [
                sys.executable,
                "-m",
                "pip",
                "--isolated",
                "install",
                "--index-url",
                "https://pypi.python.org/simple",
                "-r",
                req_file,
            ],
            cwd=root,
        )

        # This restarts the program itself with the same option
        # which allows us to reload new modules.
        os.execl(sys.executable, sys.executable, *sys.argv)
        sys.exit()


install_reqs()

# checking the install went fine
import yaml  # NOQA
