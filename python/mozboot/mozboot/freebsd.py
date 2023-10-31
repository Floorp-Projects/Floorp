# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import sys

from mozfile import which

from mozboot.base import BaseBootstrapper


class FreeBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, flavor, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)
        self.version = int(version.split(".")[0])
        self.flavor = flavor.lower()

        self.packages = [
            "gmake",
            "gtar",
            "m4",
            "npm",
            "pkgconf",
            "py%d%d-sqlite3" % sys.version_info[0:2],
            "rust",
            "watchman",
        ]

        self.browser_packages = [
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
        if not artifact_mode:
            if sys.platform.startswith("netbsd"):
                packages.extend(["brotli", "gtk3+", "libv4l", "cbindgen"])
            else:
                packages.extend(["gtk3", "mesa-dri", "v4l_compat", "rust-cbindgen"])
        self.pkg_install(*packages)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def upgrade_mercurial(self, current):
        self.pkg_install("mercurial")
