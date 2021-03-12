# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper

import sys

MERCURIAL_INSTALL_PROMPT = """
Mercurial releases a new version every 3 months and your distro's package
may become out of date. This may cause incompatibility with some
Mercurial extensions that rely on new Mercurial features. As a result,
you may not have an optimal version control experience.

To have the best Mercurial experience possible, we recommend installing
Mercurial via the "pip" Python packaging utility. This will likely result
in files being placed in /usr/local/bin and /usr/local/lib.

How would you like to continue?
  1. Install a modern Mercurial via pip [default]
  2. Install a legacy Mercurial via apt
  3. Do not install Mercurial
Your choice: """


class DebianBootstrapper(LinuxBootstrapper, BaseBootstrapper):

    # These are common packages for all Debian-derived distros (such as
    # Ubuntu).
    COMMON_PACKAGES = [
        "build-essential",
        "libpython3-dev",
        "nodejs",
        "unzip",
        "uuid",
        "zip",
    ]

    # Ubuntu and Debian don't often differ, but they do for npm.
    DEBIAN_PACKAGES = [
        # Comment the npm package until Debian bring it back
        # 'npm'
    ]

    # These are common packages for building Firefox for Desktop
    # (browser) for all Debian-derived distros (such as Ubuntu).
    BROWSER_COMMON_PACKAGES = [
        "libasound2-dev",
        "libcurl4-openssl-dev",
        "libdbus-1-dev",
        "libdbus-glib-1-dev",
        "libdrm-dev",
        "libgtk-3-dev",
        "libgtk2.0-dev",
        "libpulse-dev",
        "libx11-xcb-dev",
        "libxt-dev",
        "xvfb",
    ]

    # These are common packages for building Firefox for Android
    # (mobile/android) for all Debian-derived distros (such as Ubuntu).
    MOBILE_ANDROID_COMMON_PACKAGES = [
        "openjdk-8-jdk-headless",  # Android's `sdkmanager` requires Java 1.8 exactly.
        "wget",  # For downloading the Android SDK and NDK.
    ]

    def __init__(self, distro, version, dist_id, codename, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = distro
        self.version = version
        self.dist_id = dist_id
        self.codename = codename

        self.packages = list(self.COMMON_PACKAGES)
        if self.distro == "debian":
            self.packages += self.DEBIAN_PACKAGES

    def suggest_install_distutils(self):
        print(
            "HINT: Try installing distutils with "
            "`apt-get install python3-distutils`.",
            file=sys.stderr,
        )

    def suggest_install_pip3(self):
        print(
            "HINT: Try installing pip3 with `apt-get install python3-pip`.",
            file=sys.stderr,
        )

    def install_system_packages(self):
        self.apt_install(*self.packages)

    def install_browser_packages(self, mozconfig_builder):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.ensure_browser_packages(artifact_mode=True)

    def install_mobile_android_packages(self, mozconfig_builder):
        self.ensure_mobile_android_packages(mozconfig_builder)

    def install_mobile_android_artifact_mode_packages(self, mozconfig_builder):
        self.ensure_mobile_android_packages(mozconfig_builder, artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.apt_install(*self.BROWSER_COMMON_PACKAGES)
        modern = self.is_nasm_modern()
        if not modern:
            self.apt_install("nasm")

    def ensure_mobile_android_packages(self, mozconfig_builder, artifact_mode=False):
        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode. Android packages.
        self.apt_install(*self.MOBILE_ANDROID_COMMON_PACKAGES)

        # 2. Android pieces.
        self.ensure_java(mozconfig_builder)
        super().ensure_mobile_android_packages(artifact_mode=artifact_mode)

    def _update_package_manager(self):
        self.apt_update()

    def upgrade_mercurial(self, current):
        """Install Mercurial from pip because Debian packages typically lag."""
        if self.no_interactive:
            # Install via Apt in non-interactive mode because it is the more
            # conservative option and less likely to make people upset.
            self.apt_install("mercurial")
            return

        res = self.prompt_int(MERCURIAL_INSTALL_PROMPT, 1, 3)

        # Apt.
        if res == 2:
            self.apt_install("mercurial")
            return False

        # No Mercurial.
        if res == 3:
            print("Not installing Mercurial.")
            return False

        # pip.
        assert res == 1
        self.run_as_root(["pip3", "install", "--upgrade", "Mercurial"])
