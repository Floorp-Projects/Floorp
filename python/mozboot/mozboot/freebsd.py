# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper


class FreeBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, flavor, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)
        self.version = int(version.split('.')[0])
        self.flavor = flavor.lower()

        self.packages = [
            'autoconf213',
            'gmake',
            'mercurial',
            'pkgconf',
            'zip',
        ]

        self.browser_packages = [
            'dbus-glib',
            'gconf2',
            'gtk2',
            'gtk3',
            'libGL',
            'pulseaudio',
            'v4l_compat',
            'yasm',
        ]

        if self.flavor == 'dragonfly':
            self.packages.append('unzip')

        # gcc in base is too old
        if self.flavor == 'freebsd' and self.version < 9:
            self.browser_packages.append('gcc')

    def pkg_install(self, *packages):
        if self.which('pkg'):
            command = ['pkg', 'install']
        else:
            command = ['pkg_add', '-Fr']

        command.extend(packages)
        self.run_as_root(command)

    def install_system_packages(self):
        self.pkg_install(*self.packages)

    def install_browser_packages(self):
        self.pkg_install(*self.browser_packages)

    def upgrade_mercurial(self, current):
        self.pkg_install('mercurial')
