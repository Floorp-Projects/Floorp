# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform

from mozboot.base import BaseBootstrapper

class CentOSBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        kern = platform.uname()

        self.yum_groupinstall(
            'Development Tools',
            'Development Libraries',
            'GNOME Software Development')
        self.yum_install(
            'alsa-lib-devel',
            'autoconf213',
            'curl-devel',
            'dbus-glib-devel',
            'glibc-static',
            'gtk2-devel',
            'libstdc++-static',
            'libXt-devel',
            'mercurial',
            'mesa-libGL-devel',
            'wireless-tools-devel',
            'yasm')

        yasm = 'http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.i686.rpm'
        if 'x86_64' in kern[2]:
            yasm = 'http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.x86_64.rpm'

        self.run_as_root(['rpm', '-ivh', yasm])

    def upgrade_mercurial(self):
        self.yum_update('mercurial')

