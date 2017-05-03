# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper

STYLO_MOZCONFIG = '''
To enable Stylo in your builds, paste the lines between the chevrons
(>>> and <<<) into your mozconfig file:

<<<
ac_add_options --enable-stylo
>>>
'''

class FreeBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, flavor, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)
        self.version = int(version.split('.')[0])
        self.flavor = flavor.lower()

        self.packages = [
            'autoconf213',
            'cargo',
            'gmake',
            'gtar',
            'mercurial',
            'pkgconf',
            'rust',
            'watchman',
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

        if not self.which('unzip'):
            self.packages.append('unzip')

        # GCC 4.2 or Clang 3.4 in base are too old
        if self.flavor == 'freebsd' and self.version < 11:
            self.browser_packages.append('gcc')

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

    def ensure_stylo_packages(self, state_dir):
        self.pkg_install('llvm39')

    def suggest_browser_mozconfig(self):
        if self.stylo:
            print(STYLO_MOZCONFIG)

    def upgrade_mercurial(self, current):
        self.pkg_install('mercurial')
