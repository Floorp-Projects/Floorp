# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import re
import subprocess
import which

from distutils.version import LooseVersion

def get_hg_path():
    """Obtain the path of the Mercurial client."""

    # We use subprocess in places, which expects a Win32 executable or
    # batch script. On some versions of MozillaBuild, we have "hg.exe",
    # "hg.bat," and "hg" (a Python script). "which" will happily return the
    # Python script, which will cause subprocess to choke. Explicitly favor
    # the Windows version over the plain script.
    try:
        return which.which('hg.exe')
    except which.WhichError:
        try:
            return which.which('hg')
        except which.WhichError as e:
            print(e)

    raise Exception('Unable to obtain Mercurial path. Try running ' +
                    '|mach bootstrap| to ensure your environment is up to ' +
                    'date.')

def get_hg_version(hg):
    """Obtain the version of the Mercurial client."""

    env = os.environ.copy()
    env[b'HGPLAIN'] = b'1'

    info = subprocess.check_output([hg, '--version'], env=env)
    match = re.search('version ([^\+\)]+)', info)
    if not match:
        raise Exception('Unable to identify Mercurial version.')

    return LooseVersion(match.group(1))
