# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals
import sys

from mozboot.base import BaseBootstrapper
from mozfile import which


class FreeBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, flavor, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)
        self.version = int(version.split(".")[0])
        self.flavor = flavor.lower()

        self.packages = [
            "gmake",
            "gtar",
            "m4",
            "pkgconf",
            "py%d%d-sqlite3" % sys.version_info[0:2],
            "rust",
            "watchman",
            "zip",
        ]

        self.browser_packages = [
            "dbus-glib",
            "libXt",
            "nasm",
            "pulseaudio",
        ]

        if not which("as"):
            self.packages.append("binutils")

        if not which("unzip"):
            self.packages.append("unzip")

    def pkg_install(self, *packages):
        if sys.platform.startswith("netbsd"):
            command = ["pkgin", "install"]
        else:
            command = ["pkg", "install"]
        if self.no_interactive:
            command.append("-y")

        command.extend(packages)
        self.run_as_root(command)

    def install_system_packages(self):
        self.pkg_install(*self.packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        packages = self.browser_packages.copy()
        if sys.platform.startswith("netbsd"):
            packages.extend(["brotli", "gtk3+", "libv4l"])
        else:
            packages.extend(["gtk3", "mesa-dri", "v4l_compat"])
        self.pkg_install(*packages)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def ensure_clang_static_analysis_package(self):
        # TODO: we don't ship clang base static analysis for this platform
        pass

    def ensure_stylo_packages(self):
        # Clang / llvm already installed as browser package
        if sys.platform.startswith("netbsd"):
            self.pkg_install("cbindgen")
        else:
            self.pkg_install("rust-cbindgen")

    def ensure_nasm_packages(self):
        # installed via install_browser_packages
        pass

    def ensure_node_packages(self):
        self.pkg_install("npm")

    def upgrade_mercurial(self, current):
        self.pkg_install("mercurial")
