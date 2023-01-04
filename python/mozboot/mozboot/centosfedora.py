# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import subprocess

from mozfile import which

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
        if self.distro in ("centos", "rocky", "oracle"):
            packages = [p for p in packages if p != "watchman"]
        self.dnf_install(*packages)

    def upgrade_mercurial(self, current):
        if current is None:
            self.dnf_install("mercurial")
        else:
            self.dnf_update("mercurial")

    def dnf_install(self, *packages):
        if which("dnf"):

            def not_installed(package):
                # We could check for "Error: No matching Packages to list", but
                # checking `dnf`s exit code is sufficent.
                # Ideally we'd invoke dnf with '--cacheonly', but there's:
                # https://bugzilla.redhat.com/show_bug.cgi?id=2030255
                is_installed = subprocess.run(
                    ["dnf", "list", "--installed", package],
                    stdout=subprocess.PIPE,
                    stderr=subprocess.STDOUT,
                )
                if is_installed.returncode not in [0, 1]:
                    stdout = is_installed.stdout
                    raise Exception(
                        f'Failed to determine whether package "{package}" is installed: "{stdout}"'
                    )
                return is_installed.returncode != 0

            packages = list(filter(not_installed, packages))
            if len(packages) == 0:
                # avoid sudo prompt (support unattended re-bootstrapping)
                return

            command = ["dnf", "install"]
        else:
            command = ["yum", "install"]

        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)

    def dnf_update(self, *packages):
        if which("dnf"):
            command = ["dnf", "update"]
        else:
            command = ["yum", "update"]

        if self.no_interactive:
            command.append("-y")
        command.extend(packages)

        self.run_as_root(command)
