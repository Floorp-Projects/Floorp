# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper

class MintBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        self.run_as_root(['apt-get', 'build-dep', 'firefox'])

        self.apt_install(
            'autoconf2.13',
            'mercurial',
            'libasound2-dev',
            'libcurl4-openssl-dev',
            'libgstreamer0.10-dev',
            'libgstreamer-plugins-base0.10-dev',
            'libnotify-dev',
            'libiw-dev',
            'libxt-dev',
            'mesa-common-dev',
            'uuid',
            'yasm')

    def upgrade_mercurial(self, current):
        self.apt_install('mercurial')
