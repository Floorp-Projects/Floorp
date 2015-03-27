# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import sys
import tempfile
import subprocess
import glob

from mozboot.base import BaseBootstrapper


class ArchlinuxBootstrapper(BaseBootstrapper):
    '''Archlinux experimental bootstrapper.'''

    SYSTEM_PACKAGES = [
        'autoconf2.13',
        'base-devel',
        'ccache',
        'mercurial',
        'python2',
        'python2-setuptools',
        'unzip',
        'zip',
    ]

    BROWSER_PACKAGES = [
        'alsa-lib',
        'dbus-glib',
        'desktop-file-utils',
        'gtk2',
        'hicolor-icon-theme',
        'hunspell',
        'icu',
        'libevent',
        'libvpx',
        'libxt',
        'mime-types',
        'mozilla-common',
        'nss',
        'sqlite',
        'startup-notification',
        'diffutils',
        'gst-plugins-base-libs',
        'imake',
        'inetutils',
        'libpulse',
        'mercurial',
        'mesa',
        'python2',
        'unzip',
        'xorg-server-xvfb',
        'yasm',
        'zip',
        'gst-libav',
        'gst-plugins-good',
        'networkmanager',
    ]

    AUR_PACKAGES = [
        'https://aur.archlinux.org/packages/uu/uuid/uuid.tar.gz',
    ]

    def __init__(self, version, dist_id):
        print 'Using an experimental bootstrapper for Archlinux.'
        BaseBootstrapper.__init__(self)

    def install_system_packages(self):
        self.pacman_install(*self.SYSTEM_PACKAGES)
        self.aur_install(*self.AUR_PACKAGES)

    def install_browser_packages(self):
        self.pacman_install(*self.BROWSER_PACKAGES)

    def install_mobile_android_packages(self):
        raise NotImplementedError('Bootstrap support for mobile-android is '
                                  'not yet available for Archlinux')

    def _update_package_manager(self):
        self.pacman_update

    def upgrade_mercurial(self, current):
        self.pacman_install('mercurial')

    def upgrade_python(self, current):
        self.pacman_install('python2')

    def pacman_install(self, *packages):
        command = ['pacman', '-S', '--needed']
        command.extend(packages)

        self.run_as_root(command)

    def pacman_update(self):
        command = ['pacman', '-S', '--refresh']

        self.run_as_root(command)

    def run(self, command):
        subprocess.check_call(command, stdin=sys.stdin)

    def download(self, uri):
        command = ['curl', '-L', '-O', uri]
        self.run(command)

    def unpack(self, path, name, ext):
        if ext == 'gz':
            compression = '-z'
        elif ext == 'bz':
            compression == '-j'
        elif exit == 'xz':
            compression == 'x'

        name = os.path.join(path, name) + '.tar.' + ext
        command = ['tar', '-x', compression, '-f', name, '-C', path]
        self.run(command)

    def makepkg(self, name):
        command = ['makepkg', '-s']
        self.run(command)
        pack = glob.glob(name + '*.tar.xz')[0]
        command = ['pacman', '-U', pack]
        self.run_as_root(command)

    def aur_install(self, *packages):
        path = tempfile.mkdtemp()
        print('WARNING! This script requires to install packages from the AUR '
              'This is potentially unsecure so I recommend that you carefully '
              'read each package description and check the sources.'
              'These packages will be built in ' + path + '.')
        choice = raw_input('Do you want to continue? (yes/no) [no]')
        if choice != 'yes':
            sys.exit(1)

        base_dir = os.getcwd()
        os.chdir(path)
        for package in packages:
            name, _, ext = package.split('/')[-1].split('.')
            directory = os.path.join(path, name)
            self.download(package)
            self.unpack(path, name, ext)
            os.chdir(directory)
            self.makepkg(name)

        os.chdir(base_dir)
