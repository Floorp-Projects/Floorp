# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class GentooBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.version = version
        self.dist_id = dist_id

    def install_packages(self, packages):
        DISAMBIGUATE = {
            "gzip": "app-arch/gzip",
            "tar": "app-arch/tar",
        }
        # watchman is available but requires messing with USEs.
        packages = [DISAMBIGUATE.get(p, p) for p in packages if p != "watchman"]
        if packages:
            self.run_as_root(["emerge", "--noreplace"] + packages)

    def _update_package_manager(self):
        self.run_as_root(["emerge", "--sync"])

    def upgrade_mercurial(self, current):
        self.run_as_root(["emerge", "--update", "mercurial"])
