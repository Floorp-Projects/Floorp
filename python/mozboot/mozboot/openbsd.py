# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper


class OpenBSDBootstrapper(BaseBootstrapper):
    def __init__(self, version, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.packages = [
            "gmake",
            "gtar",
            "rust",
            "wget",
            "unzip",
            "zip",
        ]

        self.browser_packages = [
            "llvm",
            "nasm",
            "gtk+3",
            "dbus-glib",
            "pulseaudio",
        ]

    def install_system_packages(self):
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(["pkg_add", "-z"] + self.packages)

    def install_browser_packages(self, mozconfig_builder):
        self.ensure_browser_packages()

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.ensure_browser_packages(artifact_mode=True)

    def ensure_browser_packages(self, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        # we use -z because there's no other way to say "any autoconf-2.13"
        self.run_as_root(["pkg_add", "-z"] + self.browser_packages)

    def ensure_clang_static_analysis_package(self, state_dir, checkout_root):
        # TODO: we don't ship clang base static analysis for this platform
        pass

    def ensure_stylo_packages(self, state_dir, checkout_root):
        # Clang / llvm already installed as browser package
        self.run_as_root(["pkg_add", "cbindgen"])

    def ensure_nasm_packages(self, state_dir, checkout_root):
        # installed via ensure_browser_packages
        pass

    def ensure_node_packages(self, state_dir, checkout_root):
        self.run_as_root(["pkg_add", "node"])
