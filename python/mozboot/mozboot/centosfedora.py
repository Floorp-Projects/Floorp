# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class CentOSFedoraBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    def __init__(self, distro, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = distro
        self.version = int(version.split(".")[0])
        self.dist_id = dist_id

    def install_packages(self, packages):
        if self.version >= 33 and "perl" in packages:
            packages.append("perl-FindBin")
        # watchman is not available on centos/rocky
        if self.distro in ("centos", "rocky"):
            packages = [p for p in packages if p != "watchman"]
        self.dnf_install(*packages)

    def upgrade_mercurial(self, current):
        if current is None:
            self.dnf_install("mercurial")
        else:
            self.dnf_update("mercurial")
