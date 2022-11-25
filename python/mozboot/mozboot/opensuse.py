# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import subprocess

import distro
from mozboot.base import MERCURIAL_INSTALL_PROMPT, BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class OpenSUSEBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    """openSUSE experimental bootstrapper."""

    BROWSER_PACKAGES = [
        "alsa-devel",
        "gcc-c++",
        "gtk3-devel",
        "dbus-1-glib-devel",
        "glibc-devel-static",
        "libstdc++-devel",
        "libXt-devel",
        "libproxy-devel",
        "clang-devel",
        "patterns-gnome-devel_gnome",
    ]

    OPTIONAL_BROWSER_PACKAGES = [
        "gconf2-devel",  # https://bugzilla.mozilla.org/show_bug.cgi?id=1779931
    ]

    BROWSER_GROUP_PACKAGES = ["devel_C_C++", "devel_gnome"]

    def __init__(self, version, dist_id, **kwargs):
        print("Using an experimental bootstrapper for openSUSE.")
        BaseBootstrapper.__init__(self, **kwargs)

    def install_packages(self, packages):
        ALTERNATIVE_NAMES = {
            "libxml2": "libxml2-2",
        }
        # watchman is not available
        packages = [ALTERNATIVE_NAMES.get(p, p) for p in packages if p != "watchman"]
        self.zypper_install(*packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        packages_to_install = self.BROWSER_PACKAGES.copy()

        for package in self.OPTIONAL_BROWSER_PACKAGES:
            if self.zypper_can_install(package):
                packages_to_install.append(package)
            else:
                print(
                    f"WARNING! zypper cannot find a package for '{package}' for "
                    f"{distro.name(True)}. It will not be automatically installed."
                )

        self.zypper_install(*packages_to_install)

    def install_browser_group_packages(self):
        self.ensure_browser_group_packages()

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def ensure_clang_static_analysis_package(self):
        from mozboot import static_analysis

        self.install_toolchain_static_analysis(static_analysis.LINUX_CLANG_TIDY)

    def ensure_browser_group_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.zypper_patterninstall(*self.BROWSER_GROUP_PACKAGES)

    def _update_package_manager(self):
        self.zypper_update()

    def upgrade_mercurial(self, current):
        """Install Mercurial from pip because system packages could lag."""
        if self.no_interactive:
            # Install via zypper in non-interactive mode because it is the more
            # conservative option and less likely to make people upset.
            self.zypper_install("mercurial")
            return

        res = self.prompt_int(MERCURIAL_INSTALL_PROMPT, 1, 3)

        # zypper.
        if res == 2:
            self.zypper_install("mercurial")
            return False

        # No Mercurial.
        if res == 3:
            print("Not installing Mercurial.")
            return False

        # pip.
        assert res == 1
        self.run_as_root(["pip3", "install", "--upgrade", "Mercurial"])

    def zypper(self, *args):
        if self.no_interactive:
            command = ["zypper", "-n", *args]
        else:
            command = ["zypper", *args]

        self.run_as_root(command)

    def zypper_install(self, *packages):
        self.zypper("install", *packages)

    def zypper_can_install(self, package):
        return (
            subprocess.call(["zypper", "search", package], stdout=subprocess.DEVNULL)
            == 0
        )

    def zypper_update(self, *packages):
        self.zypper("update", *packages)

    def zypper_patterninstall(self, *packages):
        self.zypper("install", "-t", "pattern", *packages)
