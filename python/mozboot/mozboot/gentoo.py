# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class GentooBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

    def install_system_packages(self):
        self.ensure_system_packages()

    def ensure_system_packages(self):
        self.run_as_root(["emerge", "--noreplace", "--quiet", "app-arch/zip"])

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.run_as_root(
            [
                "emerge",
                "--oneshot",
                "--noreplace",
                "--quiet",
                "--newuse",
                "dev-libs/dbus-glib",
                "media-sound/pulseaudio",
                "x11-libs/gtk+:3",
                "x11-libs/libXt",
            ]
        )

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def _update_package_manager(self):
        self.run_as_root(["emerge", "--sync"])

    def upgrade_mercurial(self, current):
        self.run_as_root(["emerge", "--update", "mercurial"])
