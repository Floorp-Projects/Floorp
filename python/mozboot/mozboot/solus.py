# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys
import subprocess

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import (
    ClangStaticAnalysisInstall,
    FixStacksInstall,
    LucetcInstall,
    MinidumpStackwalkInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
    WasiSysrootInstall,
)

# NOTE: This script is intended to be run with a vanilla Python install.  We
# have to rely on the standard library instead of Python 2+3 helpers like
# the six module.
if sys.version_info < (3,):
    input = raw_input  # noqa


class SolusBootstrapper(
        ClangStaticAnalysisInstall,
        FixStacksInstall,
        LucetcInstall,
        MinidumpStackwalkInstall,
        NodeInstall,
        SccacheInstall,
        StyloInstall,
        WasiSysrootInstall,
        BaseBootstrapper):
    '''Solus experimental bootstrapper.'''

    SYSTEM_PACKAGES = [
        'autoconf213',
        'nodejs',
        'python',
        'python3',
        'unzip',
        'zip',
    ]
    SYSTEM_COMPONENTS = [
        'system.devel',
    ]

    BROWSER_PACKAGES = [
        'alsa-lib',
        'dbus',
        'libgtk-2',
        'libgtk-3',
        'libevent',
        'libvpx',
        'libxt',
        'nasm',
        'libstartup-notification',
        'gst-plugins-base',
        'gst-plugins-good',
        'pulseaudio',
        'xorg-server-xvfb',
        'yasm',
    ]

    MOBILE_ANDROID_COMMON_PACKAGES = [
        'openjdk-8',
        # For downloading the Android SDK and NDK.
        'wget',
        # See comment about 32 bit binaries and multilib below.
        'ncurses-32bit',
        'readline-32bit',
        'zlib-32bit',
    ]

    def __init__(self, version, dist_id, **kwargs):
        print('Using an experimental bootstrapper for Solus.')
        BaseBootstrapper.__init__(self, **kwargs)

    def install_system_packages(self):
        self.package_install(*self.SYSTEM_PACKAGES)
        self.component_install(*self.SYSTEM_COMPONENTS)

    def install_browser_packages(self):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self):
        self.ensure_mobile_android_packages()

    def install_mobile_android_artifact_mode_packages(self):
        self.ensure_mobile_android_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        self.package_install(*self.BROWSER_PACKAGES)

    def ensure_nasm_packages(self, state_dir, checkout_root):
        # installed via ensure_browser_packages
        pass

    def ensure_mobile_android_packages(self, artifact_mode=False):
        try:
            self.package_install(*self.MOBILE_ANDROID_COMMON_PACKAGES)
        except Exception as e:
            print('Failed to install all packages!')
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
        pass

    def upgrade_mercurial(self, current):
        self.package_install('mercurial')

    def upgrade_python(self, current):
        self.package_install('python2')

    def package_install(self, *packages):
        command = ['eopkg', 'install']
        if self.no_interactive:
            command.append('--yes-all')

        command.extend(packages)

        self.run_as_root(command)

    def component_install(self, *components):
        command = ['eopkg', 'install', '-c']
        if self.no_interactive:
            command.append('--yes-all')

        command.extend(components)

        self.run_as_root(command)

    def run(self, command, env=None):
        subprocess.check_call(command, stdin=sys.stdin, env=env)
