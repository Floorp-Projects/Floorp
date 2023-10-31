# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper


class OpenBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.packages = ["gmake", "gtar", "rust", "unzip"]

        self.browser_packages = [
            "llvm",
            "cbindgen",
            "nasm",
            "node",
            "gtk+3",
            "pulseaudio",
        ]

    def install_system_packages(self):
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(["pkg_add", "-z"] + self.packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(["pkg_add", "-z"] + self.browser_packages)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)
