# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import
import sys

from mozboot.base import BaseBootstrapper


class FreeBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, flavor, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)
        self.version = int(version.split('.')[0])
        self.flavor = flavor.lower()

        self.packages = [
            'autoconf213',
            'gmake',
            'gtar',
            'npm',
            'pkgconf',
            'py%s%s-sqlite3' % sys.version_info[0:2],
            'python3',
            'rust',
            'watchman',
            'zip',
        ]

        self.browser_packages = [
            'dbus-glib',
            'gconf2',
            'gtk2',
            'gtk3',
            'mesa-dri',  # depends on llvm*
            'pulseaudio',
            'v4l_compat',
            'yasm',
        ]

        if not self.which('as'):
            self.packages.append('binutils')

        if not self.which('unzip'):
            self.packages.append('unzip')

    def pkg_install(self, *packages):
        command = ['pkg', 'install']
        if self.no_interactive:
            command.append('-y')

        command.extend(packages)
        self.run_as_root(command)

    def install_system_packages(self):
        self.pkg_install(*self.packages)

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.pkg_install(*self.browser_packages)

    def ensure_stylo_packages(self, state_dir, checkout_root):
        # Already installed as browser package
        pass

    def upgrade_mercurial(self, current):
        self.pkg_install('mercurial')
