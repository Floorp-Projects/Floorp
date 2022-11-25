# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, print_function, unicode_literals

import sys

from mozboot.base import BaseBootstrapper
from mozboot.linux_common import LinuxBootstrapper

# NOTE: This script is intended to be run with a vanilla Python install.  We
# have to rely on the standard library instead of Python 2+3 helpers like
# the six module.
if sys.version_info < (3,):
    input = raw_input  # noqa


class ArchlinuxBootstrapper(LinuxBootstrapper, BaseBootstrapper):
    """Archlinux experimental bootstrapper."""

    BROWSER_PACKAGES = [
        "alsa-lib",
        "dbus-glib",
        "gtk3",
        "libevent",
        "libvpx",
        "libxt",
        "mime-types",
        "startup-notification",
        "gst-plugins-base-libs",
        "libpulse",
        "xorg-server-xvfb",
        "gst-libav",
        "gst-plugins-good",
    ]

    def __init__(self, version, dist_id, **kwargs):
        print("Using an experimental bootstrapper for Archlinux.", file=sys.stderr)
        BaseBootstrapper.__init__(self, **kwargs)

    def install_packages(self, packages):
        # watchman is not available via pacman
        packages = [p for p in packages if p != "watchman"]
        self.pacman_install(*packages)

    def install_browser_packages(self, mozconfig_builder, artifact_mode=False):
        # TODO: Figure out what not to install for artifact mode
        self.pacman_install(*self.BROWSER_PACKAGES)

    def install_browser_artifact_mode_packages(self, mozconfig_builder):
        self.install_browser_packages(mozconfig_builder, artifact_mode=True)

    def upgrade_mercurial(self, current):
        self.pacman_install("mercurial")

    def pacman_install(self, *packages):
        command = ["pacman", "-S", "--needed"]
        if self.no_interactive:
            command.append("--noconfirm")

        command.extend(packages)

        self.run_as_root(command)
