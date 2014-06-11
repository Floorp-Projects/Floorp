# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper

class OpenBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version):
        BaseBootstrapper.__init__(self)

        self.packages = [
            'mercurial',
            'llvm',
            'autoconf-2.13',
            'yasm',
            'gtk+2',
            'dbus-glib',
            'gstreamer-plugins-base',
            'pulseaudio',
            'gmake',
            'gtar',
            'wget',
            'unzip',
            'zip',
        ]

    def install_system_packages(self):
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(['pkg_add', '-z'] + self.packages)
