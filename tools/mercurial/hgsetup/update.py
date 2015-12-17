# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this,
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import unicode_literals

import os

from mozversioncontrol import get_hg_path
from mozversioncontrol.repoupdate import update_mercurial_repo

from .config import (
    HOST_FINGERPRINTS,
)

FINISHED = '''
Your Mercurial recommended extensions are now up to date!
'''.lstrip()


class MercurialUpdater(object):

    def __init__(self, state_dir):
        self.state_dir = os.path.normpath(state_dir)
        self.vcs_tools_dir = os.path.join(self.state_dir, 'version-control-tools')
        self.hgwatchman_dir = os.path.join(self.state_dir, 'hgwatchman')

    def update_all(self):
        hg = get_hg_path()

        repo_existed = os.path.isdir(self.vcs_tools_dir)
        self.update_mercurial_repo(
            hg,
            'https://hg.mozilla.org/hgcustom/version-control-tools',
            self.vcs_tools_dir,
            'default',
            'Ensuring version-control-tools is up to date...')
        self.update_mercurial_repo(
            hg,
            'https://bitbucket.org/facebook/hgwatchman',
            self.hgwatchman_dir,
            'default',
            'Ensuring hgwatchman is up to date...')
        if repo_existed:
            print(FINISHED)
        return 0

    def update_mercurial_repo(self, hg, url, dest, branch, msg):
        # Disable common extensions whose older versions may cause `hg`
        # invocations to abort.
        disable_exts = [
            'bzexport',
            'bzpost',
            'firefoxtree',
            'hgwatchman',
            'mqext',
            'qimportbz',
            'push-to-try',
            'reviewboard',
        ]
        global_args = []
        for ext in disable_exts:
            global_args.extend(['--config', 'extensions.%s=!' % ext])

        # We always pass the host fingerprints that we "know" to be canonical
        # because the existing config may have outdated fingerprints and this
        # may cause Mercurial to abort.
        return self._update_repo(hg, url, dest, branch, msg,
                                 update_mercurial_repo,
                                 hostfingerprints=HOST_FINGERPRINTS,
                                 global_args=global_args)

    def _update_repo(self, binary, url, dest, branch, msg, fn, *args, **kwargs):
        print('=' * 80)
        print(msg)
        try:
            fn(binary, url, dest, branch, *args, **kwargs)
        finally:
            print('=' * 80)
            print('')
