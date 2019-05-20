# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import (
    ClangStaticAnalysisInstall,
    NasmInstall,
    NodeInstall,
    SccacheInstall,
    StyloInstall,
)


MERCURIAL_INSTALL_PROMPT = '''
Mercurial releases a new version every 3 months and your distro's package
may become out of date. This may cause incompatibility with some
Mercurial extensions that rely on new Mercurial features. As a result,
you may not have an optimal version control experience.

To have the best Mercurial experience possible, we recommend installing
Mercurial via the "pip" Python packaging utility. This will likely result
in files being placed in /usr/local/bin and /usr/local/lib.

How would you like to continue?
  1. Install a modern Mercurial via pip (recommended)
  2. Install a legacy Mercurial via apt
  3. Do not install Mercurial
Your choice: '''


class DebianBootstrapper(NasmInstall, NodeInstall, StyloInstall, ClangStaticAnalysisInstall,
                         SccacheInstall, BaseBootstrapper):
    # These are common packages for all Debian-derived distros (such as
    # Ubuntu).
    COMMON_PACKAGES = [
        'autoconf2.13',
        'build-essential',
        'nodejs',
        'python-dev',
        'python-pip',
        'python-setuptools',
        'unzip',
        'uuid',
        'zip',
    ]

    # Subclasses can add packages to this variable to have them installed.
    DISTRO_PACKAGES = []

    # Ubuntu and Debian don't often differ, but they do for npm.
    DEBIAN_PACKAGES = [
        # Comment the npm package until Debian bring it back
        # 'npm'
    ]

    # These are common packages for building Firefox for Desktop
    # (browser) for all Debian-derived distros (such as Ubuntu).
    BROWSER_COMMON_PACKAGES = [
        'libasound2-dev',
        'libcurl4-openssl-dev',
        'libdbus-1-dev',
        'libdbus-glib-1-dev',
        'libgconf2-dev',
        'libgtk-3-dev',
        'libgtk2.0-dev',
        'libpulse-dev',
        'libx11-xcb-dev',
        'libxt-dev',
        'python-dbus',
        'xvfb',
        'yasm',
    ]

    # Subclasses can add packages to this variable to have them installed.
    BROWSER_DISTRO_PACKAGES = []

    # These are common packages for building Firefox for Android
    # (mobile/android) for all Debian-derived distros (such as Ubuntu).
    MOBILE_ANDROID_COMMON_PACKAGES = [
        'openjdk-8-jdk-headless',  # Android's `sdkmanager` requires Java 1.8 exactly.
        'wget',  # For downloading the Android SDK and NDK.
    ]

    # Subclasses can add packages to this variable to have them installed.
    MOBILE_ANDROID_DISTRO_PACKAGES = []

    def __init__(self, distro, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = distro
        self.version = version
        self.dist_id = dist_id

        self.packages = self.COMMON_PACKAGES + self.DISTRO_PACKAGES
        if self.distro == 'Debian' or self.distro == 'debian':
            self.packages += self.DEBIAN_PACKAGES
        self.browser_packages = self.BROWSER_COMMON_PACKAGES + self.BROWSER_DISTRO_PACKAGES
        self.mobile_android_packages = self.MOBILE_ANDROID_COMMON_PACKAGES + \
            self.MOBILE_ANDROID_DISTRO_PACKAGES

    def install_system_packages(self):
        # Python 3 may not be present on all distros. Search for it and
        # install if found.
        packages = list(self.packages)

        have_python3 = any([self.which('python3'), self.which('python3.6'),
                            self.which('python3.5')])

        if not have_python3:
            python3_packages = self.check_output([
                'apt-cache', 'pkgnames', 'python3'])
            python3_packages = python3_packages.splitlines()

            if 'python3' in python3_packages:
                packages.extend(['python3', 'python3-dev'])

        self.apt_install(*packages)

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
        self.apt_install(*self.browser_packages)
        modern = self.is_nasm_modern()
        if not modern:
            self.apt_install('nasm')

    def ensure_mobile_android_packages(self, artifact_mode=False):
        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode. Android packages.
        self.apt_install(*self.mobile_android_packages)

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
        self.apt_update()

    def upgrade_mercurial(self, current):
        """Install Mercurial from pip because Debian packages typically lag."""
        if self.no_interactive:
            # Install via Apt in non-interactive mode because it is the more
            # conservative option and less likely to make people upset.
            self.apt_install('mercurial')
            return

        res = self.prompt_int(MERCURIAL_INSTALL_PROMPT, 1, 3)

        # Apt.
        if res == 2:
            self.apt_install('mercurial')
            return False

        # No Mercurial.
        if res == 3:
            print('Not installing Mercurial.')
            return False

        # pip.
        assert res == 1
        self.run_as_root(['pip', 'install', '--upgrade', 'Mercurial'])
