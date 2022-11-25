# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class CentOSFedoraBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    def __init__(self, distro, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = distro
        self.version = int(version.split(".")[0])
        self.dist_id = dist_id

        self.browser_group_packages = ["GNOME Software Development"]

        self.browser_packages = [
            "alsa-lib-devel",
            "dbus-glib-devel",
            "glibc-static",
            # Development group.
            "libstdc++-static",
            "libXt-devel",
            "pulseaudio-libs-devel",
            "gcc-c++",
        ]

        if self.distro in ("centos", "rocky"):
            self.browser_packages += ["gtk3-devel"]

            if self.version != 6:
                self.browser_group_packages = ["Development Tools"]

    def install_packages(self, packages):
        if self.version >= 33 and "perl" in packages:
            packages.append("perl-FindBin")
        # watchman is not available on centos/rocky
        if self.distro in ("centos", "rocky"):
            packages = [p for p in packages if p != "watchman"]
        self.dnf_install(*packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.dnf_groupinstall(*self.browser_group_packages)
        self.dnf_install(*self.browser_packages)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def upgrade_mercurial(self, current):
        if current is None:
            self.dnf_install("mercurial")
        else:
            self.dnf_update("mercurial")
