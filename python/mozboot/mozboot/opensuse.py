# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class OpenSUSEBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    """openSUSE experimental bootstrapper."""

    SYSTEM_PACKAGES = [
        "libcurl-devel",
        "libpulse-devel",
        "rpmconf",
        "which",
    ]

    BROWSER_PACKAGES = [
        "alsa-devel",
        "gcc-c++",
        "gtk3-devel",
        "dbus-1-glib-devel",
        "gconf2-devel",
        "glibc-devel-static",
        "libstdc++-devel",
        "libXt-devel",
        "libproxy-devel",
        "libuuid-devel",
        "clang-devel",
        "patterns-gnome-devel_gnome",
    ]

    BROWSER_GROUP_PACKAGES = ["devel_C_C++", "devel_gnome"]

    MOBILE_ANDROID_COMMON_PACKAGES = ["java-1_8_0-openjdk"]

    def __init__(self, version, dist_id, **kwargs):
        print("Using an experimental bootstrapper for openSUSE.")
        BaseBootstrapper.__init__(self, **kwargs)

    def install_system_packages(self):
        self.zypper_install(*self.SYSTEM_PACKAGES)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.zypper_install(*self.BROWSER_PACKAGES)

    def install_browser_group_packages(self):
        self.ensure_browser_group_packages()

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def install_mercurial(self):
        self(["pip", "install", "--upgrade", "pip", "--user"])
        self(["pip", "install", "--upgrade", "Mercurial", "--user"])

    def ensure_clang_static_analysis_package(self):
        from mozboot import static_analysis

        self.install_toolchain_static_analysis(static_analysis.LINUX_CLANG_TIDY)

    def ensure_browser_group_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.zypper_patterninstall(*self.BROWSER_GROUP_PACKAGES)

    def install_mobile_android_packages(self, mozconfig_builder, artifact_mode=False):
        # Multi-part process:
        # 1. System packages.
        # 2. Android SDK. Android NDK only if we are not in artifact mode. Android packages.

        # 1. This is hard to believe, but the Android SDK binaries are 32-bit
        # and that conflicts with 64-bit Arch installations out of the box.  The
        # solution is to add the multilibs repository; unfortunately, this
        # requires manual intervention.
        try:
            self.zypper_install(*self.MOBILE_ANDROID_COMMON_PACKAGES)
        except Exception as e:
            print(
                "Failed to install all packages.  The Android developer "
                "toolchain requires 32 bit binaries be enabled"
            )
            raise e

        # 2. Android pieces.
        super().install_mobile_android_packages(
            mozconfig_builder, artifact_mode=artifact_mode
        )

    def _update_package_manager(self):
        self.zypper_update

    def upgrade_mercurial(self, current):
        self(["pip3", "install", "--upgrade", "pip", "--user"])
        self(["pip3", "install", "--upgrade", "Mercurial", "--user"])

    def zypper_install(self, *packages):
        command = ["zypper", "install"]
        if self.no_interactive:
            command.append("-n")

        command.extend(packages)

        self.run_as_root(command)

    def zypper_update(self, *packages):
        command = ["zypper", "update"]
        if self.no_interactive:
            command.append("-n")

        command.extend(packages)

        self.run_as_root(command)

    def zypper_patterninstall(self, *packages):
        command = ["zypper", "install", "-t", "pattern"]
        if self.no_interactive:
            command.append("-y")

        command.extend(packages)

        self.run_as_root(command)
