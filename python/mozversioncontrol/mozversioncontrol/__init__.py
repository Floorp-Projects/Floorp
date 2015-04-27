# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import re
import subprocess

from distutils.version import LooseVersion

def get_hg_version(hg):
    """Obtain the version of the Mercurial client."""

    env = os.environ.copy()
    env[b'HGPLAIN'] = b'1'

    info = subprocess.check_output([hg, '--version'], env=env)
    match = re.search('version ([^\+\)]+)', info)
    if not match:
        raise Exception('Unable to identify Mercurial version.')

    return LooseVersion(match.group(1))
