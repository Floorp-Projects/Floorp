# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, # You can obtain one at http://mozilla.org/MPL/2.0/.

from __future__ import absolute_import, unicode_literals

from mach.decorators import CommandProvider, Command, CommandArgument
from mozbuild.base import MachCommandBase


@CommandProvider
class PhabricatorCommandProvider(MachCommandBase):
    @Command(
        "install-moz-phab",
        category="misc",
        description="Install patch submission tool.",
    )
    @CommandArgument(
        "--force",
        "-f",
        action="store_true",
        help="Force installation even if already installed.",
    )
    def install_moz_phab(self, force=False):
        import logging
        import shutil
        import subprocess
        import sys

        existing = shutil.which("moz-phab")
        if existing and not force:
            self.log(
                logging.ERROR,
                "already_installed",
                {},
                "moz-phab is already installed in %s." % existing,
            )
            sys.exit(1)

        # pip3 is part of Python since 3.4, however some distros choose to
        # remove core components from languages.  While bootstrap should
        # install pip3 it isn't always possible, so display a nicer error
        # message if pip3 is missing.
        if not shutil.which("pip3"):
            self.log(
                logging.ERROR,
                "pip3_not_installed",
                {},
                "`pip3` is not installed. Try running `mach bootstrap`.",
            )
            sys.exit(1)

        command = ["pip3", "install", "--upgrade", "MozPhab"]

        if (
            sys.platform.startswith("linux")
            or sys.platform.startswith("openbsd")
            or sys.platform.startswith("dragonfly")
            or sys.platform.startswith("freebsd")
        ):
            # On all Linux and BSD distros we default to a user installation.
            command.append("--user")

        elif sys.platform.startswith("darwin"):
            # On MacOS we require brew or ports, which work better without --user.
            pass

        elif sys.platform.startswith("win32") or sys.platform.startswith("msys"):
            # Likewise for Windows we assume a system level install is preferred.
            pass

        else:
            # Unsupported, default to --user.
            self.log(
                logging.WARNING,
                "unsupported_platform",
                {},
                "Unsupported platform (%s), assuming per-user installation."
                % sys.platform,
            )
            command.append("--user")

        self.log(logging.INFO, "run", {}, "Installing moz-phab")
        subprocess.run(command)
