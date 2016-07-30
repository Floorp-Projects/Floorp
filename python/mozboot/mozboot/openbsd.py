# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper


class OpenBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.packages = [
            'mercurial',
            'autoconf-2.13',
            'gmake',
            'gtar',
            'wget',
            'unzip',
            'zip',
        ]

        self.browser_packages = [
            'llvm',
            'yasm',
            'gconf2',
            'gtk+2',
            'gtk+3',
            'dbus-glib',
            'pulseaudio',
        ]

    def install_system_packages(self):
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(['pkg_add', '-z'] + self.packages)

    def install_browser_packages(self):
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(['pkg_add', '-z'] + self.browser_packages)
