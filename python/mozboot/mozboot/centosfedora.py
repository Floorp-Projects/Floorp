# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import

import platform

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import (
    ClangStaticAnalysisInstall,
    NasmInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
)


class CentOSFedoraBootstrapper(NasmInstall, NodeInstall, StyloInstall,
                               SccacheInstall, ClangStaticAnalysisInstall,
                               BaseBootstrapper):
    def __init__(self, distro, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = distro
        self.version = version
        self.dist_id = dist_id

        self.group_packages = []

        self.packages = [
            'autoconf213',
            'nodejs',
            'npm',
            'which',
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
            'nasm',
            'pulseaudio-libs-devel',
            'wireless-tools-devel',
            'yasm',
        ]

        self.mobile_android_packages = [
            'java-1.8.0-openjdk-devel',
            # For downloading the Android SDK and NDK.
            'wget',
        ]

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
                'redhat-rpm-config',
            ]

            self.browser_packages += [
                'gcc-c++',
                'python-dbus',
            ]

            self.mobile_android_packages += [
                'ncurses-compat-libs',
            ]

    def install_system_packages(self):
        self.dnf_groupinstall(*self.group_packages)
        self.dnf_install(*self.packages)

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=False)

    def install_mobile_android_artifact_mode_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.dnf_groupinstall(*self.browser_group_packages)
        self.dnf_install(*self.browser_packages)

        if self.distro in ('CentOS', 'CentOS Linux'):
            yasm = ('http://dl.fedoraproject.org/pub/epel/6/i386/'
                    'Packages/y/yasm-1.2.0-1.el6.i686.rpm')
            if platform.architecture()[0] == '64bit':
                yasm = ('http://dl.fedoraproject.org/pub/epel/6/x86_64/'
                        'Packages/y/yasm-1.2.0-1.el6.x86_64.rpm')

            self.run_as_root(['rpm', '-ivh', yasm])

    def ensure_mobile_android_packages(self, artifact_mode=False):
        # Install Android specific packages.
        self.dnf_install(*self.mobile_android_packages)

        self.ensure_java()
        from mozboot import android
        android.ensure_android('linux', artifact_mode=artifact_mode,
                               no_interactive=self.no_interactive)

    def suggest_mobile_android_mozconfig(self, artifact_mode=False):
        from mozboot import android
        android.suggest_mozconfig('linux', artifact_mode=artifact_mode)

    def suggest_mobile_android_artifact_mode_mozconfig(self):
        self.suggest_mobile_android_mozconfig(artifact_mode=True)

    def upgrade_mercurial(self, current):
        self.dnf_update('mercurial')
