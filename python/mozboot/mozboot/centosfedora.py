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

        self.group_packages = []

        # For CentOS 7, later versions of nodejs come from nodesource
        # and include the npm package.
        self.packages = ["nodejs", "which"]

        self.browser_group_packages = ["GNOME Software Development"]

        self.browser_packages = [
            "alsa-lib-devel",
            "dbus-glib-devel",
            "glibc-static",
            # Development group.
            "libstdc++-static",
            "libXt-devel",
            "nasm",
            "pulseaudio-libs-devel",
            "gcc-c++",
        ]

        self.mobile_android_packages = []

        if self.distro in ("centos", "rocky"):
            self.group_packages += ["Development Tools"]

            self.packages += ["curl-devel"]

            self.browser_packages += ["gtk3-devel"]

            if self.version == 6:
                self.group_packages += [
                    "Development Libraries",
                    "GNOME Software Development",
                ]

                self.packages += ["npm"]

            else:
                self.packages += ["redhat-rpm-config"]

                self.browser_group_packages = ["Development Tools"]

        elif self.distro == "fedora":
            self.group_packages += ["C Development Tools and Libraries"]

            self.packages += ["npm", "redhat-rpm-config"]
            if self.version >= 33:
                self.packages.append("perl-FindBin")

            self.mobile_android_packages += ["ncurses-compat-libs"]

        self.packages += ["python3-devel"]

    def install_system_packages(self):
        self.dnf_groupinstall(*self.group_packages)
        self.dnf_install(*self.packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.dnf_groupinstall(*self.browser_group_packages)
        self.dnf_install(*self.browser_packages)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def install_mobile_android_packages(self, mozconfig_builder, artifact_mode=False):
        # Install Android specific packages.
        self.dnf_install(*self.mobile_android_packages)

        super().install_mobile_android_packages(
            mozconfig_builder, artifact_mode=artifact_mode
        )

    def upgrade_mercurial(self, current):
        if current is None:
            self.dnf_install("mercurial")
        else:
            self.dnf_update("mercurial")
