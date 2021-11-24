# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import subprocess
import sys
import tempfile

from buildconfig import topsrcdir
import mozunit
from mach.site import MozSiteMetadata, PythonVirtualenv
import pkg_resources


def test_new_package_appears_in_pkg_resources():
    try:
        # "carrot" was chosen as the package to use because:
        # * It has to be a package that doesn't exist in-scope at the start (so,
        #   all vendored modules included in the test virtualenv aren't usage).
        # * It must be on our internal PyPI mirror.
        # Of the options, "carrot" is a small install that fits these requirements.
        pkg_resources.get_distribution("carrot")
        assert False, "Expected to not find 'carrot' as the initial state of the test"
    except pkg_resources.DistributionNotFound:
        pass

    with tempfile.TemporaryDirectory() as venv_dir:
        subprocess.check_call(
            [
                sys.executable,
                os.path.join(
                    topsrcdir, "third_party", "python", "virtualenv", "virtualenv.py"
                ),
                "--no-download",
                venv_dir,
            ]
        )

        venv = PythonVirtualenv(venv_dir)
        venv.pip_install(["carrot==0.10.7"])
        activate_path = venv.activate_path

        metadata = MozSiteMetadata(None, None, None, None, None, venv.prefix)
        with metadata.update_current_site(venv.python_path):
            exec(open(activate_path).read(), dict(__file__=activate_path))

        assert pkg_resources.get_distribution("carrot").version == "0.10.7"


if __name__ == "__main__":
    mozunit.main()
