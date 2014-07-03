# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os
import subprocess

# The logic here is far from robust. Improvements are welcome.

def update_mercurial_repo(hg, repo, path, revision='default',
    hostfingerprints=None):
    """Ensure a HG repository exists at a path and is up to date."""
    hostfingerprints = hostfingerprints or {}

    args = [hg]

    for host, fingerprint in sorted(hostfingerprints.items()):
        args.extend(['--config', 'hostfingerprints.%s=%s' % (host,
            fingerprint)])

    if os.path.exists(path):
        subprocess.check_call(args + ['pull', repo], cwd=path)
    else:
        subprocess.check_call(args + ['clone', repo, path])

    subprocess.check_call([hg, 'update', '-r', revision], cwd=path)


def update_git_repo(git, repo, path, revision='origin/master'):
    """Ensure a Git repository exists at a path and is up to date."""
    if os.path.exists(path):
        subprocess.check_call([git, 'fetch', '--all'], cwd=path)
    else:
        subprocess.check_call([git, 'clone', repo, path])

    subprocess.check_call([git, 'checkout', revision], cwd=path)
