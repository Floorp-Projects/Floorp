# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class SolusBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    """Solus experimental bootstrapper."""

    def __init__(self, version, dist_id, **kwargs):
        print("Using an experimental bootstrapper for Solus.")
        BaseBootstrapper.__init__(self, **kwargs)

    def install_packages(self, packages):
        self.package_install(*packages)

    def _update_package_manager(self):
        pass

    def upgrade_mercurial(self, current):
        self.package_install("mercurial")

    def package_install(self, *packages):
        command = ["eopkg", "install"]
        if self.no_interactive:
            command.append("--yes-all")

        command.extend(packages)

        self.run_as_root(command)
