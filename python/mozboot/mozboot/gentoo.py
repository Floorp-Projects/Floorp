# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper

class GentooBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        self.run_as_root(['emerge', '--onlydeps', '--quiet', 'firefox'])

        self.run_as_root(['emerge', '--quiet', 'git', 'mercurial'])

    def _update_package_manager(self):
        self.run_as_root(['emerge', '--sync'])

    def upgrade_mercurial(self):
        self.run_as_root(['emerge', '--update', 'mercurial'])
