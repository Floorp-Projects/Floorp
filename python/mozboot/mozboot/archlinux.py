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
        'gconf',
        'gtk2',
        'gtk3',
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

    BROWSER_AUR_PACKAGES = [
        'https://aur.archlinux.org/cgit/aur.git/snapshot/uuid.tar.gz',
    ]

    MOBILE_ANDROID_COMMON_PACKAGES = [
        'zlib',  # mobile/android requires system zlib.
        'jdk7-openjdk', # It would be nice to handle alternative JDKs.  See https://wiki.archlinux.org/index.php/Java.
        'wget',  # For downloading the Android SDK and NDK.
        'multilib/lib32-libstdc++5', # See comment about 32 bit binaries and multilib below.
        'multilib/lib32-ncurses',
        'multilib/lib32-readline',
        'multilib/lib32-zlib',
    ]

    def __init__(self, version, dist_id, **kwargs):
        print 'Using an experimental bootstrapper for Archlinux.'
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

    def ensure_mobile_android_packages(self, artifact_mode=False):
        import android

        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode.
        # 3. Android packages.

        # 1. This is hard to believe, but the Android SDK binaries are 32-bit
        # and that conflicts with 64-bit Arch installations out of the box.  The
        # solution is to add the multilibs repository; unfortunately, this
        # requires manual intervention.
        try:
            self.pacman_install(*self.MOBILE_ANDROID_COMMON_PACKAGES)
        except e:
            print('Failed to install all packages.  The Android developer '
                  'toolchain requires 32 bit binaries be enabled (see '
                  'https://wiki.archlinux.org/index.php/Android).  You may need to '
                  'manually enable the multilib repository following the instructions '
                  'at https://wiki.archlinux.org/index.php/Multilib.')
            raise e

        # 2. The user may have an external Android SDK (in which case we save
        # them a lengthy download), or they may have already completed the
        # download. We unpack to ~/.mozbuild/{android-sdk-linux, android-ndk-r11b}.
        mozbuild_path = os.environ.get('MOZBUILD_STATE_PATH', os.path.expanduser(os.path.join('~', '.mozbuild')))
        self.sdk_path = os.environ.get('ANDROID_SDK_HOME', os.path.join(mozbuild_path, 'android-sdk-linux'))
        self.ndk_path = os.environ.get('ANDROID_NDK_HOME', os.path.join(mozbuild_path, 'android-ndk-r11b'))
        self.sdk_url = 'https://dl.google.com/android/android-sdk_r24.0.1-linux.tgz'
        self.ndk_url = android.android_ndk_url('linux')

        android.ensure_android_sdk_and_ndk(path=mozbuild_path,
                                           sdk_path=self.sdk_path, sdk_url=self.sdk_url,
                                           ndk_path=self.ndk_path, ndk_url=self.ndk_url,
                                           artifact_mode=artifact_mode)
        android_tool = os.path.join(self.sdk_path, 'tools', 'android')
        android.ensure_android_packages(android_tool=android_tool)

    def suggest_mobile_android_mozconfig(self, artifact_mode=False):
        import android
        android.suggest_mozconfig(sdk_path=self.sdk_path,
                                  ndk_path=self.ndk_path,
                                  artifact_mode=artifact_mode)

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
