# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os

from mozboot.base import BaseBootstrapper


class FedoraBootstrapper(BaseBootstrapper):
    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

        self.group_packages = [
            'C Development Tools and Libraries',
        ]

        self.packages = [
            'autoconf213',
            'mercurial',
            'python2-devel',
        ]

        self.browser_group_packages = [
            'GNOME Software Development',
        ]

        self.browser_packages = [
            'alsa-lib-devel',
            'gcc-c++',
            'GConf2-devel',
            'glibc-static',
            'gtk2-devel',  # it's optional in Fedora 20's GNOME Software
                           # Development group.
            'libstdc++-static',
            'libXt-devel',
            'mesa-libGL-devel',
            'pulseaudio-libs-devel',
            'wireless-tools-devel',
            'yasm',
        ]

        self.mobile_android_packages = [
            'ncurses-devel.i686',
            'libstdc++.i686',
            'zlib-devel.i686',
        ]

    def install_system_packages(self):
        self.dnf_groupinstall(*self.group_packages)
        self.dnf_install(*self.packages)

    def install_browser_packages(self):
        self.dnf_groupinstall(*self.browser_group_packages)
        self.dnf_install(*self.browser_packages)

    def install_mobile_android_packages(self):
        import android

        # Install Android specific packages.
        self.dnf_install(*self.mobile_android_packages)

        # Fetch Android SDK and NDK.
        mozbuild_path = os.environ.get('MOZBUILD_STATE_PATH', os.path.expanduser(os.path.join('~', '.mozbuild')))
        self.sdk_path = os.environ.get('ANDROID_SDK_HOME', os.path.join(mozbuild_path, 'android-sdk-linux'))
        self.ndk_path = os.environ.get('ANDROID_NDK_HOME', os.path.join(mozbuild_path, 'android-ndk-r10e'))
        self.sdk_url = 'https://dl.google.com/android/android-sdk_r24.0.1-linux.tgz'
        self.ndk_url = android.android_ndk_url('linux')

        android.ensure_android_sdk_and_ndk(path=mozbuild_path,
                                           sdk_path=self.sdk_path, sdk_url=self.sdk_url,
                                           ndk_path=self.ndk_path, ndk_url=self.ndk_url)

    def suggest_mobile_android_mozconfig(self):
        import android
        android.suggest_mozconfig(sdk_path=self.sdk_path,
                                  ndk_path=self.ndk_path)

    def upgrade_mercurial(self, current):
        self.dnf_update('mercurial')
