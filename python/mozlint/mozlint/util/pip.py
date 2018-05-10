# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import print_function
from __future__ import absolute_import

import subprocess


def _run_pip(*args):
    """
    Helper function that runs pip with subprocess
    """
    try:
        subprocess.check_output(['pip'] + list(args),
                                stderr=subprocess.STDOUT)
        return True
    except subprocess.CalledProcessError as e:
        print(e.output)
        return False


def reinstall_program(REQ_PATH):
    """
    Try to install flake8 at the target version, returns True on success
    otherwise prints the otuput of the pip command and returns False
    """
    if _run_pip('install', '-U',
                '--require-hashes', '-r',
                REQ_PATH):
        return True

    return False
