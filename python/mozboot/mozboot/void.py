# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import os
import subprocess
import sys

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper


class VoidBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    def __init__(self, version, dist_id, **kwargs):
        BaseBootstrapper.__init__(self, **kwargs)

        self.distro = "void"
        self.version = version
        self.dist_id = dist_id

    def run_as_root(self, command):
        # VoidLinux doesn't support users sudo'ing most commands by default because of the group
        # configuration.
        if os.geteuid() != 0:
            command = ["su", "root", "-c", " ".join(command)]

        print("Executing as root:", subprocess.list2cmdline(command))

        subprocess.check_call(command, stdin=sys.stdin)

    def xbps_install(self, *packages):
        command = ["xbps-install"]
        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)

    def xbps_update(self):
        command = ["xbps-install", "-Su"]
        if self.no_interactive:
            command.append("-y")

        self.run_as_root(command)

    def install_packages(self, packages):
        self.xbps_install(*packages)

    def _update_package_manager(self):
        self.xbps_update()
