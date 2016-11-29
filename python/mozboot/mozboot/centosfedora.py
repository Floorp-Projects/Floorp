# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import platform

from mozboot.base import BaseBootstrapper


class CentOSFedoraBootstrapper(BaseBootstrapper):
    def __init__(self, distro, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = distro
        self.version = version
        self.dist_id = dist_id

        self.group_packages = []

        self.packages = [
            'autoconf213',
            'mercurial',
        ]

        self.browser_group_packages = [
            'GNOME Software Development',
        ]

        self.browser_packages = [
            'alsa-lib-devel',
            'dbus-glib-devel',
            'GConf2-devel',
            'glibc-static',
            'gtk2-devel',  # It is optional in Fedora 20's GNOME Software
                           # Development group.
            'libstdc++-static',
            'libXt-devel',
            'mesa-libGL-devel',
            'pulseaudio-libs-devel',
            'wireless-tools-devel',
            'yasm',
        ]

        self.mobile_android_packages = []

        if self.distro in ('CentOS', 'CentOS Linux'):
            self.group_packages += [
                'Development Tools',
                'Development Libraries',
                'GNOME Software Development',
            ]

            self.packages += [
                'curl-devel',
            ]

            self.browser_packages += [
                'gtk3-devel',
            ]

        elif self.distro == 'Fedora':
            self.group_packages += [
                'C Development Tools and Libraries',
            ]

            self.packages += [
                'python2-devel',
            ]

            self.browser_packages += [
                'gcc-c++',
            ]

            self.mobile_android_packages += [
                'java-1.8.0-openjdk-devel',
                'ncurses-devel.i686',
                'libstdc++.i686',
                'zlib-devel.i686',
            ]

    def install_system_packages(self):
        self.dnf_groupinstall(*self.group_packages)
        self.dnf_install(*self.packages)

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self):
        if self.distro in ('CentOS', 'CentOS Linux'):
            BaseBootstrapper.install_mobile_android_packages(self)
        elif self.distro == 'Fedora':
            self.install_fedora_mobile_android_packages()

    def install_mobile_android_artifact_mode_packages(self):
        if self.distro in ('CentOS', 'CentOS Linux'):
            BaseBootstrapper.install_mobile_android_artifact_mode_packages(self)
        elif self.distro == 'Fedora':
            self.install_fedora_mobile_android_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.dnf_groupinstall(*self.browser_group_packages)
        self.dnf_install(*self.browser_packages)

        if self.distro in ('CentOS', 'CentOS Linux'):
            yasm = 'http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.i686.rpm'
            if platform.architecture()[0] == '64bit':
                yasm = 'http://pkgs.repoforge.org/yasm/yasm-1.1.0-1.el6.rf.x86_64.rpm'

            self.run_as_root(['rpm', '-ivh', yasm])

    def install_fedora_mobile_android_packages(self, artifact_mode=False):
        import android

        # Install Android specific packages.
        self.dnf_install(*self.mobile_android_packages)

        # Fetch Android SDK and NDK.
        mozbuild_path = os.environ.get('MOZBUILD_STATE_PATH', os.path.expanduser(os.path.join('~', '.mozbuild')))
        self.sdk_path = os.environ.get('ANDROID_SDK_HOME', os.path.join(mozbuild_path, 'android-sdk-linux'))
        self.ndk_path = os.environ.get('ANDROID_NDK_HOME', os.path.join(mozbuild_path, 'android-ndk-r11b'))
        self.sdk_url = 'https://dl.google.com/android/android-sdk_r24.0.1-linux.tgz'
        self.ndk_url = android.android_ndk_url('linux')

        android.ensure_android_sdk_and_ndk(path=mozbuild_path,
                                           sdk_path=self.sdk_path, sdk_url=self.sdk_url,
                                           ndk_path=self.ndk_path, ndk_url=self.ndk_url,
                                           artifact_mode=artifact_mode)

        # Most recent version of build-tools appears to be 23.0.1 on Fedora
        packages = [p for p in android.ANDROID_PACKAGES if not p.startswith('build-tools')]
        packages.append('build-tools-23.0.1')

        # 3. We expect the |android| tool to be at
        # ~/.mozbuild/android-sdk-linux/tools/android.
        android_tool = os.path.join(self.sdk_path, 'tools', 'android')
        android.ensure_android_packages(android_tool=android_tool, packages=packages)

    def suggest_mobile_android_mozconfig(self, artifact_mode=False):
        import android
        android.suggest_mozconfig(sdk_path=self.sdk_path,
                                  ndk_path=self.ndk_path,
                                  artifact_mode=artifact_mode)

    def suggest_mobile_android_artifact_mode_mozconfig(self):
        self.suggest_mobile_android_mozconfig(artifact_mode=True)

    def upgrade_mercurial(self, current):
        self.dnf_update('mercurial')
