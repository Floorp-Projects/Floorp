# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import sys
import tempfile
import subprocess
import glob

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import (
    ClangStaticAnalysisInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
)

# NOTE: This script is intended to be run with a vanilla Python install.  We
# have to rely on the standard library instead of Python 2+3 helpers like
# the six module.
if sys.version_info < (3,):
    input = raw_input


class ArchlinuxBootstrapper(NodeInstall, StyloInstall, SccacheInstall,
                            ClangStaticAnalysisInstall, BaseBootstrapper):
    '''Archlinux experimental bootstrapper.'''

    SYSTEM_PACKAGES = [
        'autoconf2.13',
        'base-devel',
        'nodejs',
        'python2',
        'python2-setuptools',
        'python',  # This is Python 3 on Arch.
        'unzip',
        'zip',
    ]

    BROWSER_PACKAGES = [
        'alsa-lib',
        'dbus-glib',
        'gtk2',
        'gtk3',
        'libevent',
        'libvpx',
        'libxt',
        'mime-types',
        'nasm',
        'startup-notification',
        'gst-plugins-base-libs',
        'libpulse',
        'xorg-server-xvfb',
        'yasm',
        'gst-libav',
        'gst-plugins-good',
    ]

    BROWSER_AUR_PACKAGES = [
        'https://aur.archlinux.org/cgit/aur.git/snapshot/uuid.tar.gz',
    ]

    MOBILE_ANDROID_COMMON_PACKAGES = [
        # It would be nice to handle alternative JDKs.  See
        # https://wiki.archlinux.org/index.php/Java.
        'jdk8-openjdk',
        # For downloading the Android SDK and NDK.
        'wget',
        # See comment about 32 bit binaries and multilib below.
        'multilib/lib32-ncurses',
        'multilib/lib32-readline',
        'multilib/lib32-zlib',
    ]

    def __init__(self, version, dist_id, **kwargs):
        print('Using an experimental bootstrapper for Archlinux.')
        BaseBootstrapper.__init__(self, **kwargs)

    def install_system_packages(self):
        self.pacman_install(*self.SYSTEM_PACKAGES)

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self):
        self.ensure_mobile_android_packages()

    def install_mobile_android_artifact_mode_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.aur_install(*self.BROWSER_AUR_PACKAGES)
        self.pacman_install(*self.BROWSER_PACKAGES)

    def ensure_nasm_packages(self, state_dir, checkout_root):
        # installed via ensure_browser_packages
        pass

    def ensure_mobile_android_packages(self, artifact_mode=False):
        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode. Android packages.

        # 1. This is hard to believe, but the Android SDK binaries are 32-bit
        # and that conflicts with 64-bit Arch installations out of the box.  The
        # solution is to add the multilibs repository; unfortunately, this
        # requires manual intervention.
        try:
            self.pacman_install(*self.MOBILE_ANDROID_COMMON_PACKAGES)
        except Exception as e:
            print('Failed to install all packages.  The Android developer '
                  'toolchain requires 32 bit binaries be enabled (see '
                  'https://wiki.archlinux.org/index.php/Android).  You may need to '
                  'manually enable the multilib repository following the instructions '
                  'at https://wiki.archlinux.org/index.php/Multilib.')
            raise e

        # 2. Android pieces.
        self.ensure_java()
        from mozboot import android
        android.ensure_android('linux', artifact_mode=artifact_mode,
                               no_interactive=self.no_interactive)

    def suggest_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android
        android.suggest_mozconfig('linux', artifact_mode=artifact_mode)

    def suggest_mobile_android_artifact_mode_mozconfig(self):
        self.suggest_mobile_android_mozconfig(artifact_mode=True)

    def _update_package_manager(self):
        self.pacman_update

    def upgrade_mercurial(self, current):
        self.pacman_install('mercurial')

    def upgrade_python(self, current):
        self.pacman_install('python2')

    def pacman_install(self, *packages):
        command = ['pacman', '-S', '--needed']
        if self.no_interactive:
            command.append('--noconfirm')

        command.extend(packages)

        self.run_as_root(command)

    def pacman_update(self):
        command = ['pacman', '-S', '--refresh']

        self.run_as_root(command)

    def run(self, command, env=None):
        subprocess.check_call(command, stdin=sys.stdin, env=env)

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
        makepkg_env = os.environ.copy()
        makepkg_env['PKGEXT'] = '.pkg.tar.xz'
        self.run(command, env=makepkg_env)
        pack = glob.glob(name + '*.pkg.tar.xz')[0]
        command = ['pacman', '-U']
        if self.no_interactive:
            command.append('--noconfirm')
        command.append(pack)
        self.run_as_root(command)

    def aur_install(self, *packages):
        path = tempfile.mkdtemp()
        if not self.no_interactive:
            print('WARNING! This script requires to install packages from the AUR '
                  'This is potentially unsecure so I recommend that you carefully '
                  'read each package description and check the sources.'
                  'These packages will be built in ' + path + '.')
            choice = input('Do you want to continue? (yes/no) [no]')
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
