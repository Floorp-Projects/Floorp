# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import platform


from mozboot.base import BaseBootstrapper


class CentOSBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id):
        BaseBootstrapper.__init__(self)

        self.version = version
        self.dist_id = dist_id

        self.group_packages = [
            'Development Tools',
            'Development Libraries',
            'GNOME Software Development',
        ]

        self.packages = [
            'autoconf213',
            'curl-devel',
            'mercurial',
        ]

        self.browser_group_packages = [
            'GNOME Software Development',
        ]

        self.browser_packages = [
            'alsa-lib-devel',
            'dbus-glib-devel',
            'glibc-static',
            'gstreamer-devel',
            'gstreamer-plugins-base-devel',
            'gtk2-devel',
            'libstdc++-static',
            'libXt-devel',
            'mesa-libGL-devel',
            'pulseaudio-libs-devel',
            'wireless-tools-devel',
            'yasm',
        ]

    def install_system_packages(self):
        self.yum_groupinstall(*self.group_packages)
        self.yum_install(*self.packages)

    def install_browser_packages(self):
        self.yum_groupinstall(*self.browser_group_packages)
        self.yum_install(*self.browser_packages)

        kern = platform.uname()
        yasm = 'http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.i686.rpm'
        if 'x86_64' in kern[2]:
            yasm = 'http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.x86_64.rpm'

        self.run_as_root(['rpm', '-ivh', yasm])

    def upgrade_mercurial(self, current):
        self.yum_update('mercurial')
