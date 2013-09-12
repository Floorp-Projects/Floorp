# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.debian import DebianBootstrapper


MERCURIAL_PPA = '''
Ubuntu does not provide a modern Mercurial in its package repository. So,
we will install a PPA that does.
'''.strip()

# Ubuntu shares much logic with Debian, so it inherits from it.
class UbuntuBootstrapper(DebianBootstrapper):
    DISTRO_PACKAGES = [
        # This contains add-apt-repository.
        'software-properties-common',
    ]

    def upgrade_mercurial(self, current):
        # Ubuntu releases up through at least 13.04 don't ship a modern
        # Mercurial. So we hook up a PPA that provides one.
        self._add_ppa('mercurial-ppa/releases')
        self._update_package_manager()
        self.apt_install('mercurial')

    def _add_ppa(self, ppa):
        # Detect PPAs that have already been added. Sadly add-apt-repository
        # doesn't do this for us and will import keys, etc every time. This
        # incurs a user prompt and is annoying, so we try to prevent it.
        list_file = ppa.replace('/', '-')
        for source in os.listdir('/etc/apt/sources.list.d'):
            if source.startswith(list_file) and source.endswith('.list'):
                return

        self.run_as_root(['add-apt-repository', 'ppa:%s' % ppa])
